local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 东部蚂蚁洞穴1层4_D8201
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
