addGuard('禁军卫士', 311, 216, DIR_DOWNLEFT)
addGuard('禁军卫士', 310, 228, DIR_DOWNLEFT)
addGuard('禁军卫士', 364, 202, DIR_DOWNLEFT)
addGuard('禁军卫士', 366, 204, DIR_DOWNLEFT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 毒蛇山谷_2
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
