#pragma once
#include "operation.hpp"
#include <filesystem>

namespace aibridge
{
    class AuditLog
    {
        public:
            static constexpr uintmax_t fileLimit = 5 * 1024 * 1024;
            explicit AuditLog(std::filesystem::path directory = {}, uintmax_t limit = fileLimit);
            void write(uint64_t timestamp, std::string_view method, const Json &context,
                const OperationRecord *operation = nullptr, std::string_view errorCode = {});
        private:
            std::filesystem::path m_directory;
            uintmax_t m_limit;
            std::filesystem::path path(size_t index) const;
            void checkFile(size_t index) const;
    };
}
