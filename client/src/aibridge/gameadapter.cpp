#include "gameadapter.hpp"
#include "movement.hpp"
#include "runtime.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "clientmonster.hpp"
#include "clientnpc.hpp"
#include "inputstringboard.hpp"
#include "dbcomid.hpp"
#include <chrono>
#include <limits>
#include <set>

namespace aibridge
{
    namespace
    {
        uint64_t steadyMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        uint64_t wallMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        GridPoint position(const ClientCreature &co)
        {
            const auto [x, y] = co.location();
            return {x, y};
        }
        GridPoint endpoint(const ClientCreature &co) { return {co.currMotion()->endX, co.currMotion()->endY}; }
        MyHero &currentHero(const ProcessRun &scene)
        {
            auto hero = dynamic_cast<MyHero *>(scene.findUID(scene.getMyHeroUID(), false));
            if(!hero){ throw ProtocolError("not_in_game", "Current character no longer exists"); }
            return *hero;
        }
        OperationUpdate failed(std::string code, bool submitted = false)
        {
            return {.outcome=OperationOutcome::Failed, .submitted=submitted,
                .error=OperationError{std::move(code), "移动无法继续", Json::object()}};
        }
        std::optional<std::string> knownText(std::string value)
        {
            if(value.empty() || value.size() > 256){ return {}; }
            return value;
        }
        std::optional<std::string> knownText(std::u8string_view value)
        {
            if(value.empty()){ return {}; }
            return knownText(std::string(reinterpret_cast<const char *>(value.data()),value.size()));
        }
        std::optional<std::string> knownText(const char8_t *value)
        {
            return value ? knownText(std::u8string_view(value)) : std::nullopt;
        }
        // Change detection only; handles use exact context and monotonic object/ground identities.
        struct Fingerprint
        {
            uint64_t value = 14695981039346656037ULL;
            void add(uint64_t v) { value ^= v; value *= 1099511628211ULL; }
            void text(std::string_view s) { add(s.size()); for(unsigned char c: s.substr(0,256)){ add(c); } }
            void point(GridPoint p) { add(p.x); add(p.y); }
        };
    }

    struct AiGameAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        ObservationHandles handles;
        FrozenPages pages;
        Json scope = nullptr;
        uint64_t sequence = 0;
        std::optional<uint64_t> fingerprint;
        std::optional<uint64_t> quickFingerprint;
        bool observationReady = false;
        std::vector<TargetIdentity> issuedTargets;
        std::vector<GroundIdentity> issuedGround;
        std::optional<uint64_t> notifiedFrame;
        bool notificationPending = false;
        std::vector<NearbyEntry> visible;
        std::vector<InventoryEntry> inventory;
        std::vector<std::string> inventoryUnavailable;
        std::optional<std::string> observationError;
        CharacterSnapshot character;
        std::unique_ptr<MoveProgress> move;
        std::string moveId;
        uint64_t moveHero = 0;
        uint64_t initialSendSequence = 0;
        bool sent = false;
        std::vector<GridPoint> route;
        size_t nextStep = 1;
        uint64_t initialServerRevision = 0;

