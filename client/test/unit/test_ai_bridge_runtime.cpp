#include <catch2/catch_test_macros.hpp>
#include "../../src/aibridge/runtime.hpp"
#include "../../src/aibridge/runtime.intent.hpp"
#include "../../src/clientargparser.hpp"

namespace
{
    constexpr SDL_FRect enableRect{28, 90, 100, 15};
    constexpr SDL_FRect pauseRect{180, 90, 70, 15};
    using Gate = aibridge::ControlGesture;

    SDL_Event button(Uint32 type, float x, float y, Uint8 which = SDL_BUTTON_LEFT, SDL_WindowID window = 1)
    {
        SDL_Event e{};
        e.type = type;
        e.button.x = x;
        e.button.y = y;
        e.button.button = which;
        e.button.windowID = window;
        return e;
    }
}

TEST_CASE("Client bridge paths preserve UTF-8 command line arguments")
{
    const auto root = std::filesystem::current_path() / std::filesystem::path(u8"\u4E2D\u6587 space");
    const auto directory = root / "bridge";
    const auto policy = root / "policy.json";
    const auto utf8 = [](const std::filesystem::path &path)
    {
        const auto text = path.u8string();
        return std::string(text.begin(), text.end());
    };
    std::vector<std::string> values{
        "client", "--enable-ai-bridge", "--ai-bridge-dir=" + utf8(directory),
        "--ai-policy=" + utf8(policy),
    };
    std::vector<char *> argv;
    for(auto &value: values){ argv.push_back(value.data()); }
    argv.push_back(nullptr);
    const argf::parser parser(static_cast<int>(values.size()), argv.data());
    const ClientArgParser parsed(parser);
    REQUIRE(parsed.aiBridgeDir == directory);
    REQUIRE(parsed.aiPolicy == policy);
}

TEST_CASE("Only a complete control button gesture is exempt from manual takeover")
{
    Gate gate;
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Consumed);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Enable);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 181, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Consumed);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 181, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Pause);
}

TEST_CASE("Only the main game window may affect AI control")
{
    REQUIRE(Gate::gameWindowEvent(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92, SDL_BUTTON_LEFT, 7), 7));
    REQUIRE_FALSE(Gate::gameWindowEvent(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92, SDL_BUTTON_LEFT, 8), 7));
    REQUIRE_FALSE(Gate::gameWindowEvent(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92, SDL_BUTTON_LEFT, 8), 7));
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = 8;
    REQUIRE_FALSE(Gate::gameWindowEvent(key, 7));
    SDL_Event text{};
    text.type = SDL_EVENT_TEXT_INPUT;
    text.text.windowID = 7;
    REQUIRE(Gate::gameWindowEvent(text, 7));
    REQUIRE_FALSE(Gate::gameWindowEvent(text, 0));
}

TEST_CASE("Panel background and right clicks still reach the original game event path")
{
    Gate gate;
    for(const auto &e: {button(SDL_EVENT_MOUSE_BUTTON_DOWN, 20, 20), button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92, SDL_BUTTON_RIGHT)}){
        REQUIRE(gate.route(e, enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
        REQUIRE(Gate::manual(e));
    }
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 128, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 105), enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 127.9f, 104.9f), enableRect, pauseRect, "epoch1") == Gate::Result::Consumed);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 128, 104), enableRect, pauseRect, "epoch1") == Gate::Result::Consumed);
}

TEST_CASE("Control press cannot grant another window context or release target")
{
    for(const auto &release: {button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92, SDL_BUTTON_LEFT, 2), button(SDL_EVENT_MOUSE_BUTTON_UP, 181, 92), button(SDL_EVENT_MOUSE_BUTTON_UP, 5, 5)}){
        Gate gate;
        gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1");
        REQUIRE(gate.route(release, enableRect, pauseRect, "epoch1") == Gate::Result::Consumed);
    }
    Gate gate;
    gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1");
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92), enableRect, pauseRect, "epoch2") == Gate::Result::Consumed);
}

