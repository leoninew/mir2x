#include "audit.hpp"
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#endif

namespace aibridge
{
    namespace
    {
        void checkPath(const std::filesystem::path &path)
        {
            std::filesystem::path prefix;
            for(const auto &part: path){
                prefix /= part;
                if(!prefix.has_root_directory()){ continue; }
#ifdef _WIN32
                const auto attributes = GetFileAttributesW(prefix.c_str());
                if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)){
                    throw std::runtime_error("AI audit path contains a reparse point");
                }
#endif
                if(std::filesystem::is_symlink(std::filesystem::symlink_status(prefix))){
                    throw std::runtime_error("AI audit path contains a symbolic link");
                }
            }
        }
        void checkPrivateDirectory(const std::filesystem::path &path)
        {
#ifdef _WIN32
            HANDLE token = nullptr;
            if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)){ throw std::runtime_error("AI audit identity unavailable"); }
            struct TokenGuard { HANDLE value; ~TokenGuard(){ CloseHandle(value); } } tokenGuard{token};
            DWORD size = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            std::vector<unsigned char> user(size);
            if(!GetTokenInformation(token, TokenUser, user.data(), size, &size)){ throw std::runtime_error("AI audit identity unavailable"); }
            alignas(SID) unsigned char system[SECURITY_MAX_SID_SIZE]{};
            DWORD systemSize = sizeof(system);
            if(!CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &systemSize)){ throw std::runtime_error("AI audit identity unavailable"); }
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            PACL acl = nullptr;
            if(GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor) != ERROR_SUCCESS){
                throw std::runtime_error("AI audit directory permissions unavailable");
            }
            struct DescriptorGuard { void *value; ~DescriptorGuard(){ LocalFree(value); } } descriptorGuard{descriptor};
            if(!acl || acl->AceCount == 0){ throw std::runtime_error("AI audit directory is not private"); }
            bool currentUser = false;
            for(DWORD i = 0; i < acl->AceCount; ++i){
                void *ace = nullptr;
                if(!GetAce(acl, i, &ace) || static_cast<ACE_HEADER *>(ace)->AceType != ACCESS_ALLOWED_ACE_TYPE){
                    throw std::runtime_error("AI audit directory is not private");
                }
                const auto allowed = static_cast<ACCESS_ALLOWED_ACE *>(ace);
                auto sid = reinterpret_cast<PSID>(&allowed->SidStart);
                const bool isUser = EqualSid(sid, reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid);
                if(!isUser && !EqualSid(sid, system)){ throw std::runtime_error("AI audit directory is not private"); }
                currentUser |= isUser && !(allowed->Header.AceFlags & INHERIT_ONLY_ACE);
            }
            if(!currentUser){ throw std::runtime_error("AI audit user access unavailable"); }
#else
            const auto permissions = std::filesystem::status(path).permissions();
            if((permissions & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none){
                throw std::runtime_error("AI audit directory is not private");
            }
#endif
        }
    }
    AuditLog::AuditLog(std::filesystem::path directory, uintmax_t limit)
        : m_directory(std::move(directory)), m_limit(limit)
    {
        if(m_directory.empty()){ return; }
        if(!m_directory.is_absolute() || limit == 0 || limit > fileLimit){
            throw std::runtime_error("invalid AI audit configuration");
        }
        m_directory = m_directory.lexically_normal();
        checkPath(m_directory);
        std::filesystem::create_directories(m_directory);
#ifndef _WIN32
        std::filesystem::permissions(m_directory, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
#endif
        checkPrivateDirectory(m_directory);
        for(size_t i = 0; i < 3; ++i){ checkFile(i); }
    }

    std::filesystem::path AuditLog::path(size_t index) const
    {
        return m_directory / (index ? "audit." + std::to_string(index) + ".jsonl" : "audit.jsonl");
    }

    void AuditLog::checkFile(size_t index) const
    {
        const auto entry = path(index);
        checkPath(entry);
        const auto status = std::filesystem::symlink_status(entry);
        if(std::filesystem::exists(status) && (!std::filesystem::is_regular_file(status)
            || std::filesystem::hard_link_count(entry) != 1)){
            throw std::runtime_error("invalid AI audit file");
        }
        if(std::filesystem::exists(status)){ checkPrivateDirectory(entry); }
    }

    void AuditLog::write(uint64_t time, std::string_view method, const Json &context,
        const OperationRecord *operation, std::string_view code)
    {
        if(m_directory.empty()){ return; }
        checkPath(m_directory);
        checkPrivateDirectory(m_directory);
        // Deliberate allowlist: never serialize params, input text, errors' messages, or credentials.
        Json record = {{"observed_at", std::to_string(time)}, {"method", method}, {"context", context}};
        if(operation){
            record["operation_id"] = operation->view.operationId;
            record["state"] = operation->state;
            record["evidence_types"] = Json::array();
            for(const auto &item: operation->evidence){ record["evidence_types"].push_back(item.at("kind")); }
            if(operation->error){ record["error_code"] = operation->error->code; }
        }
        if(!code.empty()){ record["error_code"] = code; }
        const auto line = record.dump() + "\n";
        if(line.size() > m_limit){ throw std::runtime_error("AI audit entry exceeds file limit"); }
        for(size_t i = 0; i < 3; ++i){ checkFile(i); }
        if(std::filesystem::exists(path(0)) && std::filesystem::file_size(path(0)) > m_limit - line.size()){
            std::filesystem::remove(path(2));
            if(std::filesystem::exists(path(1))){ std::filesystem::rename(path(1), path(2)); }
            std::filesystem::rename(path(0), path(1));
        }
        std::ofstream stream(path(0), std::ios::binary | std::ios::app);
#ifndef _WIN32
        std::filesystem::permissions(path(0), std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);
#endif
        stream << line;
        stream.flush();
        if(!stream){ throw std::runtime_error("AI audit write failed"); }
    }
}
