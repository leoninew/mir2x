#include <cstdint>
#include <cstdlib>
#include <cinttypes>

#include "mathf.hpp"
#include "dbcomid.hpp"
#include "server.hpp"
#include "peerconfig.hpp"
#include "dropitemconfig.hpp"
#include "serverargparser.hpp"
#include "serverconfigurewindow.hpp"

extern PeerConfig *g_peerConfig;
extern ServerArgParser *g_serverArgParser;
extern ServerConfigureWindow *g_serverConfigureWindow;

extern Server *g_server;

const std::map<int, std::vector<DropItemConfig>> &getMonsterDropItemConfigList(uint32_t monsterID)
{
    return g_server->gameplayConfig().monsterDropTable(monsterID);
}

std::vector<SDItem> getMonsterDropItemList(uint32_t monsterID)
{
    std::vector<SDItem> itemList;
    const auto [dropRate, goldRate] = []() -> std::tuple<double, double>
    {
        if(g_serverArgParser->slave){
            const auto sdPC = g_peerConfig->getConfig();
            return
            {
                sdPC.dropRate,
                sdPC.goldRate,
            };
        }
        else{
            const auto confg = g_serverConfigureWindow->getConfig();
            return
            {
                confg.dropRate,
                confg.goldRate,
            };
        }
    }();

    fflassert(dropRate >= 0.0);
    fflassert(goldRate >= 0.0);

    for(const auto &[group, dropItemList]: getMonsterDropItemConfigList(monsterID)){
        for(const auto &dropItem: dropItemList){
            const auto adjProbRecip = [&dropItem, dropRate]() -> int
            {
                if(dropItem.probRecip <= 0){
                    return 0;
                }
                return std::max<int>(1, std::lround(dropItem.probRecip / dropRate));
            }();

            if((dropItem.probRecip > 0) && ((mathf::rand() % adjProbRecip) == 0)){
                const auto &ir = DBCOM_ITEMRECORD(dropItem.itemID);
                fflassert(ir);

                if(ir.isGold()){
                    const auto adjGold = std::max<size_t>(1, std::lround(dropItem.count * goldRate));
                    for(const auto &goldItem: SDItem::buildGoldItem(adjGold)){
                        itemList.push_back(SDItem
                        {
                            .itemID = goldItem.itemID,
                            .seqID  = 1,
                            .count  = std::max<size_t>(1, std::lround(goldItem.count * mathf::rand<float>(0.9, 1.1))),
                        });
                    }
                }
                else if(to_u8sv(ir.type) == u8"矿石" || to_u8sv(ir.type) == u8"肉"){
                    itemList.append_range(SDItem::buildItemList(dropItem.itemID, to_uz(dropItem.count)));
                }
                else if(ir.packable()){
                    itemList.push_back(SDItem
                    {
                        .itemID = dropItem.itemID,
                        .seqID  = 1,
                        .count  = to_uz(dropItem.count),
                    });
                }
                else{
                    for(int i = 0; i < dropItem.count; ++i){
                        itemList.push_back(SDItem
                        {
                            .itemID = dropItem.itemID,
                            .seqID  = 1,
                            .count  = 1,
                        });
                    }
                }

                if(group > 0){
                    break;  // can only drop one item if not in zero group
                }
            }
        }
    }
    return itemList;
}
