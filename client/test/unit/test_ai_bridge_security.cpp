#include <catch2/catch_test_macros.hpp>
#include "aibridge/instancefile.hpp"
#include "aibridge/policy.hpp"
#include "aibridge/protocol.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <winioctl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    namespace fs = std::filesystem;

    struct Scratch
    {
        fs::path root;
        Scratch()
        {
            static std::atomic<unsigned> sequence = 0;
#ifdef MIR2X_TEST_WORKSPACE_ROOT
            const fs::path workspace = MIR2X_TEST_WORKSPACE_ROOT;
#else
            // CMake passes absolute source filenames for this target.
            const auto workspace = fs::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
#endif
            REQUIRE(workspace.is_absolute());
            REQUIRE(fs::exists(workspace / "CMakeLists.txt"));
            const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
            root = workspace / ".runtime" / "test-ai-bridge" / ("security-" + unique);
            REQUIRE(fs::create_directories(root));
        }
        ~Scratch()
        {
            std::error_code error;
            fs::remove_all(root, error);
        }
    };

    std::string readFile(const fs::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        REQUIRE(stream.good());
        return {std::istreambuf_iterator<char>(stream), {}};
    }

    void writeFile(const fs::path &path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE(stream.good());
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.close();
        REQUIRE(stream.good());
    }

#ifdef _WIN32
    struct WinHandle
    {
        HANDLE value = INVALID_HANDLE_VALUE;
        ~WinHandle() { if(value != INVALID_HANDLE_VALUE && value){ CloseHandle(value); } }
    };

    struct LocalMemory
    {
        PSECURITY_DESCRIPTOR value = nullptr;
        ~LocalMemory() { if(value){ LocalFree(value); } }
    };

    bool accessAllowed(PSECURITY_DESCRIPTOR descriptor, HANDLE token)
    {
        GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
        DWORD desired = GENERIC_READ | GENERIC_WRITE;
        MapGenericMask(&desired, &mapping);
        std::vector<unsigned char> privileges(4096);
        DWORD size = static_cast<DWORD>(privileges.size());
        DWORD granted = 0;
        BOOL allowed = FALSE;
        REQUIRE(AccessCheck(descriptor, token, desired, &mapping,
            reinterpret_cast<PPRIVILEGE_SET>(privileges.data()), &size, &granted, &allowed));
        return allowed != FALSE;
    }

    void verifyWindowsAcl(const fs::path &path, bool directory)
    {
        WinHandle file{CreateFileW(path.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr)};
        REQUIRE(file.value != INVALID_HANDLE_VALUE);
        LocalMemory security;
        PSID owner = nullptr;
        PACL acl = nullptr;
        REQUIRE(GetSecurityInfo(file.value, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &owner, nullptr, &acl, nullptr, &security.value) == ERROR_SUCCESS);
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        REQUIRE(GetSecurityDescriptorControl(security.value, &control, &revision));
        REQUIRE((control & SE_DACL_PROTECTED) != 0);
        REQUIRE(acl != nullptr);
        REQUIRE(acl->AceCount == 2);

        WinHandle process;
        REQUIRE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &process.value));
        DWORD size = 0;
        GetTokenInformation(process.value, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> user(size);
        REQUIRE(GetTokenInformation(process.value, TokenUser, user.data(), size, &size));
        const auto sid = reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid;
        REQUIRE(EqualSid(owner, sid));
        unsigned users = 0;
        unsigned systems = 0;
        for(DWORD i = 0; i < acl->AceCount; ++i){
            void *raw = nullptr;
            REQUIRE(GetAce(acl, i, &raw));
            const auto ace = static_cast<ACCESS_ALLOWED_ACE *>(raw);
            REQUIRE(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE);
            REQUIRE(ace->Header.AceFlags == (directory ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : 0));
            REQUIRE(ace->Mask == FILE_ALL_ACCESS);
            users += EqualSid(&ace->SidStart, sid) ? 1 : 0;
            systems += IsWellKnownSid(&ace->SidStart, WinLocalSystemSid) ? 1 : 0;
        }
        REQUIRE(users == 1);
        REQUIRE(systems == 1);

        WinHandle impersonation;
        REQUIRE(DuplicateToken(process.value, SecurityImpersonation, &impersonation.value));
        REQUIRE(accessAllowed(security.value, impersonation.value));
        BYTE world[SECURITY_MAX_SID_SIZE]{};
        DWORD worldSize = sizeof(world);
        REQUIRE(CreateWellKnownSid(WinWorldSid, nullptr, world, &worldSize));
        SID_AND_ATTRIBUTES restrictedSid{world, 0};
        WinHandle restrictedPrimary;
        REQUIRE(CreateRestrictedToken(process.value, DISABLE_MAX_PRIVILEGE, 0, nullptr, 0, nullptr, 1, &restrictedSid, &restrictedPrimary.value));
        WinHandle restricted;
        REQUIRE(DuplicateToken(restrictedPrimary.value, SecurityImpersonation, &restricted.value));
        REQUIRE_FALSE(accessAllowed(security.value, restricted.value));

        LPWSTR sddl = nullptr;
        REQUIRE(ConvertSecurityDescriptorToStringSecurityDescriptorW(security.value, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &sddl, nullptr));
        std::wstring text(sddl);
        LocalFree(sddl);
        std::cout << "Real Windows " << (directory ? "directory" : "file") << " ACL: "
            << std::string(text.begin(), text.end())
            << "; AccessCheck current=allowed, restricted-world=denied\n";
    }

    struct Junction
    {
        fs::path path;
        ~Junction() { if(!path.empty()){ RemoveDirectoryW(path.c_str()); } }
    };

    void createJunction(const fs::path &path, const fs::path &target)
    {
        REQUIRE(CreateDirectoryW(path.c_str(), nullptr));
        WinHandle handle{CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
        REQUIRE(handle.value != INVALID_HANDLE_VALUE);
        struct MountPoint
        {
            DWORD tag;
            WORD dataLength;
            WORD reserved;
            WORD substituteOffset;
            WORD substituteLength;
            WORD printOffset;
            WORD printLength;
            wchar_t names[1];
        };
        const std::wstring print = fs::path(target).make_preferred().native();
        const std::wstring substitute = L"\\??\\" + print;
        std::vector<unsigned char> buffer(offsetof(MountPoint, names) + (substitute.size() + print.size() + 2) * sizeof(wchar_t), 0);
        auto *data = reinterpret_cast<MountPoint *>(buffer.data());
        data->tag = IO_REPARSE_TAG_MOUNT_POINT;
        data->dataLength = static_cast<WORD>(buffer.size() - 8);
        data->substituteLength = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
        data->printOffset = static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t));
        data->printLength = static_cast<WORD>(print.size() * sizeof(wchar_t));
        std::copy(substitute.begin(), substitute.end(), data->names);
        std::copy(print.begin(), print.end(), data->names + substitute.size() + 1);
        DWORD returned = 0;
        REQUIRE(DeviceIoControl(handle.value, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr));
    }
