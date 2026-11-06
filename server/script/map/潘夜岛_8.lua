addGuard('大刀卫士', 269, 292, DIR_DOWNRIGHT)
addGuard('大刀卫士', 260, 302, DIR_DOWNRIGHT)
addGuard('大刀卫士', 234, 293, DIR_UPRIGHT)
addGuard('大刀卫士', 230, 288, DIR_UPRIGHT)
addGuard('大刀卫士', 212, 254, DIR_UPLEFT)
addGuard('大刀卫士', 232, 231, DIR_DOWNLEFT)
addGuard('大刀卫士', 218, 206, DIR_DOWNLEFT)
addGuard('大刀卫士', 223, 200, DIR_UPLEFT)
addGuard('大刀卫士', 244, 196, DIR_UPRIGHT)
addGuard('大刀卫士', 262, 204, DIR_DOWNRIGHT)
addGuard('大刀卫士', 279, 206, DIR_UPRIGHT)
addGuard('大刀卫士', 281, 225, DIR_DOWNRIGHT)
addGuard('大刀卫士', 271, 263, DIR_DOWNRIGHT)
addGuard('大刀卫士', 263, 269, DIR_UPRIGHT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 潘夜岛_8
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
