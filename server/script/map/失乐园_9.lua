addGuard('昂克战士', 177, 519, DIR_UPLEFT)
addGuard('昂克战士', 174, 522, DIR_UPLEFT)
addGuard('昂克战士', 238, 524, DIR_UPRIGHT)
addGuard('昂克战士', 242, 527, DIR_UPRIGHT)
addGuard('昂克战士', 246, 606, DIR_DOWNRIGHT)
addGuard('昂克战士', 249, 602, DIR_DOWNRIGHT)
addGuard('昂克战士', 254, 553, DIR_DOWNLEFT)
addGuard('昂克战士', 258, 556, DIR_DOWNLEFT)
addGuard('昂克战士', 266, 562, DIR_DOWNLEFT)
addGuard('昂克战士', 250, 548, DIR_DOWNLEFT)
addGuard('昂克战士', 168, 589, DIR_UPLEFT)
addGuard('昂克战士', 163, 539, DIR_UP)
addGuard('昂克战士', 213, 513, DIR_UPRIGHT)
addGuard('昂克战士', 209, 643, DIR_DOWN)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 失乐园_9
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