#endif
}

TEST_CASE("caller grants and local shop close require frozen policy permission", "[ai_bridge_security]")
{
    const auto readonly=aibridge::Policy::parse("{}");
    CHECK_FALSE(readonly.allowsAction("game_grant_control"));
    CHECK_FALSE(readonly.allowsAction("game_shop_close"));
    const auto movement=aibridge::Policy::parse(R"({"allowed_actions":["game_grant_control","game_move_to"]})");
    CHECK(movement.allowsAction("game_grant_control"));
    CHECK_FALSE(movement.allowsAction("game_shop_close"));
    const auto all=aibridge::Policy::parse(R"({"allowed_actions":"*"})");
    CHECK(all.allowsAction("game_grant_control"));
    CHECK(all.allowsAction("game_shop_close"));
    CHECK_FALSE(all.allowsAction("game_resume_old_task"));
}

TEST_CASE("bridge policy defaults deny and wildcard cannot authorize unknown actions", "[ai_bridge_security]")
{
    const auto empty = aibridge::Policy::parse("{}");
    REQUIRE_FALSE(empty.allowsAction("game_move_to"));
    REQUIRE(empty.allowsAction("game_stop"));
    REQUIRE_FALSE(empty.allowsItem(1));
    REQUIRE_FALSE(empty.allowsPeer(1));
    REQUIRE_FALSE(empty.allowPvp);
    REQUIRE_FALSE(empty.allowNpcDialogue);
    REQUIRE(empty.maxGoldPerOperation == 0);
    REQUIRE(empty.maxGoldPerSession == 0);
    REQUIRE(empty.maxItemCountPerOperation == 0);
    const auto denied = aibridge::Policy::parse(R"({"allowed_actions":[],"allowed_peer_uids":[],"allowed_item_ids":[]})");
    REQUIRE_FALSE(denied.allowsAction("game_attack"));
    REQUIRE_FALSE(denied.allowsPeer(1));
    REQUIRE_FALSE(denied.allowsItem(1));
    const auto all = aibridge::Policy::parse(R"({"allowed_actions":"*","allowed_peer_uids":"*","allowed_item_ids":"*"})");
    REQUIRE(all.allowsAction("game_attack"));
    REQUIRE(all.allowsPeer(1));
    REQUIRE(all.allowsItem(1));
    REQUIRE_FALSE(all.allowsAction("game_execute_lua"));
    REQUIRE_FALSE(all.allowPvp);
    REQUIRE_FALSE(all.allowNpcDialogue);
}

