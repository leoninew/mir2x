addGuard('沙漠战士', 451, 54, DIR_DOWNLEFT)
addGuard('沙漠战士', 457, 60, DIR_DOWNLEFT)
addGuard('沙漠战士', 462, 48, DIR_UPRIGHT)
addGuard('沙漠战士', 456, 46, DIR_UPRIGHT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 绿洲_4
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
