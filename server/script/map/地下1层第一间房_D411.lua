local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 地下1层第一间房_D411
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
