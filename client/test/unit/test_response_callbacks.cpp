#include <catch2/catch_test_macros.hpp>
#include <limits>
#include "../../src/responsecallbacks.hpp"

namespace
{
    void ignoreResponse(uint8_t, const uint8_t *, size_t) {}
}

TEST_CASE("Response IDs are never reused after completing or clearing a scope")
{
    ResponseCallbacks callbacks;
    const auto first = callbacks.add(ignoreResponse);
    REQUIRE(callbacks.dispatch(first, 1, nullptr, 0));
    const auto second = callbacks.add(ignoreResponse);
    callbacks.cancelAll(ResponseCallbacks::CancelReason::ProcessChanged);
    const auto third = callbacks.add(ignoreResponse);
    REQUIRE(first < second);
    REQUIRE(second < third);
    REQUIRE_FALSE(callbacks.dispatch(first, 1, nullptr, 0));
    REQUIRE_FALSE(callbacks.dispatch(second, 1, nullptr, 0));
    REQUIRE(callbacks.dispatch(third, 1, nullptr, 0));
}

TEST_CASE("Response dispatch permits reentrant registration and scope cancellation")
{
    ResponseCallbacks callbacks;
    uint64_t next = 0;
    const auto id = callbacks.add([&](uint8_t code, const uint8_t *data, size_t size)
    {
        REQUIRE(code == 7);
        REQUIRE(size == 1);
        REQUIRE(*data == 42);
        REQUIRE(callbacks.size() == 0);
        for(int i = 0; i < 1000; ++i){
            callbacks.add(ignoreResponse);
        }
        callbacks.cancelAll(ResponseCallbacks::CancelReason::ProcessChanged);
        next = callbacks.add(ignoreResponse);
    });
    const uint8_t value = 42;
    REQUIRE(callbacks.dispatch(id, 7, &value, 1));
    REQUIRE(callbacks.size() == 1);
    REQUIRE_FALSE(callbacks.dispatch(id, 7, &value, 1));
    REQUIRE(callbacks.dispatch(next, 7, &value, 1));
}

TEST_CASE("Expired scoped responses cancel once and late replies are ignored")
{
    ResponseCallbacks callbacks;
    int timeouts = 0;
    int responses = 0;
    const auto id = callbacks.add([&](auto, auto, auto){ ++responses; }, 500,
        [&](auto reason)
        {
            REQUIRE(reason == ResponseCallbacks::CancelReason::Timeout);
            ++timeouts;
        });
    callbacks.expire(499);
    REQUIRE(timeouts == 0);
    callbacks.expire(500);
    callbacks.expire(501);
    callbacks.cancelAll(ResponseCallbacks::CancelReason::Shutdown);
    REQUIRE(timeouts == 1);
    REQUIRE_FALSE(callbacks.dispatch(id, 0, nullptr, 0));
    REQUIRE(responses == 0);
}

TEST_CASE("Completed and unscoped requests are not expired")
{
    ResponseCallbacks callbacks;
    int cancelled = 0;
    const auto timed = callbacks.add(ignoreResponse, 10, [&](auto){ ++cancelled; });
    const auto untimed = callbacks.add(ignoreResponse, {}, [&](auto){ ++cancelled; });
    REQUIRE(callbacks.dispatch(timed, 0, nullptr, 0));
    callbacks.expire(std::numeric_limits<uint64_t>::max());
    REQUIRE(cancelled == 0);
    REQUIRE(callbacks.dispatch(untimed, 0, nullptr, 0));
}

TEST_CASE("A late reply cannot beat the deadline by arriving before expiry polling")
{
    ResponseCallbacks callbacks;
    int timeouts = 0;
    const auto id = callbacks.add([](auto, auto, auto){ FAIL("Late response executed"); }, 100,
        [&](auto reason)
        {
            REQUIRE(reason == ResponseCallbacks::CancelReason::Timeout);
            ++timeouts;
        });
    REQUIRE_FALSE(callbacks.dispatch(id, 0, nullptr, 0, 100));
    callbacks.expire(101);
    REQUIRE(timeouts == 1);
    REQUIRE(callbacks.size() == 0);
}

TEST_CASE("Closing a scope rejects reentrant additions and cancels all observers")
{
    ResponseCallbacks callbacks;
    int cancelled = 0;
    for(int i = 0; i < 3; ++i){
        callbacks.add(ignoreResponse, {}, [&](auto reason)
        {
            REQUIRE(reason == ResponseCallbacks::CancelReason::ProcessChanged);
            ++cancelled;
            callbacks.cancelAll(reason);
            REQUIRE_THROWS_AS(callbacks.add(ignoreResponse), std::logic_error);
        });
    }
    callbacks.cancelAll(ResponseCallbacks::CancelReason::ProcessChanged);
    REQUIRE(cancelled == 3);
    REQUIRE(callbacks.size() == 0);
    REQUIRE_NOTHROW(callbacks.add(ignoreResponse));
}

TEST_CASE("Cancellation remains consistent if a notification throws")
{
    ResponseCallbacks callbacks;
    int cancelled = 0;
    for(int i = 0; i < 3; ++i){
        callbacks.add(ignoreResponse, 5, [&](auto)
        {
            ++cancelled;
            throw std::runtime_error("test observer error");
        });
    }
    SECTION("scope cancellation")
    {
        REQUIRE_THROWS_AS(callbacks.cancelAll(ResponseCallbacks::CancelReason::Shutdown), std::runtime_error);
    }
    SECTION("timeout")
    {
        REQUIRE_THROWS_AS(callbacks.expire(5), std::runtime_error);
    }
    REQUIRE(cancelled == 3);
    REQUIRE(callbacks.size() == 0);
    REQUIRE_NOTHROW(callbacks.add(ignoreResponse));
}

TEST_CASE("Send failure removes only the failed response")
{
    ResponseCallbacks callbacks;
    int cancelled = 0;
    const auto id = callbacks.add(ignoreResponse, 100, [&](auto reason)
    {
        REQUIRE(reason == ResponseCallbacks::CancelReason::SendFailed);
        ++cancelled;
    });
    const auto other = callbacks.add(ignoreResponse);
    REQUIRE(callbacks.cancel(id, ResponseCallbacks::CancelReason::SendFailed));
    REQUIRE_FALSE(callbacks.cancel(id, ResponseCallbacks::CancelReason::SendFailed));
    REQUIRE_FALSE(callbacks.dispatch(id, 0, nullptr, 0));
    REQUIRE(callbacks.dispatch(other, 0, nullptr, 0));
    REQUIRE(cancelled == 1);
}

TEST_CASE("Exhausted response IDs fail instead of wrapping")
{
    ResponseCallbacks callbacks(std::numeric_limits<uint64_t>::max());
    const auto id = callbacks.add(ignoreResponse);
    REQUIRE(id == std::numeric_limits<uint64_t>::max());
    REQUIRE(callbacks.dispatch(id, 0, nullptr, 0));
    REQUIRE_THROWS_AS(callbacks.add(ignoreResponse), std::overflow_error);
    REQUIRE_THROWS_AS(ResponseCallbacks(0), std::invalid_argument);
    ResponseCallbacks ordinary;
    REQUIRE_THROWS_AS(ordinary.add({}), std::invalid_argument);
}