TEST_CASE("Mouse wheel keys and text cancel a pending enable before reaching the game")
{
    for(const auto type: {SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_EDITING}){
        Gate gate;
        gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1");
        SDL_Event e{};
        e.type = type;
        REQUIRE(Gate::manual(e));
        REQUIRE(gate.route(e, enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
        REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    }
}

TEST_CASE("Movement release redraw focus and key repeat never request manual takeover")
{
    for(const auto type: {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_KEY_UP, SDL_EVENT_WINDOW_EXPOSED,
                         SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_FOCUS_GAINED}){
        SDL_Event e{};
        e.type = type;
        REQUIRE_FALSE(Gate::manual(e));
    }
    SDL_Event repeated{};
    repeated.type = SDL_EVENT_KEY_DOWN;
    repeated.key.repeat = true;
    REQUIRE_FALSE(Gate::manual(repeated));
}

TEST_CASE("Focus loss cancels only pending button grant and does not pause control")
{
    Gate gate;
    gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1");
    SDL_Event lost{};
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    REQUIRE_FALSE(Gate::manual(lost));
    REQUIRE(gate.route(lost, enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 30, 92), enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
}

TEST_CASE("Motion during a control gesture retains its matching release")
{
    Gate gate;
    gate.route(button(SDL_EVENT_MOUSE_BUTTON_DOWN, 30, 92), enableRect, pauseRect, "epoch1");
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 31;
    motion.motion.y = 93;
    REQUIRE(gate.route(motion, enableRect, pauseRect, "epoch1") == Gate::Result::Pass);
    REQUIRE(gate.route(button(SDL_EVENT_MOUSE_BUTTON_UP, 31, 93), enableRect, pauseRect, "epoch1") == Gate::Result::Enable);
}

TEST_CASE("Player input invalidates an accepted action before advance without swallowing input")
{
    using namespace aibridge;
    ControllerOptions options;
    options.instanceId = std::string(32, 'a');
    options.build = "runtime-input-test";
    options.policy = Policy::parse(R"({"allowed_actions":"*"})");
    options.monotonicMs = []{ return 1000; };
    options.unixMs = []{ return 1000; };
    options.newSessionId = []{ return std::string(32, 'b'); };
    AiBridgeController controller(std::move(options));
    size_t sends = 0;
    ActionAdapter adapter;
    adapter.validate = [](const Json &, const Json &){ return ValidationResult{}; };
    adapter.begin = [&](const OperationView &){ ++sends; return OperationUpdate{}; };
    adapter.tick = [](const OperationView &)->std::optional<OperationUpdate>{ return {}; };
    adapter.cancel = [](const OperationView &){ return OperationUpdate{.outcome=OperationOutcome::Cancelled}; };
    controller.registerAction("game_move_to", std::move(adapter));
    controller.onConnected(1);
    controller.observe({"run", 100, 11, true});
    REQUIRE(controller.enableByUser());
    Json request = {{"v",1}, {"id","move"}, {"method","game_move_to"}, {"params",{{"x",10},{"y",20}}},
                    {"request_id","00000000-0000-4000-8000-000000000001"}, {"context",controller.context()}, {"timeout_ms",1000}};
    REQUIRE(controller.handleRequest({1,request}).at("ok") == true);
    Gate gate;
    const auto e = button(SDL_EVENT_MOUSE_BUTTON_DOWN, 500, 500);
    REQUIRE(gate.route(e, enableRect, pauseRect, controller.context().dump()) == Gate::Result::Pass);
    REQUIRE(Gate::manual(e));
    controller.manualInput();
    size_t originalGameEvents = 0;
    if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.x == 500){ ++originalGameEvents; }
    controller.advance();
    REQUIRE(sends == 0);
    REQUIRE(originalGameEvents == 1);
    REQUIRE(controller.controlState() == ControlState::Paused);
    REQUIRE(controller.handleRequest({1,request}).at("ok") == true); // Retry queries the cancelled ledger entry.
    controller.advance();
    REQUIRE(sends == 0);
}

TEST_CASE("AI stop invalidates old continuation but preserves subsequent local player actions")
{
    aibridge::IntentOwnership owner;
    const auto ordinaryBeforeAi = owner.stamp(false);
    REQUIRE(owner.permits(ordinaryBeforeAi));
    REQUIRE_FALSE(owner.cancel()); // Read-only/manual-only input cannot cancel a player queue.
    REQUIRE(owner.permits(ordinaryBeforeAi));
    owner.begin();
    const auto aiAttack = owner.stamp(true);
    REQUIRE(owner.permits(aiAttack));
    REQUIRE_FALSE(owner.permits(owner.stamp(false))); // A server echo does not install a new local repeat.
    REQUIRE(owner.cancel());
    REQUIRE_FALSE(owner.permits(aiAttack));
    const auto playerAttack = owner.stamp(true);
    const auto playerPickup = owner.stamp(true);
    REQUIRE(owner.permits(playerAttack));
    REQUIRE(owner.permits(playerPickup));
    REQUIRE(owner.permits(owner.stamp(false, true))); // Server correction of the current manual intention.
    REQUIRE_FALSE(owner.permits(owner.stamp(false, false))); // Unrelated late AI echo.
    REQUIRE_FALSE(owner.cancel());
    REQUIRE(owner.permits(playerAttack));
    owner.begin();
    REQUIRE_FALSE(owner.permits(aiAttack)); // Enabling a new generation never resurrects an old callback.
    REQUIRE_FALSE(owner.permits(playerAttack));
    REQUIRE_FALSE(owner.permits(owner.stamp(false, true))); // Active AI cannot gain a fresh generation from a server echo.
    REQUIRE(owner.permits(owner.stamp(true)));
}
