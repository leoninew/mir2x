addGuard('大刀卫士', 426, 283, DIR_DOWNLEFT)
addGuard('大刀卫士', 413, 272, DIR_DOWNRIGHT)
addGuard('大刀卫士', 431, 240, DIR_UPLEFT)
addGuard('大刀卫士', 427, 244, DIR_UPLEFT)
addGuard('大刀卫士', 462, 241, DIR_UPRIGHT)
addGuard('大刀卫士', 455, 232, DIR_UPRIGHT)
addGuard('大刀卫士', 441, 325, DIR_DOWN)
addGuard('大刀卫士', 451, 316, DIR_DOWNLEFT)
addGuard('大刀卫士', 427, 320, DIR_DOWNRIGHT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 边境城市_01
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
