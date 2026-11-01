#include "instancefile.hpp"

#include <array>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    [[noreturn]] void securityError()
    {
        throw std::runtime_error("AI bridge instance security check failed");
    }

    void checkPath(const std::filesystem::path &path)
    {
        if(!path.is_absolute() || path == path.root_path()){
            securityError();
        }
        for(const auto &part: path.relative_path()){
            if(part == "." || part == ".." || part.empty()){
                securityError();
            }
            const auto &text = part.native();
            if(text.find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos){
                securityError();
            }
#ifdef _WIN32
            if(text.find(L':') != std::wstring::npos || text.back() == L'.' || text.back() == L' '){
                securityError();
            }
#endif
        }
#ifdef _WIN32
        // Credentials must remain on a local drive, never a UNC share/device path.
        const auto root = path.root_name().native();
        if(root.size() != 2 || root[1] != L':' || !((root[0] >= L'A' && root[0] <= L'Z') || (root[0] >= L'a' && root[0] <= L'z'))){
            securityError();
        }
        const auto drive = GetDriveTypeW(path.root_path().c_str());
        if(drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_RAMDISK){
            securityError();
        }
#endif
    }

#ifdef _WIN32
    struct Handle
    {
        HANDLE value = INVALID_HANDLE_VALUE;
        Handle() = default;
        explicit Handle(HANDLE h): value(h) {}
        ~Handle() { if(value != INVALID_HANDLE_VALUE && value){ CloseHandle(value); } }
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        Handle(Handle &&other) noexcept: value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
        Handle &operator=(Handle &&other) noexcept
        {
            std::swap(value, other.value);
            return *this;
        }
    };

    struct LocalMemory
    {
        PSECURITY_DESCRIPTOR value = nullptr;
        ~LocalMemory() { if(value){ LocalFree(value); } }
    };

    struct Security
    {
        std::vector<unsigned char> user;
        std::array<unsigned char, SECURITY_MAX_SID_SIZE> system{};
        std::vector<unsigned char> acl;
        SECURITY_DESCRIPTOR descriptor{};
        SECURITY_ATTRIBUTES attributes{};

        PSID userSid() const { return reinterpret_cast<const TOKEN_USER *>(user.data())->User.Sid; }

        explicit Security(bool directory)
        {
            HANDLE rawToken = nullptr;
            if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)){
                securityError();
            }
            Handle token(rawToken);
            DWORD size = 0;
            GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
            if(!size){
                securityError();
            }
            user.resize(size);
            if(!GetTokenInformation(token.value, TokenUser, user.data(), size, &size)){
                securityError();
            }
            DWORD systemSize = static_cast<DWORD>(system.size());
            if(!CreateWellKnownSid(WinLocalSystemSid, nullptr, system.data(), &systemSize)){
                securityError();
            }
            acl.resize(sizeof(ACL) + 2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) + GetLengthSid(userSid()) + GetLengthSid(system.data()));
            auto *dacl = reinterpret_cast<PACL>(acl.data());
            const DWORD flags = directory ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : 0;
            if(!InitializeAcl(dacl, static_cast<DWORD>(acl.size()), ACL_REVISION)
                || !AddAccessAllowedAceEx(dacl, ACL_REVISION, flags, FILE_ALL_ACCESS, userSid())
                || !AddAccessAllowedAceEx(dacl, ACL_REVISION, flags, FILE_ALL_ACCESS, system.data())
                || !InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION)
                || !SetSecurityDescriptorOwner(&descriptor, userSid(), FALSE)
                || !SetSecurityDescriptorDacl(&descriptor, TRUE, dacl, FALSE)
                || !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)){
                securityError();
            }
            attributes = {sizeof(SECURITY_ATTRIBUTES), &descriptor, FALSE};
        }

        void verify(HANDLE handle, bool directory) const
        {
            PSID owner = nullptr;
            PACL dacl = nullptr;
            LocalMemory memory;
            if(GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                &owner, nullptr, &dacl, nullptr, &memory.value) != ERROR_SUCCESS){
                securityError();
            }
            SECURITY_DESCRIPTOR_CONTROL control = 0;
            DWORD revision = 0;
            if(!owner || !EqualSid(owner, userSid()) || !dacl || !IsValidAcl(dacl) || dacl->AceCount != 2
                || !GetSecurityDescriptorControl(memory.value, &control, &revision) || !(control & SE_DACL_PROTECTED)){
                securityError();
            }
            bool sawUser = false;
            bool sawSystem = false;
            const BYTE flags = directory ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : 0;
            for(DWORD i = 0; i < dacl->AceCount; ++i){
                void *raw = nullptr;
                if(!GetAce(dacl, i, &raw)){
                    securityError();
                }
                auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(raw);
                if(ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceFlags != flags || ace->Mask != FILE_ALL_ACCESS){
                    securityError();
                }
                PSID sid = &ace->SidStart;
                if(EqualSid(sid, userSid()) && !sawUser){
                    sawUser = true;
                }else if(EqualSid(sid, const_cast<unsigned char *>(system.data())) && !sawSystem){
                    sawSystem = true;
                }else{
                    securityError();
                }
            }
            if(!sawUser || !sawSystem){
                securityError();
            }
        }
    };

    BY_HANDLE_FILE_INFORMATION fileInfo(HANDLE handle, bool directory)
    {
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(handle, &info)
            || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            || bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != directory
            || (!directory && info.nNumberOfLinks != 1)){
            securityError();
        }
        return info;
    }

    bool sameFile(const BY_HANDLE_FILE_INFORMATION &a, const BY_HANDLE_FILE_INFORMATION &b)
    {
        return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber
            && a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow
            && a.ftCreationTime.dwHighDateTime == b.ftCreationTime.dwHighDateTime
            && a.ftCreationTime.dwLowDateTime == b.ftCreationTime.dwLowDateTime;
    }

    void removeHandle(HANDLE handle)
    {
        FILE_DISPOSITION_INFO disposition{TRUE};
        if(!SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition))){
            securityError();
        }
    }

    void randomBytes(unsigned char *bytes, std::size_t size)
    {
        if(BCryptGenRandom(nullptr, bytes, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0){
            securityError();
        }
    }
#else
    struct Handle
    {
        int value = -1;
        Handle() = default;
        explicit Handle(int fd): value(fd) {}
        ~Handle() { if(value >= 0){ ::close(value); } }
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        Handle(Handle &&other) noexcept: value(std::exchange(other.value, -1)) {}
        Handle &operator=(Handle &&other) noexcept
        {
            std::swap(value, other.value);
            return *this;
        }
    };

    struct stat fileInfo(int fd, bool directory)
    {
        struct stat info{};
        if(::fstat(fd, &info) != 0
            || (directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode))
            || info.st_uid != ::geteuid()
            || (info.st_mode & 07777) != (directory ? 0700 : 0600)
            || (!directory && info.st_nlink != 1)){
            securityError();
        }
        return info;
    }

    bool sameFile(const struct stat &a, const struct stat &b)
    {
        return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
    }

    void randomBytes(unsigned char *bytes, std::size_t size)
    {
        Handle random(::open("/dev/urandom", O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        struct stat info{};
        if(random.value < 0 || ::fstat(random.value, &info) != 0 || !S_ISCHR(info.st_mode)){
            securityError();
        }
        std::size_t offset = 0;
        while(offset < size){
            const auto count = ::read(random.value, bytes + offset, size - offset);
            if(count < 0 && errno == EINTR){
                continue;
            }
            if(count <= 0){
                securityError();
            }
            offset += static_cast<std::size_t>(count);
        }
    }
#endif

    template<std::size_t N> std::string randomHex()
    {
        std::array<unsigned char, N> bytes{};
        randomBytes(bytes.data(), bytes.size());
        constexpr char hex[] = "0123456789abcdef";
        std::string text;
        text.reserve(N * 2);
        for(const auto byte: bytes){
            text.push_back(hex[byte >> 4]);
            text.push_back(hex[byte & 15]);
        }
        return text;
    }
}

struct aibridge::InstanceFile::Impl
{
    InstanceInfo info;
    std::filesystem::path path;
    std::string content;
    bool cleaned = false;
#ifdef _WIN32
    std::vector<Handle> directories;
    BY_HANDLE_FILE_INFORMATION identity{};
#else
    Handle directory;
    struct stat identity{};
#endif
};

aibridge::InstanceFile::InstanceFile(std::unique_ptr<Impl> impl): m_impl(std::move(impl)) {}

aibridge::InstanceFile::~InstanceFile()
{
    cleanup();
}

const aibridge::InstanceInfo &aibridge::InstanceFile::info() const noexcept
{
    return m_impl->info;
}

const std::filesystem::path &aibridge::InstanceFile::path() const noexcept
{
    return m_impl->path;
}

std::unique_ptr<aibridge::InstanceFile> aibridge::InstanceFile::create(
    const std::filesystem::path &absoluteDirectory, std::uint16_t loopbackPort, std::string build)
{
    checkPath(absoluteDirectory);
    if(loopbackPort == 0 || build.empty() || build.size() > 128){
        securityError();
    }
    auto impl = std::make_unique<Impl>();
    impl->info.instance_id = randomHex<16>();
    impl->info.token = randomHex<32>();
    impl->info.port = loopbackPort;
    impl->info.build = std::move(build);
    impl->info.started_at = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
#ifdef _WIN32
    impl->info.pid = std::to_string(GetCurrentProcessId());
#else
    impl->info.pid = std::to_string(::getpid());
#endif
    impl->path = absoluteDirectory / (impl->info.instance_id + ".json");
    try{
        impl->content = nlohmann::json{
            {"v", 1}, {"instance_id", impl->info.instance_id}, {"token", impl->info.token},
            {"pid", impl->info.pid}, {"started_at", impl->info.started_at},
            {"port", impl->info.port}, {"build", impl->info.build},
        }.dump() + "\n";
    }catch(const std::exception &){
        securityError();
    }

#ifdef _WIN32
    Security directorySecurity(true);
    auto current = absoluteDirectory.root_path();
    auto openDirectory = [&impl](const std::filesystem::path &dir)
    {
        // Pin every component against rename/deletion until shutdown.
        Handle handle(CreateFileW(dir.c_str(), FILE_READ_ATTRIBUTES | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        fileInfo(handle.value, true);
        impl->directories.push_back(std::move(handle));
    };
    openDirectory(current);
    for(const auto &part: absoluteDirectory.relative_path()){
        current /= part;
        if(current == absoluteDirectory){
            if(!CreateDirectoryW(current.c_str(), &directorySecurity.attributes) && GetLastError() != ERROR_ALREADY_EXISTS){
                securityError();
            }
        }
        openDirectory(current);
    }
    directorySecurity.verify(impl->directories.back().value, true);
    Security fileSecurity(false);
    Handle file(CreateFileW(impl->path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE | READ_CONTROL,
        FILE_SHARE_READ, &fileSecurity.attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if(file.value == INVALID_HANDLE_VALUE){
        securityError();
    }
    try{
        impl->identity = fileInfo(file.value, false);
        fileSecurity.verify(file.value, false);
        DWORD written = 0;
        if(!WriteFile(file.value, impl->content.data(), static_cast<DWORD>(impl->content.size()), &written, nullptr)
            || written != impl->content.size() || !FlushFileBuffers(file.value)){
            securityError();
        }
    }catch(...){
        FILE_DISPOSITION_INFO disposition{TRUE};
        SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition, sizeof(disposition));
        throw;
    }
#else
    Handle directory(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if(directory.value < 0){
        securityError();
    }
    auto current = absoluteDirectory.root_path();
    for(const auto &part: absoluteDirectory.relative_path()){
        current /= part;
        if(current == absoluteDirectory && ::mkdirat(directory.value, part.c_str(), 0700) != 0 && errno != EEXIST){
            securityError();
        }
        Handle next(::openat(directory.value, part.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        if(next.value < 0){
            securityError();
        }
        directory = std::move(next);
    }
    fileInfo(directory.value, true);
    impl->directory = std::move(directory);
    const auto filename = impl->path.filename().native();
    Handle file(::openat(impl->directory.value, filename.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if(file.value < 0){
        securityError();
    }
    try{
        impl->identity = fileInfo(file.value, false);
        std::size_t offset = 0;
        while(offset < impl->content.size()){
            const auto count = ::write(file.value, impl->content.data() + offset, impl->content.size() - offset);
            if(count < 0 && errno == EINTR){
                continue;
            }
            if(count <= 0){
                securityError();
            }
            offset += static_cast<std::size_t>(count);
        }
        if(::fsync(file.value) != 0){
            securityError();
        }
    }catch(...){
        ::unlinkat(impl->directory.value, filename.c_str(), 0);
        throw;
    }
#endif
    return std::unique_ptr<InstanceFile>(new InstanceFile(std::move(impl)));
}

bool aibridge::InstanceFile::cleanup() noexcept
{
    if(m_impl->cleaned){
        return true;
    }
    try{
#ifdef _WIN32
        Handle file(CreateFileW(m_impl->path.c_str(), GENERIC_READ | DELETE | READ_CONTROL,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if(file.value == INVALID_HANDLE_VALUE){
            if(GetLastError() == ERROR_FILE_NOT_FOUND){
                m_impl->cleaned = true;
                return true;
            }
            return false;
        }
        const auto identity = fileInfo(file.value, false);
        if(!sameFile(identity, m_impl->identity)){
            return false;
        }
        Security(false).verify(file.value, false);
        if(identity.nFileSizeHigh != 0 || identity.nFileSizeLow != m_impl->content.size()){
            return false;
        }
        std::string content(m_impl->content.size(), '\0');
        DWORD read = 0;
        if(!ReadFile(file.value, content.data(), static_cast<DWORD>(content.size()), &read, nullptr)
            || read != content.size() || content != m_impl->content){
            return false;
        }
        removeHandle(file.value);
#else
        const auto filename = m_impl->path.filename().native();
        Handle file(::openat(m_impl->directory.value, filename.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        if(file.value < 0){
            if(errno == ENOENT){
                m_impl->cleaned = true;
                return true;
            }
            return false;
        }
        const auto identity = fileInfo(file.value, false);
        if(!sameFile(identity, m_impl->identity) || identity.st_size != static_cast<off_t>(m_impl->content.size())){
            return false;
        }
        std::string content(m_impl->content.size(), '\0');
        std::size_t offset = 0;
        while(offset < content.size()){
            const auto count = ::read(file.value, content.data() + offset, content.size() - offset);
            if(count < 0 && errno == EINTR){
                continue;
            }
            if(count <= 0){
                return false;
            }
            offset += static_cast<std::size_t>(count);
        }
        struct stat named{};
        // The 0700 directory excludes other users; no same-user adversary isolation is promised.
        if(content != m_impl->content || ::fstatat(m_impl->directory.value, filename.c_str(), &named, AT_SYMLINK_NOFOLLOW) != 0
            || !sameFile(named, identity) || ::unlinkat(m_impl->directory.value, filename.c_str(), 0) != 0){
            return false;
        }
#endif
        m_impl->cleaned = true;
        return true;
    }catch(...){
        return false;
    }
}
