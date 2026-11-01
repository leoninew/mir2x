#pragma once
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

class ResponseCallbacks final
{
    public:
        enum class CancelReason { Timeout, ProcessChanged, Shutdown, SendFailed };
        using Handler = std::function<void(uint8_t, const uint8_t *, size_t)>;
        using OnCancel = std::function<void(CancelReason)>;

    private:
        struct Entry
        {
            Handler handler;
            std::optional<uint64_t> deadline;
            OnCancel onCancel;
        };

        uint64_t m_nextID;
        bool m_exhausted = false;
        bool m_cancelling = false;
        std::unordered_map<uint64_t, Entry> m_entries;

    public:
        explicit ResponseCallbacks(uint64_t firstID = 1)
            : m_nextID(firstID)
        {
            if(!firstID){
                throw std::invalid_argument("Response ID zero is reserved");
            }
        }

        uint64_t add(Handler handler, std::optional<uint64_t> deadline = {}, OnCancel onCancel = {})
        {
            if(!handler){
                throw std::invalid_argument("Missing response handler");
            }
            if(m_cancelling){
                throw std::logic_error("Response scope is closing");
            }
            if(m_exhausted){
                throw std::overflow_error("Response IDs exhausted");
            }
            const auto id = m_nextID;
            m_entries.emplace(id, Entry{std::move(handler), deadline, std::move(onCancel)});
            if(id == std::numeric_limits<uint64_t>::max()){
                m_exhausted = true;
            }
            else{
                ++m_nextID;
            }
            return id;
        }

        bool dispatch(uint64_t id, uint8_t headCode, const uint8_t *data, size_t size, std::optional<uint64_t> now = {})
        {
            auto node = m_entries.extract(id);
            if(node.empty()){
                return false;
            }
            if(now && node.mapped().deadline && *now >= *node.mapped().deadline){
                if(auto &notify = node.mapped().onCancel; notify){
                    notify(CancelReason::Timeout);
                }
                return false;
            }
            // Remove before invoking: a handler can register or cancel other requests.
            auto handler = std::move(node.mapped().handler);
            handler(headCode, data, size);
            return true;
        }

        bool cancel(uint64_t id, CancelReason reason)
        {
            auto node = m_entries.extract(id);
            if(node.empty()){
                return false;
            }
            if(auto &notify = node.mapped().onCancel; notify){
                notify(reason);
            }
            return true;
        }

        void expire(uint64_t now)
        {
            std::vector<uint64_t> expired;
            for(const auto &[id, entry]: m_entries){
                if(entry.deadline && now >= *entry.deadline){
                    expired.push_back(id);
                }
            }
            std::exception_ptr error;
            for(const auto id: expired){
                try{
                    cancel(id, CancelReason::Timeout);
                }
                catch(...){
                    if(!error){
                        error = std::current_exception();
                    }
                }
            }
            if(error){
                std::rethrow_exception(error);
            }
        }

        void cancelAll(CancelReason reason)
        {
            const bool wasCancelling = std::exchange(m_cancelling, true);
            auto entries = std::exchange(m_entries, {});
            std::exception_ptr error;
            for(auto &[id, entry]: entries){
                if(entry.onCancel){
                    try{
                        entry.onCancel(reason);
                    }
                    catch(...){
                        if(!error){
                            error = std::current_exception();
                        }
                    }
                }
            }
            m_cancelling = wasCancelling;
            if(error){
                std::rethrow_exception(error);
            }
        }

        size_t size() const noexcept { return m_entries.size(); }
};