TEST_CASE("bridge policy retains exact 64 bit limits and explicit allowlists", "[ai_bridge_security]")
{
    const auto policy = aibridge::Policy::parse(R"({
        "allowed_actions":["game_trade_commit","game_npc_choose"],
        "allow_pvp":true,"allow_npc_dialogue":true,
        "allowed_peer_uids":["18446744073709551615"],
        "allowed_item_ids":[1,4294967295],
        "max_gold_per_operation":"9007199254740993",
        "max_gold_per_session":"18446744073709551615",
        "max_item_count_per_operation":4294967295
    })");
    REQUIRE(policy.allowsAction("game_trade_commit"));
    REQUIRE_FALSE(policy.allowsAction("game_trade_offer"));
    REQUIRE(policy.allowsPeer(UINT64_MAX));
    REQUIRE_FALSE(policy.allowsPeer(1));
    REQUIRE(policy.allowsItem(UINT32_MAX));
    REQUIRE_FALSE(policy.allowsItem(2));
    REQUIRE(policy.allowPvp);
    REQUIRE(policy.allowNpcDialogue);
    REQUIRE(policy.maxGoldPerOperation == 9007199254740993ULL);
    REQUIRE(policy.maxGoldPerSession == UINT64_MAX);
    REQUIRE(policy.maxItemCountPerOperation == UINT32_MAX);
}

TEST_CASE("bridge policy rejects malformed ambiguous and out of range input", "[ai_bridge_security]")
{
    const std::vector<std::string> invalid{
        "", "[]", "null", "{", "{}{}",
        R"({"unknown":true})", R"({"allow_pvp":true,"allow_pvp":false})",
        R"({"allow_pvp":1})", R"({"allow_npc_dialogue":null})",
        R"({"allowed_actions":["*"]})", R"({"allowed_actions":["game_execute_lua"]})",
        R"({"allowed_actions":["game_attack","game_attack"]})", R"({"allowed_actions":""})",
        R"({"allowed_item_ids":[-1]})", R"({"allowed_item_ids":[4294967296]})",
        R"({"allowed_item_ids":["1"]})", R"({"allowed_peer_uids":[1]})",
        R"({"max_gold_per_operation":1})", R"({"max_gold_per_operation":"18446744073709551616"})",
        R"({"max_gold_per_operation":"-1"})", R"({"max_gold_per_operation":"01"})",
        R"({"max_gold_per_operation":"+1"})", R"({"max_gold_per_operation":"1e2"})",
        R"({"max_gold_per_operation":" 1"})", R"({"max_gold_per_operation":"1.0"})",
        R"({"max_gold_per_operation":""})", R"({"max_gold_per_operation":"*"})",
        R"({"max_item_count_per_operation":-1})", R"({"max_item_count_per_operation":4294967296})",
        R"({"max_item_count_per_operation":1.0})", R"({"max_item_count_per_operation":true})",
        R"({"max_gold_per_session":1e999})",
        std::string("{\"allowed_actions\":[\"") + char(0xff) + "\"]}",
        std::string("{}") + '\0' + "{}",
        std::string(65537, ' '),
        std::string(20, '[') + "0" + std::string(20, ']'),
    };
    for(const auto &text: invalid){
        REQUIRE_THROWS_AS(aibridge::Policy::parse(text), std::runtime_error);
    }
}

TEST_CASE("bridge policy loads bounded local configuration", "[ai_bridge_security]")
{
    Scratch scratch;
    const auto path = scratch.root / "policy.json";
    writeFile(path, R"({"allowed_actions":["game_move_to"]})");
    REQUIRE(aibridge::Policy::load(path).allowsAction("game_move_to"));
    writeFile(path, std::string(65537, ' '));
    REQUIRE_THROWS(aibridge::Policy::load(path));
    REQUIRE_THROWS(aibridge::Policy::load("relative.json"));
    REQUIRE_THROWS(aibridge::Policy::load(scratch.root / "missing.json"));
}

