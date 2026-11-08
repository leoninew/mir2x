#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class LoginAccountStore
{
    public:
        struct Account
        {
            std::string id;
            std::string password;
        };

    private:
        std::filesystem::path m_path;
        std::vector<Account> m_accounts;

    public:
        explicit LoginAccountStore(std::filesystem::path path);

        void load();
        void remember(const std::string &id, const std::string &password);
        const Account *adjacent(std::string_view currentID, bool previous) const;

        const std::vector<Account> &accounts() const
        {
            return m_accounts;
        }
};
