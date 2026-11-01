#include "aibridge/protocol.hpp"
#include "aibridge/transport.hpp"
#include "aibridge/instancefile.hpp"
#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <thread>
#include <functional>

using namespace aibridge;
using namespace std::chrono_literals;
namespace
{
    Json readJson(const std::filesystem::path &path)
    {
        std::ifstream in(path);
        REQUIRE(in.good());
        return Json::parse(in);
    }
    Json query(std::string method = "game_get_session", std::string id = "query")
    {
        return {{"v", 1}, {"id", id}, {"method", method}, {"params", Json::object()}};
    }
    struct Fixture
    {
        AiBridgeTransport bridge;
        asio::io_context io;
        std::filesystem::path directory;
        Fixture()
        {
            static size_t sequence = 0;
            directory = std::filesystem::path(MIR2X_AI_TEST_DIR) / ("transport-" + std::to_string(++sequence));
            std::filesystem::create_directories(directory);
            const auto policy = directory / "policy.json";
            std::ofstream(policy) << "{}";
            bridge.start({std::filesystem::absolute(directory / "instances"), std::filesystem::absolute(policy), "test-build"});
        }
        asio::ip::tcp::socket connect()
        {
            asio::ip::tcp::socket socket(io);
            socket.connect({asio::ip::make_address_v4("127.0.0.1"), bridge.instance()->port});
            socket.non_blocking(true);
            return socket;
        }
        void pump(std::function<bool()> until, std::chrono::milliseconds timeout = 1000ms)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            do{
                bridge.pollBudget();
                if(until()){ return; }
                std::this_thread::sleep_for(1ms);
            }while(std::chrono::steady_clock::now() < deadline);
            REQUIRE(until());
        }
        void send(asio::ip::tcp::socket &socket, const Json &request)
        {
            const auto frame = encodeFrame(request, maxRequestBytes);
            asio::write(socket, asio::buffer(frame));
        }
        Json receive(asio::ip::tcp::socket &socket)
        {
            std::vector<uint8_t> bytes;
            size_t length = 0;
            pump([&]{
                std::array<uint8_t, 4096> buffer{};
                asio::error_code ec;
                const auto n = socket.read_some(asio::buffer(buffer), ec);
                if(!ec){ bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + n); }
                if(bytes.size() >= 4 && !length){ length = decodeLength(std::span<const uint8_t,4>(bytes.data(),4), maxResponseBytes); }
                return length && bytes.size() >= length + 4;
            });
            return strictParse(std::string_view(reinterpret_cast<const char *>(bytes.data() + 4), length));
        }
        Json hello() const
        {
            auto r = query("hello");
            r["params"] = {{"instance_id", bridge.instance()->instance_id}, {"token", bridge.instance()->token}};
            return r;
        }
        void authenticate(asio::ip::tcp::socket &s)
        {
            send(s, hello());
            const auto response = receive(s);
            validateDefinition(response, "helloResponse");
            REQUIRE(bridge.authenticated());
        }
        bool closed(asio::ip::tcp::socket &s)
        {
            std::array<char, 32> buf{};
            asio::error_code ec;
            s.read_some(asio::buffer(buf), ec);
            return ec == asio::error::eof || ec == asio::error::connection_reset || ec == asio::error::connection_aborted;
        }
    };
}

TEST_CASE("all 35 methods share closed request and response fixtures")
{
    const auto fixtures = readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "fixtures.json");
    REQUIRE(fixtures.size() == 35);
    REQUIRE(contract().at("x-methods").size() == 35);
    for(const auto &f: fixtures){
        const auto method = f.at("method").get<std::string>();
        INFO(method);
        CHECK(decodeRequest(f.at("request").dump()) == f.at("request"));
        CHECK_NOTHROW(validateDefinition(f.at("success"), method + "Response"));
        CHECK_NOTHROW(validateDefinition(f.at("failure"), method + "Response"));
        CHECK_THROWS_AS(decodeRequest(f.at("invalid").dump()), ProtocolError);
        auto wrong = f.at("request");
        wrong["extra"] = 1;
        CHECK_THROWS_AS(decodeRequest(wrong.dump()), ProtocolError);
        wrong = f.at("success");
        wrong["data"]["extra"] = 1;
        CHECK_THROWS_AS(validateDefinition(wrong, method + "Response"), ProtocolError);
    }
    for(const auto &op: readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "operation-fixtures.json")){
        CHECK_NOTHROW(validateDefinition(op, "operation"));
    }
}

