addGuard('大刀卫士', 232, 180, DIR_UPLEFT)
addGuard('大刀卫士', 228, 184, DIR_UPLEFT)
addGuard('大刀卫士', 244, 168, DIR_UPLEFT)
addGuard('大刀卫士', 247, 165, DIR_UPLEFT)
addGuard('大刀卫士', 274, 167, DIR_UPRIGHT)
addGuard('大刀卫士', 277, 170, DIR_UPRIGHT)
addGuard('大刀卫士', 283, 176, DIR_UPRIGHT)
addGuard('大刀卫士', 287, 180, DIR_UPRIGHT)
addGuard('大刀卫士', 289, 205, DIR_DOWNRIGHT)
addGuard('大刀卫士', 286, 208, DIR_DOWNRIGHT)
addGuard('大刀卫士', 277, 233, DIR_DOWNRIGHT)
addGuard('大刀卫士', 274, 236, DIR_DOWNRIGHT)
addGuard('大刀卫士', 229, 239, DIR_DOWNLEFT)
addGuard('大刀卫士', 227, 237, DIR_DOWNLEFT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 银杏山谷_02
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
