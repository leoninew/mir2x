#pragma once
#include "economy.hpp"
#include "client.hpp"
#include "servermsg.hpp"

namespace aibridge
{
    // The network callback owns only a receipt. GUI settlement happens on its normal update path.
    template<typename ErrorMessage>
    std::shared_ptr<EconomicRequest> sendEconomic(Client &client, const ClientMsgBuf &message,
        uint8_t errorHead, uint64_t timeoutMs = 30000)
    {
        auto request = std::make_shared<EconomicRequest>();
        try{
            request->submitted = true;
            client.sendScoped(message, timeoutMs,
                [request,errorHead](uint8_t head, const uint8_t *data, size_t size)
                {
                    request->submitted = true;
                    if(head == SM_OK){ request->receipt = EconomicReceipt::Ok; }
                    else if(head == errorHead && size == sizeof(ErrorMessage)){
                        request->error = ServerMsg::conv<ErrorMessage>(data,size).error;
                        request->receipt = EconomicReceipt::Rejected;
                    }
                    else{ request->receipt = EconomicReceipt::Unknown; }
                },
                [request](ResponseCallbacks::CancelReason reason)
                {
                    request->submitted = reason != ResponseCallbacks::CancelReason::SendFailed;
                    request->receipt = EconomicReceipt::Unknown;
                });
            request->submitted = true;
        }
        catch(...){ request->receipt = EconomicReceipt::Unknown; }
        return request;
    }
}