TEST_CASE("strict JSON rejects ambiguous and unsafe inputs")
{
    for(const auto *s: {"", "{", "{\"v\":1,\"v\":2}", "{\"a\":{\"x\":1,\"x\":2}}", "{\"a\":NaN}", "{\"a\":1e9999}", "{} trailing", "[Infinity]"}){
        CHECK_THROWS_AS(strictParse(s), ProtocolError);
    }
    CHECK_THROWS_AS(strictParse(std::string("{\"a\":\"") + char(0xff) + "\"}"), ProtocolError);
    CHECK_THROWS_AS(strictParse(std::string(18,'[') + "0" + std::string(18,']')), ProtocolError);
    CHECK_THROWS_AS(strictParse(std::string(17,'[') + std::string(17,']')), ProtocolError);
    CHECK_NOTHROW(strictParse(std::string(16,'[') + std::string(16,']')));
    CHECK_THROWS_AS(strictParse(std::string("{}\0hidden", 9)), ProtocolError);
    CHECK_NOTHROW(strictParse("{\"a\":{\"x\":1},\"b\":{\"x\":2}}"));
    CHECK_THROWS_AS(decodeRequest(query("shell").dump()), ProtocolError);
    auto q = query(); q["v"] = 2;
    CHECK_THROWS_AS(decodeRequest(q.dump()), ProtocolError);
    q = query(); q["v"] = 1.0;
    CHECK_THROWS_AS(decodeRequest(q.dump()), ProtocolError);
    CHECK_THROWS_AS(decodeRequest(std::string(maxRequestBytes + 1, ' ')), ProtocolError);
}

TEST_CASE("contract preserves unknown ground and delivery data and real direction codes")
{
    const auto fixtures = readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "fixtures.json");
    for(const auto &f: fixtures){
        const auto method = f.at("method").get<std::string>();
        if(method == "game_get_state"){
            auto response = f.at("success");
            for(const auto direction: {1,8}){
                response["data"]["direction"] = direction;
                CHECK_NOTHROW(validateDefinition(response, method+"Response"));
            }
            response["data"]["direction"] = 0;
            CHECK_THROWS_AS(validateDefinition(response, method+"Response"),ProtocolError);
        }
        if(method == "game_move_to" || method == "game_attack"){
            auto request = f.at("request");
            auto explicitDefault = request;
            if(method == "game_move_to"){ explicitDefault["params"]["tolerance"] = 0; }
            else{ explicitDefault["params"]["mode"] = "once"; }
            CHECK(canonicalRequest(request) == canonicalRequest(explicitDefault));
        }
    }
    Json ground = {{"item_id",123},{"seq_id",nullptr},{"count",nullptr},{"name",nullptr},{"location","ground"}};
    CHECK_NOTHROW(validateDefinition(ground,"groundItem"));
    CHECK_THROWS_AS(validateDefinition(ground,"inventoryItem"),ProtocolError);
    Json delivery = {{"delivery_handle","opaque"},{"source","system"},{"auction_id",nullptr},{"text",""},{"claim_state","unclaimed"},{"items",nullptr},{"gold",nullptr}};
    CHECK_NOTHROW(validateDefinition(delivery,"delivery"));
    delivery["items"] = Json::array();
    CHECK_NOTHROW(validateDefinition(delivery,"delivery"));
    for(auto operation: readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "operation-fixtures.json")){
        if(operation["state"] == "succeeded"){
            operation["evidence"] = Json::array();
            CHECK_THROWS_AS(validateDefinition(operation,"operation"),ProtocolError);
        }
    }
    for(const auto &error: readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "error-fixtures.json")){
        CHECK_NOTHROW(validateDefinition(error,"transportError"));
    }
}

TEST_CASE("bounded integer strings targets and action context")
{
    auto fixtures = readJson(std::filesystem::path(MIR2X_AI_CONTRACT_DIR) / "fixtures.json");
    for(auto &f: fixtures){
        const auto method = f.at("method").get<std::string>();
        auto request = f.at("request");
        if(isAction(method)){
            CHECK(request.contains("context"));
            auto duplicate = request; duplicate["id"] = "different";
            CHECK(canonicalRequest(request) == canonicalRequest(duplicate));
            duplicate["timeout_ms"] = 0;
            CHECK_THROWS_AS(decodeRequest(duplicate.dump()), ProtocolError);
            duplicate["timeout_ms"] = 30001;
            CHECK_THROWS_AS(decodeRequest(duplicate.dump()), ProtocolError);
            duplicate = request; duplicate.erase("context");
            CHECK_THROWS_AS(decodeRequest(duplicate.dump()), ProtocolError);
            request["context"]["world_epoch"] = "18446744073709551615";
            CHECK_NOTHROW(decodeRequest(request.dump()));
            request["context"]["world_epoch"] = "18446744073709551616";
            CHECK_THROWS_AS(decodeRequest(request.dump()), ProtocolError);
            request["context"]["world_epoch"] = 9007199254740993ULL;
            CHECK_THROWS_AS(decodeRequest(request.dump()), ProtocolError);
        }
    }
    auto region = query("game_get_map_region");
    region["params"] = {{"x",0},{"y",0},{"width",33},{"height",33}};
    CHECK_NOTHROW(decodeRequest(region.dump()));
    region["params"]["width"] = 34;
    CHECK_THROWS_AS(decodeRequest(region.dump()), ProtocolError);
    region["params"]["width"] = -1;
    CHECK_THROWS_AS(decodeRequest(region.dump()), ProtocolError);
}

