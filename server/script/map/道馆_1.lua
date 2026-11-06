addGuard('大刀卫士', 371, 160, DIR_DOWNLEFT)
addGuard('大刀卫士', 375, 164, DIR_DOWNLEFT)
addGuard('大刀卫士', 368, 112, DIR_UPLEFT)
addGuard('大刀卫士', 372, 108, DIR_UPLEFT)
addGuard('大刀卫士', 414, 166, DIR_DOWNRIGHT)
addGuard('大刀卫士', 417, 163, DIR_DOWNRIGHT)
addGuard('大刀卫士', 411, 115, DIR_DOWNLEFT)
addGuard('大刀卫士', 414, 118, DIR_DOWNLEFT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 道馆_1
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
