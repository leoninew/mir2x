#include <catch2/catch_test_macros.hpp>
#include "sysconst.hpp"
#include "protocoldef.hpp"
#include "sdruntimeconfig.hpp"

TEST_CASE("Continuous attack retains the legacy configuration key and default", "[sdruntimeconfig]")
{
    const auto legacyKey = cerealf::serialize(std::tuple<int>(23), 0);
    REQUIRE(RTCFG_持续攻击 == 23);
    REQUIRE(SDRuntimeConfigAccessor<RTCFG_持续攻击>::keyString() == legacyKey);

    SDRuntimeConfig config;
    REQUIRE(SDRuntimeConfig_getConfig<RTCFG_持续攻击>(config));
    REQUIRE_FALSE(config.getConfig(legacyKey).has_value());

    config.setConfig(legacyKey, cerealf::serialize<bool>(false));
    REQUIRE_FALSE(SDRuntimeConfig_getConfig<RTCFG_持续攻击>(config));

    const auto restored = cerealf::deserialize<SDRuntimeConfig>(cerealf::serialize(config));
    REQUIRE_FALSE(SDRuntimeConfig_getConfig<RTCFG_持续攻击>(restored));
    REQUIRE(SDRuntimeConfig_getConfig<RTCFG_显示体力变化>(restored));

    REQUIRE(SDRuntimeConfig_setConfig<RTCFG_持续攻击>(config, true));
    REQUIRE(SDRuntimeConfig_getConfig<RTCFG_持续攻击>(config));
    REQUIRE_FALSE(config.getConfig(legacyKey).has_value());
    REQUIRE_FALSE(SDRuntimeConfig_setConfig<RTCFG_持续攻击>(config, true));

    REQUIRE(SDRuntimeConfig_setConfig<RTCFG_持续攻击>(config, false));
    REQUIRE_FALSE(SDRuntimeConfig_getConfig<RTCFG_持续攻击>(config));
    REQUIRE(config.getConfig(legacyKey).has_value());
}
