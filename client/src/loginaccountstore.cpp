#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <SDL3/SDL_filesystem.h>
#include <nlohmann/json.hpp>
#include "loginaccountstore.hpp"
#include "sysconst.hpp"

LoginAccountStore::LoginAccountStore(std::filesystem::path path)
    : m_path(std::move(path))
{}

void LoginAccountStore::load()
{
    if(!std::filesystem::exists(m_path)){
        m_accounts.clear();
        return;
    }

    std::ifstream stream(m_path, std::ios::binary);
    if(!stream){
        throw std::runtime_error("Unable to read saved login accounts");
    }

    const auto doc = nlohmann::json::parse(stream);
    if(doc.at("schema_version") != 1 || !doc.at("accounts").is_array()){
        throw std::runtime_error("Invalid saved login accounts format");
    }

    std::vector<Account> accounts;
    for(const auto &row: doc.at("accounts")){
        Account account{row.at("id").get<std::string>(), row.at("password").get<std::string>()};
        if(account.id.empty() || account.id.size() > SYS_IDSIZE
                || account.password.empty() || account.password.size() > SYS_PWDSIZE
                || account.id.contains('\0') || account.password.contains('\0')){
            throw std::runtime_error("Invalid saved login account entry");
        }
        if(std::ranges::find(accounts, account.id, &Account::id) == accounts.end()){
            accounts.push_back(std::move(account));
        }
    }
    m_accounts = std::move(accounts);
}

void LoginAccountStore::remember(const std::string &id, const std::string &password)
{
    auto accounts = m_accounts;
    std::erase_if(accounts, [&id](const Account &account){ return account.id == id; });
    accounts.insert(accounts.begin(), Account{id, password});

    auto rows = nlohmann::json::array();
    for(const auto &account: accounts){
        rows.push_back({{"id", account.id}, {"password", account.password}});
    }
    const nlohmann::json doc{{"schema_version", 1}, {"accounts", std::move(rows)}};

    auto temporary = m_path;
    temporary += ".tmp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << doc.dump(2) << '\n';
    stream.close();

    const auto from = temporary.u8string();
    const auto to = m_path.u8string();
    if(!SDL_RenamePath(reinterpret_cast<const char *>(from.c_str()), reinterpret_cast<const char *>(to.c_str()))){
        throw std::runtime_error("Unable to replace saved login accounts");
    }
    m_accounts = std::move(accounts);
}

const LoginAccountStore::Account *LoginAccountStore::adjacent(std::string_view currentID, bool previous) const
{
    if(m_accounts.empty()){
        return nullptr;
    }
    const auto current = std::ranges::find(m_accounts, currentID, &Account::id);
    if(current == m_accounts.end()){
        return &m_accounts.front();
    }
    if(previous){
        return &*(current == m_accounts.begin() ? m_accounts.end() - 1 : current - 1);
    }
    return &*(current + 1 == m_accounts.end() ? m_accounts.begin() : current + 1);
}