TEST_CASE("framing rejects zero oversize signed and preserves UTF8")
{
    for(auto bytes: {std::array<uint8_t,4>{0,0,0,0}, {0,1,0,1}, {255,255,255,255}}){
        CHECK_THROWS_AS(decodeLength(bytes), ProtocolError);
    }
    auto frame = encodeFrame(Json{{"text", "中文"}});
    CHECK(decodeLength(std::span<const uint8_t,4>(frame.data(),4)) == frame.size()-4);
    CHECK_THROWS_AS(encodeFrame(Json(std::string(maxResponseBytes,'x'))), ProtocolError);
}

TEST_CASE("transport disabled authenticated fragmented coalesced and stop priority")
{
    AiBridgeTransport disabled;
    CHECK(disabled.instance() == nullptr);
    CHECK(disabled.pollBudget() == 0);
    Fixture f;
    auto socket = f.connect();
    auto hello = encodeFrame(f.hello());
    asio::write(socket, asio::buffer(hello.data(), 2));
    f.bridge.pollBudget();
    CHECK_FALSE(f.bridge.authenticated());
    asio::write(socket, asio::buffer(hello.data()+2, hello.size()-2));
    CHECK(f.receive(socket).at("ok") == true);
    auto a = encodeFrame(query());
    auto b = encodeFrame(query("game_stop","stop"));
    a.insert(a.end(), b.begin(), b.end());
    asio::write(socket, asio::buffer(a));
    f.pump([&]{ return f.bridge.queuedRequests() == 2; });
    auto requests = f.bridge.takeRequests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].message.at("method") == "game_stop");
    CHECK(requests[1].message.at("method") == "game_get_session");
    CHECK_FALSE(f.bridge.sendResponse(requests[0].connectionId+1, errorResponse("stop", "paused")));
    CHECK(f.bridge.sendResponse(requests[0].connectionId, errorResponse("stop", "paused")));
    CHECK(f.receive(socket).at("error").at("code") == "paused");
    socket.close();
    f.pump([&]{return !f.bridge.authenticated();});
    CHECK(f.bridge.takeDisconnected() == std::vector<uint64_t>{requests[0].connectionId});
    CHECK(f.bridge.queuedRequests() == 0);
    const auto file = f.directory / "instances" / (f.bridge.instance()->instance_id + ".json");
    REQUIRE(std::filesystem::exists(file));
    f.bridge.close(); f.bridge.close();
    CHECK_FALSE(std::filesystem::exists(file));
}

TEST_CASE("authentication rejects wrong token instance version and extra connections")
{
    Fixture f;
    for(const auto *field: {"token", "instance_id", "v"}){
        auto s = f.connect();
        auto h = f.hello();
        if(std::string(field) == "v"){ h["v"] = 2; }
        else{ h["params"][field] = std::string(std::string(field)=="token" ? 64 : 32, 'f'); }
        f.send(s,h);
        f.pump([&]{return f.closed(s);});
        CHECK_FALSE(f.bridge.authenticated());
    }
    auto first = f.connect(); f.authenticate(first);
    auto second = f.connect(); f.send(second, f.hello());
    f.pump([&]{return f.closed(second);});
    CHECK(f.bridge.authenticated());
}

TEST_CASE("unauthenticated limits and handshake deadline")
{
    Fixture f;
    std::vector<asio::ip::tcp::socket> sockets;
    for(int i=0;i<5;++i){sockets.push_back(f.connect());}
    f.pump([&]{return f.closed(sockets.back());});
    CHECK_FALSE(f.bridge.authenticated());
    f.pump([&]{return f.closed(sockets.front());}, 3500ms);
    auto s = f.connect(); f.send(s,query());
    f.pump([&]{return f.closed(s);});
}

TEST_CASE("rate limiting and full queue still permit stop")
{
    Fixture f;
    auto s = f.connect(); f.authenticate(s);
    for(int batch=0;batch<4;++batch){
        for(int i=0;i<20;++i){f.send(s,query());}
        const size_t expected=std::min(size_t((batch+1)*20),size_t(64));
        f.pump([&]{return f.bridge.queuedRequests()==expected;});
        if(batch<3){std::this_thread::sleep_for(1010ms);}
    }
    f.send(s,query("game_stop"));
    f.pump([&]{return f.bridge.queuedRequests()==65;});
    const auto requests=f.bridge.takeRequests(100);
    REQUIRE(requests.size()==8);
    CHECK(requests.front().message.at("method")=="game_stop");
}

