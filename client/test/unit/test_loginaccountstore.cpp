#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include "loginaccountstore.hpp"

namespace
{
    struct AccountFiles
    {
        const std::filesystem::path directory = std::filesystem::temp_directory_path()
            / ("mir2x-login-accounts-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::filesystem::path path = directory / "accounts.json";

        AccountFiles()
        {
            std::filesystem::create_directory(directory);
        }

        ~AccountFiles()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            std::filesystem::remove(directory / "accounts.json.tmp", ignored);
            std::filesystem::remove(directory, ignored);
        }
    };
}

TEST_CASE("Saved login accounts reopen with their matching passwords", "[loginaccountstore]")
{
    AccountFiles files;
    LoginAccountStore store(files.path);
    store.load();
    store.remember("first-account", "first-password");
    store.remember("second-account", "second-password");

    LoginAccountStore reopened(files.path);
    reopened.load();
    REQUIRE(reopened.accounts().size() == 2);
    CHECK(reopened.accounts()[0].id == "second-account");
    CHECK(reopened.accounts()[0].password == "second-password");
    CHECK(reopened.accounts()[1].id == "first-account");
    CHECK(reopened.accounts()[1].password == "first-password");
}

TEST_CASE("A remembered account is updated and the account list cycles in both directions", "[loginaccountstore]")
{
    AccountFiles files;
    LoginAccountStore store(files.path);
    store.remember("alpha", "alpha-password");
    store.remember("beta", "beta-password");
    store.remember("gamma", "gamma-password");
    store.remember("alpha", "alpha-updated-password");

    LoginAccountStore reopened(files.path);
    reopened.load();
    REQUIRE(reopened.accounts().size() == 3);
    CHECK(reopened.accounts().front().id == "alpha");
    CHECK(reopened.accounts().front().password == "alpha-updated-password");
    CHECK(reopened.adjacent("alpha", true)->id == "beta");
    CHECK(reopened.adjacent("alpha", false)->id == "gamma");
    CHECK(reopened.adjacent("beta", false)->id == "alpha");
    CHECK(reopened.adjacent("gamma", true)->id == "alpha");
}