TEST_CASE("bridge policy matches published method permissions and example fixtures", "[ai_bridge_security]")
{
    const auto all = aibridge::Policy::parse(R"({"allowed_actions":"*"})");
    for(const auto &[method, definition]: aibridge::contract().at("x-methods").items()){
        const bool expected = definition.at("write").get<bool>();
        CAPTURE(method);
        REQUIRE(all.allowsAction(method) == expected);
        const auto policy = nlohmann::json{{"allowed_actions", nlohmann::json::array({method})}};
        if(expected){
            REQUIRE(aibridge::Policy::parse(policy.dump()).allowsAction(method));
            REQUIRE_NOTHROW(aibridge::validateDefinition(policy, "policy"));
        }else{
            REQUIRE_THROWS(aibridge::Policy::parse(policy.dump()));
        }
    }
    Scratch scratch;
    const auto workspace = scratch.root.parent_path().parent_path().parent_path();
    for(const auto filename: {"policy-example.json", "policy-readonly.json"}){
        const auto path = workspace / "contracts" / "ai-bridge" / "v1" / filename;
        REQUIRE_NOTHROW(aibridge::Policy::load(path));
        REQUIRE_NOTHROW(aibridge::validateDefinition(nlohmann::json::parse(readFile(path)), "policy"));
    }
}

TEST_CASE("bridge policy bounds allowlists before materializing typed permissions", "[ai_bridge_security]")
{
    nlohmann::json value{{"allowed_item_ids", nlohmann::json::array()}, {"allowed_peer_uids", nlohmann::json::array()}};
    for(unsigned i = 0; i < 4096; ++i){
        value["allowed_item_ids"].push_back(i);
        value["allowed_peer_uids"].push_back(std::to_string(i));
    }
    REQUIRE_NOTHROW(aibridge::Policy::parse(value.dump()));
    value["allowed_item_ids"].push_back(4096);
    REQUIRE_THROWS(aibridge::Policy::parse(value.dump()));
    value.erase("allowed_item_ids");
    value["allowed_peer_uids"].push_back("4096");
    REQUIRE_THROWS(aibridge::Policy::parse(value.dump()));
}

TEST_CASE("bridge instance credentials have real private permissions and metadata", "[ai_bridge_security]")
{
    Scratch scratch;
    const auto directory = scratch.root / "instances";
    auto instance = aibridge::InstanceFile::create(directory, 12345, "test-build");
    const auto &info = instance->info();
    REQUIRE(info.v == 1);
    REQUIRE(info.port == 12345);
    REQUIRE(info.build == "test-build");
    REQUIRE(info.instance_id.size() == 32);
    REQUIRE(info.token.size() == 64);
    REQUIRE(bool(info.token.find_first_not_of("0123456789abcdef") == std::string::npos));
    REQUIRE(bool(info.instance_id.find_first_not_of("0123456789abcdef") == std::string::npos));
    REQUIRE_FALSE(info.started_at.empty());
    REQUIRE_FALSE(info.pid.empty());
    const auto json = nlohmann::json::parse(readFile(instance->path()));
    REQUIRE(json.size() == 7);
    REQUIRE(bool(json.at("token") == info.token));
    REQUIRE(bool(json.at("instance_id") == info.instance_id));
    REQUIRE(json.at("pid").is_string());
    REQUIRE(json.at("started_at").is_string());
    REQUIRE_NOTHROW(aibridge::validateDefinition(json, "instance"));
#ifdef _WIN32
    REQUIRE(info.pid == std::to_string(GetCurrentProcessId()));
    verifyWindowsAcl(directory, true);
    verifyWindowsAcl(instance->path(), false);
#else
    REQUIRE(info.pid == std::to_string(::getpid()));
    struct stat dirStat{}, fileStat{};
    REQUIRE(::stat(directory.c_str(), &dirStat) == 0);
    REQUIRE(::stat(instance->path().c_str(), &fileStat) == 0);
    REQUIRE((dirStat.st_mode & 07777) == 0700);
    REQUIRE((fileStat.st_mode & 07777) == 0600);
    REQUIRE(fileStat.st_uid == ::geteuid());
#endif
    const auto path = instance->path();
    instance.reset();
    REQUIRE_FALSE(fs::exists(path));
}

