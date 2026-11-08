#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "serdesmsg.hpp"

namespace
{
    SDItem qualityItem(uint32_t itemID, int quality)
    {
        return SDItem
        {
            .itemID = itemID,
            .extAttrList{SDItem::build_EA_QUALITY(quality)},
        };
    }
}

TEST_CASE("Materials receive per-copy quality independent of durability", "[itemquality]")
{
    const auto [itemID, threshold] = GENERATE(
        std::pair{DBCOM_ITEMID(u8"铁矿"), 13},
        std::pair{DBCOM_ITEMID(u8"牛肉"), 10},
        std::pair{DBCOM_ITEMID(u8"鸡肉"), 4});
    CAPTURE(itemID, threshold);
    bool below = false;
    bool qualifies = false;
    const auto generated = SDItem::buildItemList(itemID, 512);
    REQUIRE(generated.size() == 512);
    for(const auto &item: generated){
        const auto quality = item.getExtAttr<SDItem::EA_QUALITY_t>();
        REQUIRE(quality.has_value());
        REQUIRE(quality.value() >= 1);
        REQUIRE(quality.value() <= 20);
        REQUIRE(item.duration[0] == 0);
        REQUIRE(item.duration[1] == 0);
        below |= quality.value() < threshold;
        qualifies |= quality.value() >= threshold;
    }
    REQUIRE(below);
    REQUIRE(qualifies);
    REQUIRE_FALSE(SDItem::buildItemList(DBCOM_ITEMID(u8"木剑"), 1).front().getExtAttr<SDItem::EA_QUALITY_t>());
}

TEST_CASE("Meat thresholds are inclusive and missing quality never qualifies", "[itemquality]")
{
    const auto [itemID, threshold] = GENERATE(
        std::pair{DBCOM_ITEMID(u8"牛肉"), 10},
        std::pair{DBCOM_ITEMID(u8"鸡肉"), 4});
    CAPTURE(itemID, threshold);
    SDInventory inventory;
    inventory.add(qualityItem(itemID, threshold - 1), false);
    inventory.add(qualityItem(itemID, threshold), false);
    inventory.add(SDItem{.itemID = itemID}, false);
    const auto qualified = inventory.getQualityItemList(itemID, threshold, 1);
    REQUIRE(qualified.has_value());
    REQUIRE(qualified->size() == 1);
    REQUIRE(qualified->front().seqID == 2);
    REQUIRE_FALSE(inventory.getQualityItemList(itemID, threshold, 2));
    REQUIRE(inventory.has(itemID, 0) == 3);
}

TEST_CASE("Quality survives serialization and Lua conversion", "[itemquality]")
{
    const auto iron = qualityItem(DBCOM_ITEMID(u8"铁矿"), 13);
    const auto serialized = cerealf::deserialize<SDItem>(cerealf::serialize(iron));
    REQUIRE(serialized.getExtAttr<SDItem::EA_QUALITY_t>() == 13);
    const auto luaItem = SDItem::fromLuaVar(iron.asLuaVar());
    REQUIRE(luaItem.extAttrList == iron.extAttrList);
}

TEST_CASE("Item descriptions show ore purity and meat quality", "[itemquality]")
{
    REQUIRE(qualityItem(DBCOM_ITEMID(u8"铁矿"), 13).getXMLLayout().find(u8"【纯度】13") != std::u8string::npos);
    REQUIRE(qualityItem(DBCOM_ITEMID(u8"牛肉"), 10).getXMLLayout().find(u8"【品质】10") != std::u8string::npos);
}

