addGuard('禁军卫士', 285, 268, DIR_UPLEFT)
addGuard('禁军卫士', 275, 278, DIR_UPLEFT)
addGuard('禁军卫士', 353, 309, DIR_DOWNRIGHT)
addGuard('禁军卫士', 346, 316, DIR_DOWNRIGHT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 盟重县_74
getMapSpawnConfig())

function main()
    while true do
        local rc, errMsg = coroutine.resume(addMonCo)
        if not rc then
            fatalPrintf('addMonCo failed: %s', argDefault(errMsg, 'unknown error'))
        end
        pause(1000 * 5)
    end
end
