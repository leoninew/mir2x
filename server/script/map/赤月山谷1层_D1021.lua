local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 赤月山谷1层_D1021
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
