addGuard('禁军卫士', 457, 386, DIR_DOWNLEFT)
addGuard('禁军卫士', 454, 383, DIR_DOWNLEFT)
addGuard('禁军卫士', 471, 371, DIR_DOWNLEFT)
addGuard('禁军卫士', 467, 367, DIR_DOWNLEFT)
addGuard('禁军卫士', 495, 387, DIR_DOWNLEFT)
addGuard('禁军卫士', 499, 391, DIR_DOWNLEFT)
addGuard('禁军卫士', 511, 369, DIR_UPRIGHT)
addGuard('禁军卫士', 515, 373, DIR_UPRIGHT)
addGuard('禁军卫士', 497, 344, DIR_UPRIGHT)
addGuard('禁军卫士', 492, 339, DIR_UPRIGHT)
addGuard('禁军卫士', 511, 325, DIR_UPRIGHT)
addGuard('禁军卫士', 515, 329, DIR_UPRIGHT)
addGuard('禁军卫士', 466, 325, DIR_UPRIGHT)
addGuard('禁军卫士', 461, 320, DIR_UPRIGHT)
addGuard('禁军卫士', 451, 342, DIR_DOWNLEFT)
addGuard('禁军卫士', 448, 339, DIR_DOWNLEFT)
addGuard('禁军卫士', 406, 343, DIR_DOWNLEFT)
addGuard('禁军卫士', 389, 328, DIR_UPLEFT)
addGuard('禁军卫士', 387, 330, DIR_UPLEFT)
addGuard('禁军卫士', 412, 426, DIR_UPLEFT)
addGuard('禁军卫士', 403, 443, DIR_DOWNLEFT)
addGuard('禁军卫士', 396, 436, DIR_DOWNLEFT)
addGuard('禁军卫士', 393, 449, DIR_DOWNLEFT)
addGuard('禁军卫士', 391, 447, DIR_DOWNLEFT)
addGuard('禁军卫士', 507, 448, DIR_DOWNRIGHT)
addGuard('禁军卫士', 503, 452, DIR_DOWNRIGHT)
addGuard('禁军卫士', 483, 410, DIR_DOWNLEFT)

local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 比奇县_0
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
