#include "combatadapter.hpp"
#include "gameadapter.hpp"
#include "movement.hpp"
#include "actions.hpp"
#include "runtime.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "gui/skillboard/skillboard.hpp"
#include "inputstringboard.hpp"
#include "dbcomid.hpp"
#include <chrono>
#include <limits>
#include <map>
#include <set>

namespace aibridge
{
    namespace
    {
        uint64_t monotonicMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        uint64_t unixMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        GridPoint point(const ClientCreature &co) { const auto [x,y]=co.location(); return {x,y}; }
        bool beneficialMagic(uint32_t id)
        {
            return id==DBCOM_MAGICID(u8"治愈术") || id==DBCOM_MAGICID(u8"幽灵盾") || id==DBCOM_MAGICID(u8"神圣战甲术");
        }
        bool canDamage(std::string_view method,const Json &params)
        {
            if(method=="game_attack"){ return true; }
            if(method!="game_cast"){ return false; }
            const auto id=params.at("magic_id").get<uint32_t>();
            return !beneficialMagic(id) && id!=DBCOM_MAGICID(u8"魔法盾") && id!=DBCOM_MAGICID(u8"阴阳法环")
                && ProcessRun::supportedMagicCastType(id)!=ProcessRun::MagicCastType::Toggle;
        }
        std::string_view castTypeName(uint32_t id)
        {
            switch(ProcessRun::supportedMagicCastType(id)){
                case ProcessRun::MagicCastType::Target: return "target";
                case ProcessRun::MagicCastType::Ground: return "ground";
                case ProcessRun::MagicCastType::Self: return "self";
                case ProcessRun::MagicCastType::Toggle: return "toggle";
                default: return "unsupported";
            }
        }
        MyHero &heroOf(ProcessRun &scene)
        {
            const auto hero=dynamic_cast<MyHero *>(scene.findUID(scene.getMyHeroUID(),false));
            if(!hero){ throw ProtocolError("not_in_game","Character is not available"); }
            return *hero;
        }
        using ItemKey=std::pair<uint32_t,uint32_t>;
        using ItemCounts=std::map<ItemKey,uint32_t>;
        ItemCounts inventoryCounts(const MyHero &hero)
        {
            if(!hero.inventoryKnown()){ throw ProtocolError("unavailable","Inventory is not known"); }
            const auto &bins=hero.getInvPack().getPackBinList();
            if(bins.size()>FrozenPages::maxRows){ throw ProtocolError("unavailable","Inventory scan limit exceeded"); }
            ItemCounts result;
            for(const auto &bin:bins){
                const auto &i=bin.item;
                if(!i){ continue; }
                auto &count=result[{i.itemID,i.seqID}];
                if(i.count>std::numeric_limits<uint32_t>::max()-uint64_t(count)){
                    throw ProtocolError("unavailable","Inventory count is outside contract range");
                }
                count+=uint32_t(i.count);
            }
            return result;
        }
        bool busyPanel(const ProcessRun &scene)
        {
            return dynamic_cast<const InputStringBoard *>(scene.getWidget("InputStringBoard"))->showModal()
                || dynamic_cast<const DirectTradeBoard *>(scene.getWidget("DirectTradeBoard"))->peerUID()
                || dynamic_cast<const AuctionBoard *>(scene.getWidget("AuctionBoard"))->pending()
                || dynamic_cast<const AuctionRegisterBoard *>(scene.getWidget("AuctionRegisterBoard"))->pending();
        }
        OperationError error(std::string code)
        {
            return {std::move(code),"当前游戏状态不满足操作条件",Json::object()};
        }
    }