TEST_CASE("outbound backpressure disconnect and heartbeat deadline")
{
    Fixture f;
    auto s=f.connect();f.authenticate(s);
    f.send(s,query());f.pump([&]{return f.bridge.queuedRequests()==1;});
    auto request=f.bridge.takeRequests().front();
    Json large={{"data",std::string(700000,'x')}};
    CHECK(f.bridge.sendResponse(request.connectionId,large));
    CHECK(f.bridge.sendResponse(request.connectionId,large));
    CHECK_FALSE(f.bridge.sendResponse(request.connectionId,large));
    CHECK_FALSE(f.bridge.authenticated());
    CHECK(f.bridge.takeDisconnected().size()==1);
    CHECK(f.bridge.queuedOutputBytes()==0);
    auto next=f.connect();f.authenticate(next);
    f.send(next,query("heartbeat"));
    CHECK(f.receive(next).at("data").at("alive")==true);
    f.pump([&]{return !f.bridge.authenticated();},6500ms);
    CHECK(f.bridge.takeDisconnected().size()==1);
}

TEST_CASE("authenticated malformed frame is bounded and disconnect during write is safe")
{
    Fixture f;
    auto s=f.connect();f.authenticate(s);
    const std::array<uint8_t,4> oversize={0,1,0,1};
    asio::write(s,asio::buffer(oversize));
    f.pump([&]{return !f.bridge.authenticated();});
    CHECK(f.bridge.takeDisconnected().size()==1);
    auto next=f.connect();f.authenticate(next);
    f.send(next,query());f.pump([&]{return f.bridge.queuedRequests()==1;});
    const auto request=f.bridge.takeRequests().front();
    CHECK(f.bridge.sendResponse(request.connectionId,Json{{"data",std::string(1000000,'x')}}));
    next.set_option(asio::socket_base::linger(true,0));
    next.close();
    f.pump([&]{return !f.bridge.authenticated();});
    CHECK_FALSE(f.bridge.sendResponse(request.connectionId,errorResponse("query","paused")));
    CHECK(f.bridge.queuedOutputBytes()==0);
    CHECK(f.bridge.takeDisconnected().size()==1);
}

TEST_CASE("slow reader expires even while authenticated heartbeats continue")
{
    Fixture f;
    auto s=f.connect();
    s.set_option(asio::socket_base::receive_buffer_size(1024));
    f.authenticate(s);
    f.send(s,query());f.pump([&]{return f.bridge.queuedRequests()==1;});
    const auto request=f.bridge.takeRequests().front();
    CHECK(f.bridge.sendResponse(request.connectionId,Json{{"data",std::string(1000000,'x')}}));
    CHECK(f.bridge.sendResponse(request.connectionId,Json{{"data",std::string(1000000,'x')}}));
    auto last=std::chrono::steady_clock::now();
    size_t heartbeats=0;
    f.pump([&]{
        if(!f.bridge.authenticated()){return true;}
        if(std::chrono::steady_clock::now()-last>=1500ms){
            f.send(s,query("heartbeat"));
            last=std::chrono::steady_clock::now();
            ++heartbeats;
        }
        return false;
    },8500ms);
    CHECK(heartbeats>=3);
    CHECK(f.bridge.takeDisconnected().size()==1);
}

TEST_CASE("policy schema handles full allowlists and live instance metadata")
{
    Json values=Json::array();
    for(int i=0;i<200;++i){values.push_back(i);}
    CHECK_NOTHROW(validateDefinition(Json{{"allowed_item_ids",values}},"policy"));
    values.push_back(1);
    CHECK_THROWS_AS(validateDefinition(Json{{"allowed_item_ids",values}},"policy"),ProtocolError);
    Fixture f;
    const auto path=f.directory/"instances"/(f.bridge.instance()->instance_id+".json");
    auto metadata=readJson(path);
    CHECK_NOTHROW(validateDefinition(metadata,"instance"));
    metadata["build"]="";
    CHECK_THROWS_AS(validateDefinition(metadata,"instance"),ProtocolError);
    auto s=f.connect();f.authenticate(s);
    auto invalid=query();invalid["params"]["unknown"]=true;
    f.send(s,invalid);
    const auto error=f.receive(s);
    CHECK(error.at("id")==invalid.at("id"));
    CHECK(error.at("error").at("code")=="invalid_argument");
    CHECK_NOTHROW(validateDefinition(error,"transportError"));
}