TEST_CASE("Skill books describe their profession and learning requirements", "[itemdescription]")
{
    size_t bookCount = 0;
    for(uint32_t itemID = 1; itemID < DBCOM_ITEMENDID(); ++itemID){
        const auto &ir = DBCOM_ITEMRECORD(itemID);
        if(!ir.isBook()){
            continue;
        }
        CAPTURE(itemID, to_cstr(ir.name));
        const auto &mr = DBCOM_MAGICRECORD(ir.isMagicBook() ? ir.book.magic : ir.name);
        REQUIRE(mr);
        REQUIRE(str_haschar(mr.description));
        const SDItem item{.itemID = itemID};
        const auto description = item.getXMLDescription();
        REQUIRE(description.find(str_printf(u8"【技能作用】%s", mr.description)) != std::u8string::npos);
        REQUIRE(description.find(str_printf(u8"【适用职业】%s", mr.req.job)) != std::u8string::npos);
        REQUIRE(description.find(str_printf(u8"【技能等级要求】%d级", mr.req.level[0])) != std::u8string::npos);
        REQUIRE(description.find(u8"游戏处于开发阶段") == std::u8string::npos);
        REQUIRE(item.getXMLLayout().find(description) != std::u8string::npos);
        REQUIRE(description.find(ir.isMagicBook() ? u8"在背包中使用" : u8"交给武功教头誊抄") != std::u8string::npos);
        if(str_haschar(mr.req.prior)){
            REQUIRE(description.find(str_printf(u8"【前置技能】%s", mr.req.prior)) != std::u8string::npos);
        }
        ++bookCount;
    }
    REQUIRE(bookCount == 126);

    const auto lightning = SDItem{.itemID = DBCOM_ITEMID(u8"雷电术（秘籍）")}.getXMLDescription();
    REQUIRE(lightning.find(u8"【适用职业】法师") != std::u8string::npos);
    REQUIRE(lightning.find(u8"【技能等级要求】16级") != std::u8string::npos);
    REQUIRE(lightning.find(u8"【前置技能】霹雳掌") != std::u8string::npos);
    REQUIRE(lightning.find(u8"【技能作用】召唤一道雷电劈击目标。") != std::u8string::npos);
    for(const auto *name: {u8"基本剑术", u8"基本剑术（秘籍）"}){
        REQUIRE(SDItem{.itemID = DBCOM_ITEMID(name)}.getXMLDescription().find(u8"【技能作用】提高近战攻击的命中率。") != std::u8string::npos);
    }
    REQUIRE(SDItem{.itemID = DBCOM_ITEMID(u8"治愈术（秘籍）")}.getXMLDescription().find(u8"【技能作用】恢复自己或同伴的生命值。") != std::u8string::npos);
    for(const auto *name: {u8"幽灵盾（秘籍）", u8"神圣战甲术（秘籍）"}){
        const auto description = SDItem{.itemID = DBCOM_ITEMID(name)}.getXMLDescription();
        REQUIRE(description.find(u8"【前置链等级要求】27级") != std::u8string::npos);
    }
    const auto shield = SDItem{.itemID = DBCOM_ITEMID(u8"神圣战甲术（秘籍）")}.getXMLDescription();
    REQUIRE(shield.find(u8"【技能等级要求】25级") != std::u8string::npos);
    REQUIRE(shield.find(u8"【前置技能】幽灵盾") != std::u8string::npos);
}

TEST_CASE("Qualified ore removal preserves low-quality and unrelated items", "[itemquality]")
{
    const auto ironID = DBCOM_ITEMID(u8"铁矿");
    const auto beefID = DBCOM_ITEMID(u8"牛肉");
    SDInventory inventory;
    inventory.add(qualityItem(ironID, 12), false);
    for(int i = 0; i < 5; ++i){
        inventory.add(qualityItem(ironID, 13 + i), false);
    }
    inventory.add(SDItem{.itemID = ironID}, false);
    inventory.add(qualityItem(beefID, 20), false);
    inventory = cerealf::deserialize<SDInventory>(cerealf::serialize(inventory));

    REQUIRE_FALSE(inventory.getQualityItemList(ironID, 13, 6));
    REQUIRE(inventory.has(ironID, 0) == 7);
    const auto plan = inventory.getQualityItemList(ironID, 13, 5);
    REQUIRE(plan.has_value());
    REQUIRE(plan->size() == 5);
    for(const auto &item: plan.value()){
        CAPTURE(item.seqID);
        REQUIRE(item.seqID >= 2);
        REQUIRE(item.seqID <= 6);
        const auto [removed, seqID, remaining] = inventory.remove(item.itemID, item.seqID, item.count, true);
        REQUIRE(removed == 1);
        REQUIRE(seqID == item.seqID);
        REQUIRE_FALSE(remaining);
    }
    REQUIRE(inventory.has(ironID, 0) == 2);
    REQUIRE(inventory.find(ironID, 1));
    REQUIRE(inventory.find(ironID, 7));
    REQUIRE(inventory.has(beefID, 0) == 1);
    REQUIRE(inventory.getQualityItemList(ironID, 0, 1).has_value());
    REQUIRE_FALSE(inventory.getQualityItemList(ironID, 0, 2));
}