    struct AiCombatAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        uint64_t sequence=0;
        std::string operationId;
        std::unique_ptr<ActionProgress> progress;
        uint64_t heroIncarnation=0, actionSequence=0, attackSequence=0, spellSequence=0, kickSequence=0;
        uint64_t pickupSequence=0, pickupActionSequence=0, consumeSequence=0, pickupErrorSequence=0, serverRevision=0;
        uint64_t deathSequence=0;
        uint64_t reviveReceiptSequence=0;
        uint64_t started=0, lastProgress=0;
        std::optional<TargetIdentity> target;
        std::optional<GroundIdentity> pickupGround;
        std::optional<EquipmentSelection> equipment;
        uint64_t equipmentSequence=0;
        std::set<uint32_t> pickupItemIds;
        bool pickupGold=false;
        GridPoint lastPosition;
        bool sent=false, actionSent=false, targetDead=false, beforeInventoryKnown=false;
        ItemCounts beforeItems;
        std::optional<uint64_t> beforeGold;
        std::vector<pathf::PathNode> route;
        size_t nextHop=1;
        GridPoint routeTarget;

        Impl(Client &c,AiBridgeRuntime &r):client(c),runtime(r) {}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            auto scene=client.getProcess<ProcessRun>();
            if(!scene || scene->aiState().phase!="run" || controller().context().is_null()){
                throw ProtocolError("not_in_game","No current game scene");
            }
            return *scene;
        }
        SnapshotStamp stamp() { return {++sequence,unixMs()}; }
        Json evidence(std::string_view kind,std::string_view description)
        {
            const auto now=stamp();
            return Json::array({{{"source","client"},{"kind",kind},{"observed_at",std::to_string(now.observedAt)},
                {"snapshot_seq",std::to_string(now.sequence)},{"server_code",nullptr},{"description",description}}});
        }
        ClientCreature &resolve(const Json &params,const Json &context)
        {
            const auto identity=runtime.game()->resolveTarget(params.at("target_handle").get<std::string>(),context);
            const auto co=run().findUID(identity.uid,true);
            if(!co || co->incarnation()!=identity.incarnation){ throw ProtocolError("stale_target","Target was replaced"); }
            return *co;
        }
        void validateCommon(ProcessRun &scene,MyHero &hero,const Json &context)
        {
            if(controller().context()!=context){ throw ProtocolError("stale_context","Character context changed"); }
            if(!scene.aiState().alive){ throw ProtocolError("paused","Character is not alive"); }
            if(scene.getCOList().size()>FrozenPages::maxRows){ throw ProtocolError("unavailable","Object scan limit exceeded"); }
            if(!hero.aiCanStartMove() || busyPanel(scene) || scene.getFocusUID(FOCUS_ATTACK)
                || scene.getFocusUID(FOCUS_FOLLOW) || scene.getFocusUID(FOCUS_MAGIC)){
                throw ProtocolError("busy","A human action or modal interaction is active");
            }
        }
        std::optional<OperationError> combatPermission(std::string_view method,const Json &params,const ProcessRun &scene,const MyHero &hero) const
        {
            if(!canDamage(method,params)){ return {}; }
            if(scene.getCOList().size()>FrozenPages::maxRows){ return error("unavailable"); }
            std::vector<uint64_t> peers;
            for(const auto &[uid,co]:scene.getCOList()){
                if(uid!=hero.UID() && co && uidf::getUIDType(uid)==UID_PLY && scene.findUID(uid,true) && hero.inViewRange(co.get())){
                    peers.push_back(uid);
                }
            }
            const auto policy=runtime.policy();
            if(!policy){ return error("policy_denied"); }
            // A conservative visible-view superset also covers explicit-target AoE and swing toggles.
            return validateCombatPeers(*policy,peers);
        }
        ValidationResult validate(std::string_view method,const Json &params,const Json &context)
        {
            try{
                auto &scene=run(); auto &hero=heroOf(scene);
                if(method=="game_revive"){
                    if(controller().context()!=context){ throw ProtocolError("stale_context","Character context changed"); }
                    if(!scene.aiState().knownDead()){ throw ProtocolError("paused","Revival requires confirmed death"); }
                    if(busyPanel(scene)){ throw ProtocolError("busy","A modal interaction is active"); }
                    return {};
                }
                validateCommon(scene,hero,context);
                BudgetRequest budget;
                bool targetIsSelf=false;
                if(method=="game_attack" || (method=="game_cast" && params.contains("target_handle"))){
                    auto &co=resolve(params,context);
                    const auto type=uidf::getUIDType(co.UID());
                    targetIsSelf=co.UID()==hero.UID();
                    if((targetIsSelf && method=="game_attack") || (type!=UID_MON && type!=UID_PLY)){
                        throw ProtocolError("invalid_argument","Target is not a combat creature");
                    }
                    if(!co.alive() || co.dead()==true){ throw ProtocolError("target_lost","Target is dead"); }
                    if(!co.dead().has_value()){ throw ProtocolError("unavailable","Target life state is unknown"); }
                    if(type==UID_PLY && co.UID()!=hero.UID()){
                        budget.pvp=method=="game_attack" || !beneficialMagic(params.at("magic_id").get<uint32_t>());
                        budget.peerUid=co.UID();
                    }
                }
                if(method=="game_pick_up"){
                    const auto ground=runtime.game()->resolveGround(params.at("ground_handle").get<std::string>(),context);
                    if(!hero.aiMotionStopped() || point(hero)!=ground.position
                        || hero.currMotion()->endX!=ground.position.x || hero.currMotion()->endY!=ground.position.y){
                        throw ProtocolError("need_move","Pickup requires the actual current grid");
                    }
                    const auto &items=scene.getGroundItemIDList(ground.position.x,ground.position.y);
                    if(items.size()>100){ throw ProtocolError("unavailable","Ground item coverage exceeds limit"); }
                    budget.itemIds=items;
                }
                else if(method=="game_use_item"){
                    const auto id=params.at("item_id").get<uint32_t>(), seq=params.at("seq_id").get<uint32_t>();
                    const auto count=params.at("count").get<uint32_t>();
                    const auto &record=DBCOM_ITEMRECORD(id);
                    const bool wearable=inventoryWearSlot(id,hero.getWLDesp().wear).has_value();
                    if(!record || (!wearable && to_u8sv(record.type)!=u8"恢复药水" && to_u8sv(record.type)!=u8"强化药水" && to_u8sv(record.type)!=u8"技能书")){
                        throw ProtocolError("unsupported_item_type","Item does not use the normal consume path");
                    }
                    if(wearable){ scene.prepareInventoryWear(id,seq,count,EquipmentOrigin::Ai); }
                    if(to_u8sv(record.type)==u8"技能书" && count!=1){ throw ProtocolError("invalid_argument","Books are consumed one at a time"); }
                    const auto items=inventoryCounts(hero);
                    const auto p=items.find({id,seq});
                    if(p==items.end() || p->second<count){ throw ProtocolError("invalid_argument","Not enough available inventory"); }
                    const auto matches=[&](const SDItem &item){ return item && item.itemID==id && item.seqID==seq; };
                    if(matches(hero.getInvPack().getGrabbedItem())
                        || matches(dynamic_cast<const AuctionRegisterBoard *>(scene.getWidget("AuctionRegisterBoard"))->reservedItem())){
                        throw ProtocolError("busy","Item identity is reserved by a GUI interaction");
                    }
                    for(const auto &item:dynamic_cast<const DirectTradeBoard *>(scene.getWidget("DirectTradeBoard"))->localItemList()){
                        if(matches(item)){ throw ProtocolError("busy","Item identity is reserved by trade"); }
                    }
                    budget.itemIds={id}; budget.itemCount=count;
                }
                else if(method=="game_cast"){
                    const auto id=params.at("magic_id").get<uint32_t>();
                    const auto &config=dynamic_cast<const SkillBoard *>(scene.getWidget("SkillBoard"))->getConfig();
                    const auto level=config.getMagicLevel(id);
                    const auto type=ProcessRun::supportedMagicCastType(id);
                    if(!scene.aiSkillsKnown()){ throw ProtocolError("unavailable","Learned skills are not known"); }
                    if(!level || *level<1 || type==ProcessRun::MagicCastType::Unsupported){ throw ProtocolError("unsupported_skill","Skill has no supported learned action path"); }
                    if(hero.magicCooldownRemaining(id)){ throw ProtocolError("busy","Skill is cooling down"); }
                    const bool hasTarget=params.contains("target_handle"), hasGrid=params.contains("x");
                    if(auto invalid=validateCastTarget(castTypeName(id),beneficialMagic(id),hasTarget,hasGrid,targetIsSelf)){
                        return {*invalid,{}};
                    }
                    if(hasGrid){
                        const auto x=params.at("x").get<int64_t>(),y=params.at("y").get<int64_t>();
                        if(x>std::numeric_limits<int16_t>::max() || y>std::numeric_limits<int16_t>::max() || !scene.onMap(int(x),int(y))){
                            throw ProtocolError("out_of_bounds","Spell grid is outside the current action map");
                        }
                        if(!hero.inViewRange(int(x),int(y))){ throw ProtocolError("out_of_bounds","Spell grid is outside current view"); }
                    }
                }
                if(auto denied=combatPermission(method,params,scene,hero)){ return {*denied,{}}; }
                return {{},std::move(budget)};
            }
            catch(const ProtocolError &e){ return {error(e.code),{}}; }
        }
        Json skills()
        {
            auto &scene=run(); auto &hero=heroOf(scene);
            auto result=snapshotMetadata(stamp(),"client_known",scene.aiSkillsKnown() ? std::vector<std::string>{} : std::vector<std::string>{"items"});
            result["items"]=Json::array();
            if(scene.aiSkillsKnown()){
                const auto &config=dynamic_cast<const SkillBoard *>(scene.getWidget("SkillBoard"))->getConfig();
                if(config.magicCount()>100){ throw ProtocolError("unavailable","Skill scan limit exceeded"); }
                const auto learned=config.learnedMagicLevels();
                if(learned.size()>100){ throw ProtocolError("unavailable","Skill snapshot exceeds contract limit"); }
                for(const auto &[id,level]:learned){
                    const auto &record=DBCOM_MAGICRECORD(id);
                    const auto type=ProcessRun::supportedMagicCastType(id);
                    result["items"].push_back({{"magic_id",id},{"name",to_cstr(record.name)},{"level",level},
                        {"supported",type!=ProcessRun::MagicCastType::Unsupported},{"action_type",castTypeName(id)},
                        {"cooldown_ms",hero.magicCooldownRemaining(id)},
                        {"enabled",type==ProcessRun::MagicCastType::Toggle ? Json(hero.hasSwingMagic(id)) : Json(nullptr)}});
                }
            }
            validateDefinition(result,"skills");
            return result;
        }
        bool submitted()
        {
            if(auto scene=client.getProcess<ProcessRun>()){
                const auto hero=dynamic_cast<MyHero *>(scene->findUID(scene->getMyHeroUID(),false));
                if(hero && hero->incarnation()==heroIncarnation){
                    if(equipment){ sent |= scene->equipmentRequests().submitted(equipmentSequence,*equipment); }
                    else{
                        sent |= hero->actionSendSequence()!=actionSequence
                            || scene->aiPickupSendSequence()!=pickupSequence || scene->aiConsumeSendSequence()!=consumeSequence;
                    }
                }
            }
            return sent;
        }
        OperationUpdate finish(const OperationView &view,OperationUpdate update)
        {
            if(update.outcome!=OperationOutcome::Running){
                runtime.releaseIntent(view.operationId);
                progress.reset(); route.clear();
            }
            return update;
        }
        OperationUpdate fail(const OperationView &view,std::string code,bool uncertain=false)
        {
            const bool wasSent=view.submitted || submitted();
            return finish(view,OperationUpdate{.outcome=uncertain && wasSent ? OperationOutcome::Unknown : OperationOutcome::Failed,
                .submitted=wasSent,.error=error(std::move(code))});
        }
        OperationUpdate begin(const OperationView &view)
        {
            try{
                const auto validation=validate(view.method,view.params,view.context);
                if(validation.error){ return {.outcome=OperationOutcome::Failed,.error=validation.error}; }
                auto &scene=run(); auto &hero=heroOf(scene);
                operationId=view.operationId; heroIncarnation=hero.incarnation();
                target.reset(); pickupGround.reset(); route.clear(); beforeItems.clear(); beforeGold.reset();
                equipment.reset(); equipmentSequence=0;
                pickupItemIds.clear(); pickupGold=false;
                sent=false; actionSent=false; targetDead=false; beforeInventoryKnown=false;
                actionSequence=hero.actionSendSequence(); attackSequence=hero.actionTypeSendSequence(ACTION_ATTACK);
                spellSequence=hero.actionTypeSendSequence(ACTION_SPELL); kickSequence=hero.actionTypeSendSequence(ACTION_SPINKICK);
                pickupActionSequence=hero.actionTypeSendSequence(ACTION_PICKUP);
                pickupSequence=scene.aiPickupSendSequence(); consumeSequence=scene.aiConsumeSendSequence();
                pickupErrorSequence=scene.aiPickupErrorSequence(); serverRevision=scene.aiServerRevision();
                deathSequence=scene.aiDeathSequence();
                started=lastProgress=monotonicMs(); lastPosition=point(hero);
                if(started>=view.deadlineMs){ return fail(view,"timeout"); }
                if(view.method=="game_revive"){
                    reviveReceiptSequence=scene.aiReviveReceiptSequence();
                    const auto before=scene.aiReviveSendSequence();
                    scene.requestRevive();
                    sent=scene.aiReviveSendSequence()!=before;
                    return {.submitted=sent,.result={{"submitted",sent}}};
                }
                ActionKind kind=ActionKind::PickUp;
                if(view.method=="game_use_item"){
                    kind=ActionKind::UseItem;
                    const auto id=view.params.at("item_id").get<uint32_t>();
                    if(inventoryWearSlot(id,hero.getWLDesp().wear)){
                        equipment=scene.prepareInventoryWear(id,view.params.at("seq_id").get<uint32_t>(),view.params.at("count").get<uint32_t>(),EquipmentOrigin::Ai);
                        equipmentSequence=scene.equipmentRequests().sequence()+1;
                        kind=ActionKind::EquipWear;
                    }
                }
                else if(view.method=="game_attack"){
                    kind=view.params.value("mode","once")=="once" ? ActionKind::AttackOnce : ActionKind::AttackUntilDead;
                }
                else if(view.method=="game_cast"){
                    kind=ProcessRun::supportedMagicCastType(view.params.at("magic_id").get<uint32_t>())==ProcessRun::MagicCastType::Toggle
                        ? ActionKind::Toggle : ActionKind::Cast;
                }
                if(view.params.contains("target_handle")){
                    auto &co=resolve(view.params,view.context); target=TargetIdentity{co.UID(),co.incarnation()};
                }
                if(view.method=="game_pick_up" || (view.method=="game_use_item" && !equipment)){
                    if(hero.inventoryKnown()){ beforeItems=inventoryCounts(hero); beforeInventoryKnown=true; }
                    if(hero.goldKnown()){ beforeGold=hero.getGold(); }
                }
                progress=std::make_unique<ActionProgress>(kind,view.context,started,view.deadlineMs-started,
                    view.params.value("item_id",0U),view.params.value("seq_id",0U),view.params.value("count",1U),
                    equipment ? equipment->slot : -1,equipmentSequence);
                if(!runtime.claimIntent(view.operationId,view.method=="game_attack" ? "attack" : "queued")){
                    return fail(view,"busy");
                }
                if(view.method=="game_attack"){
                    if(!runtime.setIntentFocus(view.operationId,FOCUS_ATTACK,target->uid)){ return fail(view,"stale_context"); }
                }
                else if(view.method=="game_pick_up"){
                    pickupGround=runtime.game()->resolveGround(view.params.at("ground_handle").get<std::string>(),view.context);
                    for(const auto id:scene.getGroundItemIDList(pickupGround->position.x,pickupGround->position.y)){
                        pickupItemIds.insert(id);
                        pickupGold |= DBCOM_ITEMRECORD(id).isGold();
                    }
                    hero.setAiPickupGround(pickupGround->position.x,pickupGround->position.y,pickupGround->revision);
                    hero.emplaceAction(ActionPickUp{.x=hero.currMotion()->endX,.y=hero.currMotion()->endY});
                }
                else if(view.method=="game_use_item"){
                    if(equipment){
                        if(!scene.requestInventoryWear(*equipment,EquipmentOrigin::Ai,[this,view]{ return maySubmit(view); })){ return fail(view,"busy",true); }
                    }
                    else{
                        scene.requestConsumeItem(view.params.at("item_id").get<uint32_t>(),view.params.at("seq_id").get<uint32_t>(),view.params.at("count").get<uint32_t>());
                    }
                }
                else if(view.method=="game_cast"){
                    const auto id=view.params.at("magic_id").get<uint32_t>();
                    if(target && !runtime.setIntentFocus(view.operationId,FOCUS_MAGIC,target->uid)){ return fail(view,"stale_context"); }
                    const auto aimUID=target ? target->uid : (beneficialMagic(id) && !view.params.contains("x") ? hero.UID() : 0);
                    if(!scene.castMagic(id,aimUID,view.params.value("x",hero.x()),view.params.value("y",hero.y()))){
                        return fail(view,"busy");
                    }
                    runtime.game()->markChanged();
                }
                const auto life=controller().lifetime();
                hero.setAiActionGuard([this,life,view]{ return !life.expired() && maySubmit(view); });
                return tick(view);
            }
            catch(const ProtocolError &e){ return fail(view,e.code,true); }
        }
        bool maySubmit(const OperationView &view) const
        {
            if(controller().controlState()!=ControlState::Active || controller().context()!=view.context
                || controller().activeOperation()!=view.operationId || monotonicMs()>=view.deadlineMs){ return false; }
            auto &scene=run(); auto &hero=heroOf(scene);
            if(hero.incarnation()!=heroIncarnation || !scene.aiState().alive || scene.aiServerRevision()!=serverRevision
                || scene.getCOList().size()>FrozenPages::maxRows || busyPanel(scene)){ return false; }
            if(target){
                const auto co=scene.findUID(target->uid,true);
                if(!co || co->incarnation()!=target->incarnation || !hero.inViewRange(co) || !co->alive() || co->dead()==true){ return false; }
            }
            if(pickupGround && scene.aiGroundRevision(pickupGround->position.x,pickupGround->position.y)!=pickupGround->revision){ return false; }
            if(combatPermission(view.method,view.params,scene,hero)){ return false; }
            return true;
        }
        ActionObservation observation(const OperationView &view,ProcessRun &scene,MyHero &hero)
        {
            ActionObservation observed;
            observed.stamp=stamp(); observed.context=controller().context(); observed.alive=scene.aiState().alive;
            observed.submitted=submitted();
            if(view.method=="game_attack"){ actionSent |= hero.actionTypeSendSequence(ACTION_ATTACK)!=attackSequence; }
            else if(view.method=="game_cast"){
                actionSent |= hero.actionTypeSendSequence(ACTION_SPELL)!=spellSequence || hero.actionTypeSendSequence(ACTION_SPINKICK)!=kickSequence;
                const auto id=view.params.at("magic_id").get<uint32_t>();
                if(ProcessRun::supportedMagicCastType(id)==ProcessRun::MagicCastType::Toggle){ observed.toggleEnabled=hero.hasSwingMagic(id); }
            }
            else if(view.method=="game_pick_up"){ actionSent |= hero.actionTypeSendSequence(ACTION_PICKUP)!=pickupActionSequence; }
            else if(view.method=="game_use_item"){
                if(equipment){
                    actionSent |= scene.equipmentRequests().submitted(equipmentSequence,*equipment);
                    if(scene.equipmentRequests().observed(equipmentSequence,*equipment,hero.getWLDesp().wear)){
                        observed.equipment=EquipmentObservation{equipment->itemId,equipment->seqId,equipment->slot,equipmentSequence};
                    }
                    if(scene.equipmentRequests().rejected(equipmentSequence)){ observed.rejection=error("server_rejected"); }
                }
                else{ actionSent |= scene.aiConsumeSendSequence()!=consumeSequence; }
            }
            observed.actionStarted=actionSent;
            observed.actionFinished=actionSent && hero.aiMotionStopped();
            if(target){
                const auto co=scene.findUID(target->uid,true);
                observed.targetVisible=co && co->incarnation()==target->incarnation && hero.inViewRange(co);
                if(observed.targetVisible){ targetDead |= !co->alive() || co->dead()==true; }
                targetDead |= scene.aiObservedDeath(target->uid,target->incarnation,deathSequence);
                observed.targetDead=targetDead;
            }
            if(view.method=="game_pick_up" || (view.method=="game_use_item" && !equipment)){
                if(beforeInventoryKnown && hero.inventoryKnown()){
                    auto after=inventoryCounts(hero);
                    if(view.method=="game_use_item"){
                        const auto id=view.params.at("item_id").get<uint32_t>(),seq=view.params.at("seq_id").get<uint32_t>();
                        if(beforeItems[{id,seq}]!=after[{id,seq}]){
                            observed.itemChanges.push_back({id,seq,beforeItems[{id,seq}],after[{id,seq}]});
                        }
                    }
                    else{
                        std::set<ItemKey> keys;
                        for(const auto &[key,count]:beforeItems){ keys.insert(key); }
                        for(const auto &[key,count]:after){ keys.insert(key); }
                        for(const auto &key:keys){
                            if(!pickupItemIds.contains(key.first)){ continue; }
                            const auto before=beforeItems.contains(key) ? beforeItems.at(key) : 0;
                            const auto now=after.contains(key) ? after.at(key) : 0;
                            if(before!=now){
                                if(observed.itemChanges.size()==100){ throw ProtocolError("unavailable","Inventory evidence exceeds contract limit"); }
                                observed.itemChanges.push_back({key.first,key.second,before,now});
                            }
                        }
                    }
                }
                if(beforeGold && hero.goldKnown() && *beforeGold!=hero.getGold() && (view.method!="game_pick_up" || pickupGold)){
                    observed.goldBefore=beforeGold; observed.goldAfter=hero.getGold();
                }
            }
            return observed;
        }
        OperationUpdate tick(const OperationView &view)
        {
            try{
                if(view.method=="game_revive"){
                    if(operationId!=view.operationId){ return fail(view,"stale_context",true); }
                    const auto scene=client.getProcess<ProcessRun>();
                    if(!scene){ return fail(view,"not_in_game",true); }
                    if(scene->aiReviveReceiptSequence()!=reviveReceiptSequence && scene->aiState().knownAlive()){
                        auto proof=evidence("action","收到正常复活回执，并已重新观察到角色存活；普通动作仍暂停");
                        proof[0]["source"]="server"; proof[0]["server_code"]="SM_REVIVE";
                        return finish(view,{.outcome=OperationOutcome::Succeeded,.submitted=sent,
                            .completionScope="observed_postcondition",.evidence=std::move(proof),
                            .result={{"submitted",sent},{"position",{{"x",heroOf(*scene).x()},{"y",heroOf(*scene).y()}}}}});
                    }
                    return {.submitted=sent,.result={{"submitted",sent}}};
                }
                if(!progress || operationId!=view.operationId){ return fail(view,"stale_context",true); }
                auto &scene=run(); auto &hero=heroOf(scene);
                if(hero.incarnation()!=heroIncarnation){ return fail(view,"stale_context",true); }
                if(equipment && (controller().controlState()!=ControlState::Active || controller().context()!=view.context
                    || controller().activeOperation()!=view.operationId)){ return fail(view,"stale_context",true); }
                if(scene.aiServerRevision()!=serverRevision){ return fail(view,"unavailable",true); }
                if(pickupGround && scene.aiPickupSendSequence()==pickupSequence
                    && scene.aiGroundRevision(pickupGround->position.x,pickupGround->position.y)!=pickupGround->revision){
                    return fail(view,"stale_target",true);
                }
                auto observed=observation(view,scene,hero);
                if(view.method=="game_cast" && target && (!observed.targetVisible || observed.targetDead)){
                    return fail(view,"target_lost",true);
                }
                auto update=progress->observe(observed,monotonicMs());
                if(view.method=="game_pick_up" && scene.aiPickupErrorSequence()!=pickupErrorSequence){
                    auto extra=evidence("action","收到拾取错误，物品ID="+std::to_string(scene.aiPickupFailedItem())+"；回执无操作关联，不能确认本次全部结果");
                    extra[0]["source"]="server"; extra[0]["server_code"]="SM_PICKUPERROR";
                    update.evidence.insert(update.evidence.end(),extra.begin(),extra.end());
                }
                if(update.outcome!=OperationOutcome::Running){ return finish(view,std::move(update)); }
                if(auto denied=combatPermission(view.method,view.params,scene,hero)){ return fail(view,denied->code,true); }
                if(hero.aiActionRejected()){ return fail(view,"stale_target",true); }
                if(scene.getCOList().size()>FrozenPages::maxRows){ return fail(view,"unavailable",true); }
                if(busyPanel(scene)){ return fail(view,"busy",true); }
                if(view.method=="game_attack"){
                    const auto co=scene.findUID(target->uid,true);
                    if(!co || co->incarnation()!=target->incarnation || !hero.inViewRange(co)){ return fail(view,"target_lost",true); }
                    const auto current=point(hero);
                    if(current!=lastPosition){ lastPosition=current; lastProgress=monotonicMs(); }
                    const GridPoint end{hero.currMotion()->endX,hero.currMotion()->endY};
                    const auto destination=point(*co);
                    if(!withinTolerance(end,destination,1) && monotonicMs()-lastProgress>=3000){ return fail(view,"stalled"); }
                    if(hero.aiMotionStopped()){
                        if(end==destination){ return fail(view,"need_move"); }
                        if(withinTolerance(end,destination,1) && end!=destination){
                            scene.trackAttack(true,target->uid);
                            route.clear(); lastProgress=monotonicMs();
                        }
                        else{
                            if(route.empty() || routeTarget!=destination){
                                route=hero.aiPlanMove(destination.x,destination.y); nextHop=1; routeTarget=destination;
                                if(route.empty()){ return fail(view,"unreachable"); }
                            }
                            while(nextHop<route.size() && GridPoint{route[nextHop].X,route[nextHop].Y}==current){ ++nextHop; }
                            if(nextHop<route.size()){
                                const GridPoint hop{route[nextHop].X,route[nextHop].Y};
                                if(!withinTolerance(end,hop,1)){ return fail(view,"unavailable",true); }
                                hero.emplaceAction(ActionMove{.speed=SYS_DEFSPEED,.x=end.x,.y=end.y,.aimX=hop.x,.aimY=hop.y,.extParam{.onHorse=hero.onHorse()}});
                            }
                        }
                    }
                }
                update.submitted=submitted();
                return update;
            }
            catch(const ProtocolError &e){ return fail(view,e.code,true); }
        }
        OperationUpdate cancel(const OperationView &view)
        {
            const bool wasSent=view.submitted || submitted();
            auto result=progress ? progress->cancel(wasSent) : OperationUpdate{
                .outcome=wasSent ? OperationOutcome::Unknown : OperationOutcome::Cancelled,.submitted=wasSent};
            progress.reset(); route.clear();
            // Controller invokes the ownership-aware stop hook after cancellation.
            return result;
        }
    };

    AiCombatAdapter::AiCombatAdapter(Client &client,AiBridgeRuntime &runtime):m_impl(std::make_unique<Impl>(client,runtime)) {}
    AiCombatAdapter::~AiCombatAdapter()=default;
    void AiCombatAdapter::registerTools()
    {
        auto &s=*m_impl;
        s.controller().registerQuery("game_get_skills",[&s](const Json &,const Json &){ return s.skills(); });
        for(const std::string method:{"game_pick_up","game_attack","game_cast","game_use_item","game_revive"}){
            ActionAdapter adapter;
            adapter.validate=[&s,method](const Json &params,const Json &context){ return s.validate(method,params,context); };
            adapter.begin=[&s](const OperationView &view){ return s.begin(view); };
            adapter.tick=[&s](const OperationView &view)->std::optional<OperationUpdate>{ return s.tick(view); };
            adapter.cancel=[&s](const OperationView &view){ return s.cancel(view); };
            s.controller().registerAction(method,std::move(adapter));
        }
    }
}