        Impl(Client &c, AiBridgeRuntime &r): client(c), runtime(r) {}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            auto p = client.getProcess<ProcessRun>();
            if(!p || p->aiState().phase != "run" || controller().context().is_null()){
                throw ProtocolError("not_in_game", "No current character scene");
            }
            return *p;
        }
        SnapshotStamp stamp() { return {++sequence, wallMs()}; }

        void readInventory(MyHero &hero, ProcessRun &scene, Fingerprint &change)
        {
            inventory.clear(); inventoryUnavailable.clear();
            const auto add = [&](const SDItem &item, std::string location, std::optional<uint16_t> slot, bool reserved)
            {
                if(!item){ return; }
                if(inventory.size() >= FrozenPages::maxRows || item.count > std::numeric_limits<uint32_t>::max()){
                    throw ProtocolError("unavailable", "Inventory observation exceeds contract limit");
                }
                InventoryEntry row;
                row.itemId = item.itemID; row.seqId = item.seqID; row.count = static_cast<uint32_t>(item.count);
                row.reservedCount = reserved ? row.count : 0;
                row.location = std::move(location); row.slot = slot;
                row.name = knownText(DBCOM_ITEMRECORD(item.itemID).name);
                inventory.push_back(std::move(row));
            };
            if(hero.inventoryKnown()){
                for(const auto &bin: hero.getInvPack().getPackBinList()){ add(bin.item, "inventory", {}, false); }
                add(hero.getInvPack().getGrabbedItem(), "inventory", {}, true);
                const auto trade = dynamic_cast<const DirectTradeBoard *>(scene.getWidget("DirectTradeBoard"));
                for(const auto &item: trade->localItemList()){ add(item, "inventory", {}, true); }
                const auto auction = dynamic_cast<const AuctionRegisterBoard *>(scene.getWidget("AuctionRegisterBoard"));
                add(auction->reservedItem(), "inventory", {}, true);
            }
            else{ inventoryUnavailable.push_back("inventory"); }
            if(hero.beltKnown()){
                for(size_t i=0; i<hero.getBelt().list.size(); ++i){ add(hero.getBelt(i), "belt", uint16_t(i), false); }
            }
            else{ inventoryUnavailable.push_back("belt"); }
            if(hero.wearKnown()){
                for(int slot=WLG_BEGIN; slot<WLG_END; ++slot){ add(hero.getWLDesp().wear.getWLItem(slot), "equipment", uint16_t(slot), false); }
            }
            else{ inventoryUnavailable.push_back("equipment"); }
            std::sort(inventory.begin(), inventory.end(), [](const auto &a, const auto &b){
                return std::tie(a.location,a.slot,a.itemId,a.seqId,a.reservedCount) < std::tie(b.location,b.slot,b.itemId,b.seqId,b.reservedCount);
            });
            std::vector<InventoryEntry> merged;
            for(auto &row: inventory){
                if(!merged.empty() && std::tie(merged.back().location,merged.back().slot,merged.back().itemId,merged.back().seqId)
                    == std::tie(row.location,row.slot,row.itemId,row.seqId)){
                    auto &last=merged.back();
                    if(uint64_t(last.count)+row.count>std::numeric_limits<uint32_t>::max()){
                        throw ProtocolError("unavailable","Reserved inventory exceeds contract limit");
                    }
                    last.count+=row.count; last.reservedCount+=row.reservedCount;
                }
                else{ merged.push_back(std::move(row)); }
            }
            inventory=std::move(merged);
            for(const auto &row: inventory){
                change.add(row.itemId); change.add(row.seqId); change.add(row.count); change.add(row.reservedCount);
                change.text(row.location); change.add(row.slot.value_or(65535));
            }
            for(const auto &field: inventoryUnavailable){ change.text(field); }
        }

        void refresh()
        {
            if(observationReady){ return; }
            observationReady=true;
            auto scene = client.getProcess<ProcessRun>();
            auto current = controller().context();
            if(!scene || scene->aiState().phase != "run" || current.is_null()){
                handles.clear(); pages.clear(); scope = nullptr; fingerprint.reset(); visible.clear(); inventory.clear();
                observationError="not_in_game";
                notificationPending=false;
                return;
            }
            current.erase("control_epoch");
            if(scope != current){ pages.clear(); fingerprint.reset(); scope = current; }
            observationError.reset();
            try{
                auto &hero = currentHero(*scene);
                character = {};
                character.characterUid = hero.UID(); character.mapUid = scene->mapUID();
                character.position = position(hero); character.actionEndpoint = endpoint(hero);
                character.name = knownText(hero.knownName());
                character.mapName = knownText(DBCOM_MAPRECORD(scene->mapID()).name);
                const int dir = hero.currMotion()->direction;
                if(dir >= DIR_BEGIN && dir < DIR_END){ character.direction = dir; }
                if(const auto &health = hero.getSDHealth()){
                    character.hp=health->hp; character.maxHp=health->getMaxHP(); character.mp=health->mp; character.maxMp=health->getMaxMP();
                }
                if(hero.expKnown()){ character.level = hero.getLevel(); }
                if(hero.goldKnown()){ character.gold = hero.getInvPack().getGold(); }
                character.action = hero.aiMotionStopped() ? "idle" : "motion_" + std::to_string(hero.currMotion()->type);
                if(scene->aiDeathKnown()){ character.knownStates.push_back("dead"); }
                else if(const auto dead = hero.dead()){ character.knownStates.push_back(*dead ? "dead" : "alive"); }
                Fingerprint change;
                change.add(hero.incarnation()); change.add(scene->mapUID()); change.point(character.position); change.point(character.actionEndpoint);
                change.add(dir); change.add(hero.currMotion()->type); change.text(character.name.value_or(""));
                change.add(scene->aiDeathKnown());
                for(const auto v: {character.hp, character.maxHp, character.mp, character.maxMp, character.level}){
                    change.add(v.has_value()); if(v){ change.add(*v); }
                }
                change.add(character.gold.has_value()); if(character.gold){ change.add(*character.gold); }
                std::vector<TargetIdentity> targets;
                std::vector<GroundIdentity> ground;
                visible.clear();
                if(scene->getCOList().size()>FrozenPages::maxRows){ throw ProtocolError("unavailable","Object observation budget exceeded"); }
                for(const auto &[uid, co]: scene->getCOList()){
                    if(!co || uid == hero.UID() || !scene->findUID(uid, true) || !hero.inViewRange(co.get())){ continue; }
                    NearbyEntry entry;
                    entry.position = position(*co); entry.target = TargetIdentity{uid, co->incarnation()};
                    if(const auto player = dynamic_cast<const Hero *>(co.get())){ entry.kind="player"; entry.name=knownText(player->knownName()); }
                    else if(const auto monster = dynamic_cast<const ClientMonster *>(co.get())){ entry.kind="monster"; entry.name=knownText(monster->getMR().name); }
                    else if(const auto npc = dynamic_cast<const ClientNPC *>(co.get())){ entry.kind="npc"; entry.name=knownText(npc->knownName()); }
                    else{ continue; }
                    if(const auto &health = co->getSDHealth()){ entry.hp=health->hp; entry.maxHp=health->getMaxHP(); entry.alive=!health->dead(); }
                    targets.push_back(*entry.target); visible.push_back(std::move(entry));
                    if(visible.size() > FrozenPages::maxRows){ throw ProtocolError("unavailable", "Too many visible objects"); }
                }
                const auto [mapUid, width, height] = scene->getMap();
                const int radius = hero.viewRange()-1;
                for(int y=std::max(0,hero.y()-radius); y<std::min(height,hero.y()+radius+1); ++y){
                    for(int x=std::max(0,hero.x()-radius); x<std::min(width,hero.x()+radius+1); ++x){
                        if(!hero.inViewRange(x,y)){ continue; }
                        const auto &items = scene->getGroundItemIDList(x,y);
                        if(items.empty()){ continue; }
                        NearbyEntry entry;
                        entry.kind="ground"; entry.position={x,y}; entry.ground=GroundIdentity{{x,y},scene->aiGroundRevision(x,y)};
                        // Oversized lists remain a controlled query failure, never an adapter exception.
                        if(items.size()>100){ observationError="unavailable"; }
                        else{ entry.groundItemIds=items; }
                        ground.push_back(*entry.ground); visible.push_back(std::move(entry));
                    }
                }
                std::sort(visible.begin(), visible.end(), [](const auto &a, const auto &b){
                    return std::tie(a.kind,a.position,a.target,a.ground) < std::tie(b.kind,b.position,b.target,b.ground);
                });
                handles.sync(controller().context(), targets, ground);
                issuedTargets=std::move(targets); issuedGround=std::move(ground);
                for(const auto &row: visible){
                    change.text(row.kind); change.point(row.position); change.text(row.name.value_or(""));
                    if(row.target){ change.add(row.target->uid); change.add(row.target->incarnation); }
                    if(row.ground){ change.add(row.ground->revision); }
                    change.add(row.hp.has_value()); if(row.hp){ change.add(*row.hp); }
                    change.add(row.maxHp.has_value()); if(row.maxHp){ change.add(*row.maxHp); }
                }
                readInventory(hero,*scene,change);
                if(!fingerprint || *fingerprint != change.value){
                    fingerprint=change.value; notificationPending=true;
                }
                character.stamp=stamp();
            }
            catch(const ProtocolError &error){ observationError=error.code; }
        }

        void observe()
        {
            auto scene=client.getProcess<ProcessRun>();
            auto context=controller().context();
            if(!scene || scene->aiState().phase!="run" || context.is_null()){
                handles.clear(); pages.clear(); issuedTargets.clear(); issuedGround.clear();
                scope=nullptr; quickFingerprint.reset(); observationReady=false; notificationPending=false;
                return;
            }
            context.erase("control_epoch");
            if(scope!=context){
                handles.clear(); pages.clear(); issuedTargets.clear(); issuedGround.clear();
                scope=context; fingerprint.reset(); quickFingerprint.reset(); observationReady=false;
            }
            auto &hero=currentHero(*scene);
            Fingerprint change;
            change.add(hero.incarnation()); change.add(scene->mapUID()); change.point(position(hero)); change.point(endpoint(hero));
            change.add(hero.currMotion()->type); change.add(hero.currMotion()->direction); change.add(scene->aiDeathKnown());
            change.text(hero.knownName()); change.add(hero.getExp()); change.add(hero.getInvPack().getGold());
            change.add(hero.expKnown()); change.add(hero.goldKnown()); change.add(hero.inventoryKnown()); change.add(hero.beltKnown()); change.add(hero.wearKnown());
            if(const auto &health=hero.getSDHealth()){
                change.add(1); change.add(health->hp); change.add(health->getMaxHP()); change.add(health->mp); change.add(health->getMaxMP());
            }
            const auto targetCount=issuedTargets.size(), groundCount=issuedGround.size();
            std::erase_if(issuedTargets,[&](const auto identity){
                const auto co=scene->findUID(identity.uid,true);
                return !co || co->incarnation()!=identity.incarnation || !hero.inViewRange(co);
            });
            std::erase_if(issuedGround,[&](const auto identity){
                const auto p=identity.position;
                return !hero.inViewRange(p.x,p.y) || scene->aiGroundRevision(p.x,p.y)!=identity.revision || scene->getGroundItemIDList(p.x,p.y).empty();
            });
            if(targetCount!=issuedTargets.size() || groundCount!=issuedGround.size()){
                handles.sync(controller().context(),issuedTargets,issuedGround);
                observationReady=false; notificationPending=true;
            }
            if(scene->getCOList().size()<=FrozenPages::maxRows){
                for(const auto &[uid,co]:scene->getCOList()){
                    if(!co || !scene->findUID(uid,true) || !hero.inViewRange(co.get())){ continue; }
                    change.add(co->incarnation()); change.point(position(*co)); change.point(endpoint(*co));
                    if(const auto &health=co->getSDHealth()){ change.add(health->hp); change.add(health->getMaxHP()); }
                    if(const auto player=dynamic_cast<const Hero *>(co.get())){ change.text(player->knownName()); }
                    else if(const auto npc=dynamic_cast<const ClientNPC *>(co.get())){ change.text(npc->knownName()); }
                }
            }
            change.add(scene->getCOList().size()); change.add(scene->aiGroundSequence());
            const auto [mapUid,width,height]=scene->getMap();
            const auto origin=position(hero);
            const int radius=hero.viewRange()-1;
            for(int y=std::max(0,origin.y-radius);y<std::min(height,origin.y+radius+1);++y){
                for(int x=std::max(0,origin.x-radius);x<std::min(width,origin.x+radius+1);++x){
                    if(hero.inViewRange(x,y) && !scene->getGroundItemIDList(x,y).empty()){
                        change.point({x,y}); change.add(scene->aiGroundRevision(x,y));
                    }
                }
            }
            const auto item=[&](const SDItem &i){ change.add(i.itemID); change.add(i.seqID); change.add(i.count); };
            const auto &bins=hero.getInvPack().getPackBinList();
            change.add(bins.size());
            for(size_t n=0;n<std::min(bins.size(),FrozenPages::maxRows);++n){ item(bins[n].item); }
            item(hero.getInvPack().getGrabbedItem());
            const auto trade=dynamic_cast<const DirectTradeBoard *>(scene->getWidget("DirectTradeBoard"));
            const auto reserved=trade->localItemList();
            change.add(reserved.size());
            for(size_t n=0;n<std::min(reserved.size(),FrozenPages::maxRows);++n){ item(reserved[n]); }
            item(dynamic_cast<const AuctionRegisterBoard *>(scene->getWidget("AuctionRegisterBoard"))->reservedItem());
            for(const auto &i:hero.getBelt().list){ item(i); }
            for(int slot=WLG_BEGIN;slot<WLG_END;++slot){ item(hero.getWLDesp().wear.getWLItem(slot)); }
            if(!quickFingerprint || *quickFingerprint!=change.value){ quickFingerprint=change.value; notificationPending=true; }
            if(notificationPending && notifiedFrame!=client.renderSequence()){
                controller().notifySnapshot(); notifiedFrame=client.renderSequence(); notificationPending=false;
            }
        }

        bool submitted()
        {
            if(auto scene=client.getProcess<ProcessRun>()){
                if(auto hero=dynamic_cast<MyHero *>(scene->findUID(scene->getMyHeroUID(),false)); hero && hero->incarnation()==moveHero){
                    sent |= hero->actionSendSequence()!=initialSendSequence;
                }
            }
            return sent;
        }
        bool moveBlocked(const ProcessRun &scene, const MyHero &hero) const
        {
            const auto input=dynamic_cast<const InputStringBoard *>(scene.getWidget("InputStringBoard"));
            const auto trade=dynamic_cast<const DirectTradeBoard *>(scene.getWidget("DirectTradeBoard"));
            const auto auction=dynamic_cast<const AuctionBoard *>(scene.getWidget("AuctionBoard"));
            const auto registration=dynamic_cast<const AuctionRegisterBoard *>(scene.getWidget("AuctionRegisterBoard"));
            return !hero.aiCanStartMove() || input->showModal() || trade->peerUID()
                || auction->pending() || registration->pending()
                || scene.getFocusUID(FOCUS_ATTACK) || scene.getFocusUID(FOCUS_FOLLOW) || scene.getFocusUID(FOCUS_MAGIC);
        }
        ValidationResult validateMove(const Json &params, const Json &context)
        {
            try{
                auto &scene=run(); auto &hero=currentHero(scene);
                if(controller().context()!=context){ throw ProtocolError("stale_context","Character context changed"); }
                if(!scene.aiState().alive){ throw ProtocolError("paused","Character is not known alive"); }
                if(scene.getCOList().size()>FrozenPages::maxRows){ throw ProtocolError("unavailable","Dynamic observation budget exceeded"); }
                const auto x=params.at("x").get<int64_t>(), y=params.at("y").get<int64_t>();
                if(x<0 || y<0 || x>std::numeric_limits<int16_t>::max() || y>std::numeric_limits<int16_t>::max() || !scene.onMap(int(x),int(y))){
                    throw ProtocolError("out_of_bounds","Destination is outside the current action grid");
                }
                if(moveBlocked(scene,hero)){ throw ProtocolError("busy","A player action or modal interaction is active"); }
                return {};
            }
            catch(const ProtocolError &e){ return {.error=OperationError{e.code,"当前角色或交互状态不允许移动",Json::object()}}; }
        }
        MoveObservation observation()
        {
            auto &scene=run(); auto &hero=currentHero(scene);
            MoveObservation value{.stamp=stamp(),.context=controller().context(),.position=position(hero),.actionEndpoint=endpoint(hero),
                .stopped=hero.aiMotionStopped(),.alive=scene.aiState().alive};
            if(const auto &server=scene.aiServerPosition()){ value.lastServerPosition=GridPoint{server->first,server->second}; }
            return value;
        }
        OperationUpdate begin(const OperationView &view)
        {
            auto &scene=run(); auto &hero=currentHero(scene);
            const GridPoint target{view.params.at("x").get<int>(),view.params.at("y").get<int>()};
            const int tolerance=view.params.value("tolerance",0);
            const auto now=steadyMs();
            if(now>=view.deadlineMs){ return failed("timeout"); }
            route.clear(); nextStep=1;
            if(!withinTolerance(endpoint(hero),target,tolerance)){
                try{
                    for(const auto &step:hero.aiPlanMove(target.x,target.y)){ route.push_back({step.X,step.Y}); }
                }
                catch(const ProtocolError &error){ return failed(error.code); }
                if(route.empty()){ return failed("unreachable"); }
            }
            if(steadyMs()>=view.deadlineMs){ return failed("timeout"); }
            if(!runtime.claimIntent(view.operationId,"move")){ return failed("busy"); }
            moveId=view.operationId; moveHero=hero.incarnation(); initialSendSequence=hero.actionSendSequence(); sent=false;
            initialServerRevision=scene.aiServerRevision();
            move=std::make_unique<MoveProgress>(target,tolerance,view.context,position(hero),now,std::min<uint64_t>(30000,view.deadlineMs-now));
            return tick(view).value_or(OperationUpdate{});
        }
        std::optional<OperationUpdate> tick(const OperationView &view)
        {
            if(!move || moveId!=view.operationId){ return failed("stale_context",view.submitted); }
            auto &scene=run(); auto &hero=currentHero(scene);
            if(scene.aiServerRevision()!=initialServerRevision){ route.clear(); }
            const auto observed=observation();
            auto update=move->observe(observed,steadyMs(),submitted());
            if(update.outcome!=OperationOutcome::Running){ runtime.releaseIntent(view.operationId); move.reset(); route.clear(); }
            else if(observed.stopped && !route.empty()){
                while(nextStep<route.size() && route[nextStep]==observed.position){ ++nextStep; }
                if(nextStep<route.size()){
                    const auto dst=route[nextStep];
                    if(!withinTolerance(observed.actionEndpoint,dst,1)){ route.clear(); }
                    else{
                        // Unit hops keep the existing decompActionMove on its constant-work branch.
                        hero.emplaceAction(ActionMove{.speed=SYS_DEFSPEED,.x=hero.currMotion()->endX,.y=hero.currMotion()->endY,
                            .aimX=dst.x,.aimY=dst.y,.extParam{.onHorse=hero.onHorse()}});
                    }
                }
            }
            return update;
        }
    };

    AiGameAdapter::AiGameAdapter(Client &client, AiBridgeRuntime &runtime): m_impl(std::make_unique<Impl>(client,runtime)) {}
    AiGameAdapter::~AiGameAdapter() = default;
    void AiGameAdapter::beginPoll() noexcept { m_impl->observationReady=false; }
    void AiGameAdapter::observe() { m_impl->observe(); }
    void AiGameAdapter::markChanged() noexcept { m_impl->notificationPending=true; m_impl->observationReady=false; }
    std::string AiGameAdapter::targetHandle(uint64_t uid, const Json &context) const
    {
        auto &s=*m_impl;
        if(s.controller().context()!=context){ throw ProtocolError("stale_context","Target context changed"); }
        s.observe();
        s.refresh();
        auto &scene=s.run();
        const auto co=scene.findUID(uid,true);
        if(s.observationError || !co || !currentHero(scene).inViewRange(co)){
            throw ProtocolError("stale_target","Target is not currently observable");
        }
        return s.handles.targetHandle({uid,co->incarnation()});
    }
    TargetIdentity AiGameAdapter::resolveTarget(std::string_view handle,const Json &context) const
    {
        auto &s=*m_impl;
        s.observe();
        const auto identity=s.handles.resolveTarget(handle,context);
        auto &scene=s.run();
        const auto co=scene.findUID(identity.uid,true);
        if(!co || co->incarnation()!=identity.incarnation || !currentHero(scene).inViewRange(co)){
            throw ProtocolError("stale_target","Target left the current observation");
        }
        return identity;
    }
    GroundIdentity AiGameAdapter::resolveGround(std::string_view handle,const Json &context) const
    {
        auto &s=*m_impl;
        s.observe();
        const auto identity=s.handles.resolveGround(handle,context);
        auto &scene=s.run();
        const auto p=identity.position;
        if(!currentHero(scene).inViewRange(p.x,p.y) || scene.getGroundItemIDList(p.x,p.y).empty()
            || scene.aiGroundRevision(p.x,p.y)!=identity.revision){
            throw ProtocolError("stale_target","Ground observation changed");
        }
        return identity;
    }

    void AiGameAdapter::registerTools()
    {
        auto &s=*m_impl;
        s.controller().registerQuery("game_get_state",[&s](const Json &,const Json &){
            s.run();
            s.refresh();
            if(s.observationError || s.character.characterUid==0){ throw ProtocolError("unavailable","Character observation not ready"); }
            return stateSnapshot(s.character);
        });
        s.controller().registerQuery("game_list_nearby",[&s](const Json &params,const Json &context){
            s.run();
            Json filters={{"kind",params.value("kind","all")},{"radius",params.value("radius",15)}};
            if(params.contains("cursor")){ return s.pages.next(SnapshotList::Nearby,context,filters,params.at("cursor").get<std::string>()); }
            s.refresh();
            if(s.observationError){ throw ProtocolError(*s.observationError,"Nearby observation exceeds available coverage"); }
            std::vector<Json> rows;
            for(const auto &entry: selectNearby(s.visible,s.character.position,filters.at("kind").get<std::string>(),filters.at("radius").get<int>())){
                rows.push_back(nearbyEntry(entry,s.handles));
            }
            return s.pages.first(SnapshotList::Nearby,context,filters,s.character.stamp,std::move(rows));
        });
        s.controller().registerQuery("game_get_inventory",[&s](const Json &params,const Json &context){
            s.run();
            if(params.contains("cursor")){ return s.pages.next(SnapshotList::Inventory,context,Json::object(),params.at("cursor").get<std::string>()); }
            s.refresh();
            if(s.observationError){ throw ProtocolError(*s.observationError,"Inventory observation unavailable"); }
            std::vector<Json> rows;
            for(const auto &entry:s.inventory){ rows.push_back(inventoryEntry(entry)); }
            return s.pages.first(SnapshotList::Inventory,context,Json::object(),s.character.stamp,std::move(rows),s.inventoryUnavailable);
        });
        s.controller().registerQuery("game_get_map_region",[&s](const Json &params,const Json &){
            auto &scene=s.run(); auto &hero=currentHero(scene);
            if(scene.getCOList().size()>FrozenPages::maxRows){ throw ProtocolError("unavailable","Object observation budget exceeded"); }
            const auto [uid,w,h]=scene.getMap();
            std::set<GridPoint> occupied;
            for(const auto &[id,co]:scene.getCOList()){
                if(co && scene.findUID(id,true) && hero.inViewRange(co.get())){ occupied.insert(position(*co)); occupied.insert(endpoint(*co)); }
            }
            return mapRegion(s.stamp(),uid,w,h,params.at("x").get<int>(),params.at("y").get<int>(),params.at("width").get<int>(),params.at("height").get<int>(),
                [&](int x,int y){ return TerrainCell{scene.groundValid(x,y),hero.inViewRange(x,y) ? std::optional<bool>(occupied.contains({x,y})) : std::nullopt}; });
        });
        ActionAdapter movement;
        movement.validate=[&s](const Json &p,const Json &c){ return s.validateMove(p,c); };
        movement.begin=[&s](const OperationView &v){ return s.begin(v); };
        movement.tick=[&s](const OperationView &v){ return s.tick(v); };
        movement.cancel=[&s](const OperationView &v){
            // The controller calls its ownership-aware stop hook immediately after this callback.
            const bool submitted=v.submitted || s.submitted();
            s.move.reset();
            s.route.clear();
            return OperationUpdate{.outcome=OperationOutcome::Cancelled,.submitted=submitted};
        };
        s.controller().registerAction("game_move_to",std::move(movement));
    }
}