TEST_CASE("bridge cleanup removes only its matching instance and is idempotent", "[ai_bridge_security]")
{
    Scratch scratch;
    const auto directory = scratch.root / "instances";
    auto first = aibridge::InstanceFile::create(directory, 12345, "test");
    auto second = aibridge::InstanceFile::create(directory, 12346, "test");
    REQUIRE(bool(first->info().token != second->info().token));
    REQUIRE(first->path() != second->path());
    const auto original = readFile(first->path());
    writeFile(first->path(), "{}");
    REQUIRE_FALSE(first->cleanup());
    REQUIRE(fs::exists(first->path()));
    writeFile(first->path(), original);
    const auto renamed = directory / "retained.json";
    fs::rename(first->path(), renamed);
    writeFile(first->path(), original);
    REQUIRE_FALSE(first->cleanup());
    REQUIRE(fs::exists(first->path()));
    fs::remove(first->path());
    fs::rename(renamed, first->path());
    REQUIRE(first->cleanup());
    REQUIRE(first->cleanup());
    REQUIRE_FALSE(fs::exists(first->path()));
    REQUIRE(fs::exists(second->path()));
    fs::remove(second->path());
    REQUIRE(second->cleanup());
    REQUIRE(second->cleanup());
}

TEST_CASE("bridge instance refuses insecure directories and invalid paths", "[ai_bridge_security]")
{
    Scratch scratch;
    REQUIRE_THROWS(aibridge::InstanceFile::create("relative", 12345, "test"));
    REQUIRE_THROWS(aibridge::InstanceFile::create(scratch.root / ".." / "escape", 12345, "test"));
    REQUIRE_THROWS(aibridge::InstanceFile::create(scratch.root / "zero", 0, "test"));
    REQUIRE_THROWS(aibridge::InstanceFile::create(scratch.root / "empty", 12345, ""));
    REQUIRE_THROWS(aibridge::InstanceFile::create(scratch.root / "long", 12345, std::string(129, 'a')));
    auto maximumBuild = aibridge::InstanceFile::create(scratch.root / "maximum", 12345, std::string(128, 'a'));
    REQUIRE_NOTHROW(aibridge::validateDefinition(nlohmann::json::parse(readFile(maximumBuild->path())), "instance"));
    const auto insecure = scratch.root / "insecure";
    REQUIRE(fs::create_directory(insecure));
#ifdef _WIN32
    LocalMemory security;
    REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;WD)", SDDL_REVISION_1, &security.value, nullptr));
    PACL acl = nullptr;
    BOOL present = FALSE, defaulted = FALSE;
    REQUIRE(GetSecurityDescriptorDacl(security.value, &present, &acl, &defaulted));
    REQUIRE(SetNamedSecurityInfoW(const_cast<wchar_t *>(insecure.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr) == ERROR_SUCCESS);
#else
    REQUIRE(::chmod(insecure.c_str(), 0755) == 0);
#endif
    REQUIRE_THROWS(aibridge::InstanceFile::create(insecure, 12345, "test"));
    REQUIRE(fs::is_empty(insecure));
}

TEST_CASE("bridge cleanup refuses hard linked credentials", "[ai_bridge_security]")
{
    Scratch scratch;
    auto instance = aibridge::InstanceFile::create(scratch.root / "instances", 12345, "test");
    const auto link = instance->path().parent_path() / "hardlink.json";
    fs::create_hard_link(instance->path(), link);
    REQUIRE_FALSE(instance->cleanup());
    REQUIRE(fs::exists(instance->path()));
    fs::remove(link);
    REQUIRE(instance->cleanup());
}

TEST_CASE("bridge instance refuses real directory reparse points or symlinks", "[ai_bridge_security]")
{
    Scratch scratch;
    const auto target = scratch.root / "target";
    auto instance = aibridge::InstanceFile::create(target, 12345, "test");
    instance.reset();
    const auto alias = scratch.root / "alias";
#ifdef _WIN32
    Junction junction{alias};
    createJunction(alias, target);
#else
    fs::create_directory_symlink(target, alias);
#endif
    REQUIRE_THROWS(aibridge::InstanceFile::create(alias, 12345, "test"));
    REQUIRE_THROWS(aibridge::InstanceFile::create(alias / "child", 12345, "test"));
    REQUIRE(fs::is_empty(target));
}
