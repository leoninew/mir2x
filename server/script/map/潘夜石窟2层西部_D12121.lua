local addmon = require('map.addmonster')
local addMonCo = addmon.monGener( -- 潘夜石窟2层西部_D12121
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
