#include "policy.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

namespace
{
    constexpr std::size_t maxPolicyBytes = 65536;
    constexpr auto actionNames = std::to_array<std::string_view>({
        "game_stop", "game_grant_control", "game_shop_close", "game_revive", "game_move_to", "game_npc_open", "game_npc_choose",
        "game_npc_input", "game_npc_close", "game_pick_up", "game_attack",
        "game_cast", "game_use_item", "game_shop_buy", "game_shop_quote_sell", "game_shop_sell",
        "game_trade_request", "game_trade_respond", "game_trade_offer",
        "game_trade_lock", "game_trade_commit", "game_trade_cancel",
        "game_auction_register", "game_auction_buy", "game_auction_cancel",
        "game_delivery_claim",
    });

    [[noreturn]] void invalidPolicy()
    {
        // Never include input: a misconfigured policy may contain credentials.
        throw std::runtime_error("invalid AI bridge policy");
    }

    bool knownAction(std::string_view name)
    {
        return std::find(actionNames.begin(), actionNames.end(), name) != actionNames.end();
    }

    std::uint64_t decimal(const nlohmann::json &value)
    {
        if(!value.is_string()){
            invalidPolicy();
        }
        const auto &text = value.get_ref<const std::string &>();
        if(text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0')){
            invalidPolicy();
        }
        if(!std::all_of(text.begin(), text.end(), [](char c){ return c >= '0' && c <= '9'; })){
            invalidPolicy();
        }
        std::uint64_t result = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if(parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()){
            invalidPolicy();
        }
        return result;
    }

    std::uint32_t integer(const nlohmann::json &value)
    {
        if(!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0)){
            invalidPolicy();
        }
        const auto number = value.get<std::uint64_t>();
        if(number > std::numeric_limits<std::uint32_t>::max()){
            invalidPolicy();
        }
        return static_cast<std::uint32_t>(number);
    }

    template<typename Set, typename Parse>
    void allowList(const nlohmann::json &value, bool &all, Set &values, std::size_t maximum, Parse parse)
    {
        if(value.is_string() && value.get_ref<const std::string &>() == "*"){
            all = true;
            return;
        }
        if(!value.is_array() || value.size() > maximum){
            invalidPolicy();
        }
        for(const auto &item: value){
            if(!values.insert(parse(item)).second){
                invalidPolicy();
            }
        }
    }
}

aibridge::Policy aibridge::Policy::parse(std::string_view jsonText)
{
    if(jsonText.empty() || jsonText.size() > maxPolicyBytes || jsonText.find('\0') != std::string_view::npos){
        invalidPolicy();
    }
    try{
        std::vector<std::set<std::string>> keys;
        const auto json = nlohmann::json::parse(jsonText, [&keys](int depth, nlohmann::json::parse_event_t event, nlohmann::json &value)
        {
            if(depth > 16){
                invalidPolicy();
            }
            using Event = nlohmann::json::parse_event_t;
            if(event == Event::object_start){
                keys.emplace_back();
            }else if(event == Event::key){
                if(keys.empty() || !keys.back().insert(value.get<std::string>()).second){
                    invalidPolicy();
                }
            }else if(event == Event::object_end){
                keys.pop_back();
            }
            return true;
        });
        if(!json.is_object()){
            invalidPolicy();
        }
        Policy result;
        for(const auto &[key, value]: json.items()){
            if(key == "allowed_actions"){
                allowList(value, result.m_allActions, result.m_actions, 37, [](const auto &item)
                {
                    if(!item.is_string()){
                        invalidPolicy();
                    }
                    auto name = item.template get<std::string>();
                    if(!knownAction(name)){
                        invalidPolicy();
                    }
                    return name;
                });
            }else if(key == "allowed_peer_uids"){
                allowList(value, result.m_allPeers, result.m_peers, 4096, decimal);
            }else if(key == "allowed_item_ids"){
                allowList(value, result.m_allItems, result.m_items, 4096, integer);
            }else if(key == "allow_pvp" || key == "allow_npc_dialogue"){
                if(!value.is_boolean()){
                    invalidPolicy();
                }
                (key == "allow_pvp" ? result.allowPvp : result.allowNpcDialogue) = value.get<bool>();
            }else if(key == "max_gold_per_operation"){
                result.maxGoldPerOperation = decimal(value);
            }else if(key == "max_gold_per_session"){
                result.maxGoldPerSession = decimal(value);
            }else if(key == "max_item_count_per_operation"){
                result.maxItemCountPerOperation = integer(value);
            }else{
                invalidPolicy();
            }
        }
        return result;
    }catch(const std::exception &){
        invalidPolicy();
    }
}

aibridge::Policy aibridge::Policy::load(const std::filesystem::path &absolutePath)
{
    if(!absolutePath.is_absolute()){
        invalidPolicy();
    }
    std::ifstream stream(absolutePath, std::ios::binary);
    if(!stream){
        invalidPolicy();
    }
    std::string text(maxPolicyBytes + 1, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    if(stream.bad()){
        invalidPolicy();
    }
    text.resize(static_cast<std::size_t>(stream.gcount()));
    return parse(text);
}

bool aibridge::Policy::allowsAction(std::string_view action) const
{
    // Stop is a safety operation; authentication is still required by transport.
    return action == "game_stop" || (knownAction(action) && (m_allActions || m_actions.contains(action)));
}

bool aibridge::Policy::allowsPeer(std::uint64_t uid) const
{
    return m_allPeers || m_peers.contains(uid);
}

bool aibridge::Policy::allowsItem(std::uint32_t itemId) const
{
    return m_allItems || m_items.contains(itemId);
}
