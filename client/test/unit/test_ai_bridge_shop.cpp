#include "aibridge/shopmodel.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aibridge;
namespace
{
    Json shopContext()
    {
        return {{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},{"world_epoch","1"},{"control_epoch","1"}};
    }
}

TEST_CASE("Shop first-buy receipt cannot claim a manual overlapping retry or overwrite its first result")
{
    ShopBuyReceipt late;
    late.arm(0);
    CHECK_FALSE(late.receive(2,0));
    CHECK_FALSE(late.receive(2,7));
    CHECK_FALSE(late.result());
    ShopBuyReceipt reverse;
    reverse.arm(0);
    CHECK_FALSE(reverse.receive(2,7));
    CHECK_FALSE(reverse.receive(2,0));
    CHECK_FALSE(reverse.result());
    ShopBuyReceipt single;
    single.arm(0);
    REQUIRE(single.receive(1,0));
    CHECK_FALSE(single.receive(1,7));
    CHECK(single.result()==0);
    ShopBuyReceipt repeated;
    repeated.arm(1);
    CHECK_FALSE(repeated.receive(2,0));
}

TEST_CASE("Shop real panel source survives first observation but rejects a previous world")
{
    ShopPanelSource source;
    const auto initial=shopContext();
    CHECK_FALSE(source.current(initial));
    REQUIRE(source.observe(initial));
    CHECK(source.current(initial));
    CHECK_FALSE(source.observe(initial));
    auto resumed=initial; resumed["control_epoch"]="2";
    CHECK(source.current(resumed));
    auto world=initial; world["world_epoch"]="2";
    CHECK_FALSE(source.current(world));
    CHECK(source.observe(world));
    CHECK(source.current(world));
    CHECK_FALSE(source.current(initial));
}

TEST_CASE("Shop query coalesces only the same live identity")
{
    ShopQuery query;
    const ShopQueryKey key{10,20,30,40,1};
    REQUIRE(query.begin(key,100)==ShopQuery::Start::Sent);
    CHECK(query.begin(key,101)==ShopQuery::Start::Coalesced);
    auto next=key; ++next.panelRevision;
    CHECK(query.begin(next,102)==ShopQuery::Start::Busy);
    CHECK(query.receive(30,41,1,103)==ShopQuery::Reply::Ignored);
    CHECK(query.pending());
    CHECK(query.receive(30,40,2,104)==ShopQuery::Reply::Ignored);
    CHECK(query.receive(30,40,1,105)==ShopQuery::Reply::Accepted);
    CHECK(query.accepted());
    CHECK_FALSE(query.timedOut(50000));
}

TEST_CASE("Shop timeout retains the unnumbered response slot until it drains")
{
    ShopQuery query;
    const ShopQueryKey key{10,20,30,40,1};
    REQUIRE(query.begin(key,100)==ShopQuery::Start::Sent);
    CHECK(query.timedOut(10100));
    CHECK(query.begin(key,10100)==ShopQuery::Start::Busy);
    CHECK(query.receive(30,40,1,10101)==ShopQuery::Reply::Drained);
    CHECK_FALSE(query.accepted());
    CHECK(query.timedOut(10102));
    CHECK(query.begin(key,10103)==ShopQuery::Start::Sent);
    CHECK(query.serial()==2);
    CHECK_FALSE(query.timedOut(10104));
}

TEST_CASE("Shop panel replacement cannot consume the old quotation as a new one")
{
    ShopQuery query;
    const ShopQueryKey key{10,20,30,40,1};
    REQUIRE(query.begin(key,100)==ShopQuery::Start::Sent);
    query.abandon();
    CHECK(query.begin(key,101)==ShopQuery::Start::Busy);
    CHECK(query.receive(30,40,1,102)==ShopQuery::Reply::Drained);
    CHECK_FALSE(query.accepted());
    auto replacement=key; ++replacement.npcUid;
    CHECK(query.begin(replacement,103)==ShopQuery::Start::Sent);
    CHECK(query.receive(30,40,1,104)==ShopQuery::Reply::Accepted);
    query.abandon();
    CHECK_FALSE(query.accepted());
}

TEST_CASE("Shop clock rollback cannot renew a query")
{
    ShopQuery query;
    REQUIRE(query.begin({1,2,3,4,5},100)==ShopQuery::Start::Sent);
    CHECK(query.timedOut(99));
    CHECK(query.receive(3,4,5,99)==ShopQuery::Reply::Drained);
}

TEST_CASE("Packable shop buy observes real one-request costs and full inventory change")
{
    ShopQuote quote{.itemId=1,.unitCount=1,.gold=10,.materials={{2,3}}};
    ShopObservation before{.gold=100,.items={{1,2},{2,5}}};
    ShopObservation after{.gold=90,.items={{1,7},{2,2}}};
    CHECK(shopPostcondition(quote,5,before,after));
    after.gold=50;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    after.gold=90; after.items[2]=5;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    after.items[2]=2; after.items[9]=1;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
}

TEST_CASE("Shop buy handles matching cost and product item without unsigned wrap")
{
    ShopQuote quote{.itemId=1,.unitCount=1,.gold=10,.materials={{1,3}}};
    ShopObservation before{.gold=100,.items={{1,3}}};
    ShopObservation after{.gold=90,.items={{1,5}}};
    CHECK(shopPostcondition(quote,5,before,after));
    before.items[1]=2;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    before.items[1]=UINT64_MAX; quote.materials.clear();
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    before.gold=9;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
}

TEST_CASE("Shop sale requires the original sell unit plus matching gold")
{
    ShopQuote quote{.itemId=1,.seqId=3,.unitCount=5,.gold=10,.selling=true};
    ShopObservation before{.gold=100,.exactCount=5,.items={{1,7}}};
    ShopObservation after{.gold=110,.exactCount=0,.items={{1,2}}};
    CHECK(shopPostcondition(quote,5,before,after));
    after.exactCount=1;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    after.exactCount=0; after.gold=100;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    after.gold=110; before.exactCount=4;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
    before.exactCount=5; before.gold=UINT64_MAX;
    CHECK_FALSE(shopPostcondition(quote,5,before,after));
}

TEST_CASE("Shop default script sells one item from a stack without inventing a count parameter")
{
    ShopQuote quote{.itemId=1,.seqId=3,.unitCount=1,.gold=10,.selling=true};
    ShopObservation before{.gold=100,.exactCount=5,.items={{1,7}}};
    ShopObservation after{.gold=110,.exactCount=4,.items={{1,6}}};
    CHECK(shopPostcondition(quote,1,before,after));
    after.exactCount=0; after.items[1]=2;
    CHECK_FALSE(shopPostcondition(quote,1,before,after));
}

TEST_CASE("Shop nonpackable buy requires the quoted exact sequence in observed inventory")
{
    ShopQuote quote{.itemId=1,.seqId=9,.unitCount=1,.gold=10};
    ShopObservation before{.gold=100,.items={{1,2}}};
    ShopObservation after{.gold=90,.exactCount=0,.items={{1,3}}};
    CHECK_FALSE(shopPostcondition(quote,1,before,after));
    after.exactCount=1;
    CHECK(shopPostcondition(quote,1,before,after));
}
