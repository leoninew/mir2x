addGuard('沙漠战士', 240, 166, DIR_DOWNLEFT)
addGuard('沙漠战士', 237, 163, DIR_DOWNLEFT)
addGuard('沙漠战士', 208, 167, DIR_DOWNRIGHT)
addGuard('沙漠战士', 204, 171, DIR_DOWNRIGHT)
addGuard('沙漠战士', 238, 192, DIR_DOWNLEFT)
addGuard('沙漠战士', 242, 196, DIR_DOWNLEFT)
addGuard('沙漠战士', 219, 206, DIR_UPRIGHT)
addGuard('沙漠战士', 197, 184, DIR_UPRIGHT)
addGuard('沙漠战士', 182, 202, DIR_DOWNLEFT)
addGuard('沙漠战士', 205, 224, DIR_DOWNLEFT)
addGuard('沙漠战士', 143, 269, DIR_DOWNLEFT)
addGuard('沙漠战士', 138, 265, DIR_DOWNLEFT)
addGuard('沙漠战士', 122, 273, DIR_DOWNLEFT)
addGuard('沙漠战士', 129, 280, DIR_DOWNLEFT)
addGuard('沙漠战士', 132, 198, DIR_UPLEFT)
addGuard('沙漠战士', 138, 192, DIR_UPLEFT)
addGuard('沙漠战士', 199, 279, DIR_DOWNRIGHT)
addGuard('沙漠战士', 207, 271, DIR_DOWNRIGHT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 沙漠土城_5
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
