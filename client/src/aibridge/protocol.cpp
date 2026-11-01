#include "protocol.hpp"
#include "aibridge_contract.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <regex>
#include <set>

namespace aibridge
{
    ProtocolError::ProtocolError(std::string c, std::string message)
        : std::runtime_error(std::move(message)), code(std::move(c))
    {}

    namespace
    {
        [[noreturn]] void invalid()
        {
            throw ProtocolError("invalid_argument", "请求不符合桥接契约");
        }

        void check(const Json &v, const Json &s, size_t depth)
        {
            if(depth > 64){ invalid(); }
            if(s.contains("$ref")){
                const auto ref = s.at("$ref").get<std::string>();
                if(!ref.starts_with("#/$defs/")){ invalid(); }
                check(v, contract().at(Json::json_pointer(ref.substr(1))), depth + 1);
                return;
            }
            if(s.contains("oneOf")){
                size_t matches = 0;
                for(const auto &branch: s.at("oneOf")){
                    try{ check(v, branch, depth + 1); ++matches; }
                    catch(const ProtocolError &){}
                }
                if(matches != 1){ invalid(); }
            }
            if(s.contains("anyOf")){
                bool matched = false;
                for(const auto &branch: s.at("anyOf")){
                    try{ check(v, branch, depth + 1); matched = true; break; }
                    catch(const ProtocolError &){}
                }
                if(!matched){ invalid(); }
            }
            if(s.contains("const") && v != s.at("const")){ invalid(); }
            if(s.contains("enum")){
                bool found = false;
                for(const auto &entry: s.at("enum")){ found |= entry == v; }
                if(!found){ invalid(); }
            }
            if(s.contains("type")){
                const auto type = s.at("type").get<std::string>();
                const bool valid = (type == "object" && v.is_object())
                    || (type == "array" && v.is_array())
                    || (type == "string" && v.is_string())
                    || (type == "integer" && (v.is_number_integer() || v.is_number_unsigned()))
                    || (type == "boolean" && v.is_boolean())
                    || (type == "null" && v.is_null());
                if(!valid){ invalid(); }
            }
            if(v.is_object()){
                for(const auto &key: s.value("required", Json::array())){
                    if(!v.contains(key.get<std::string>())){ invalid(); }
                }
                const auto props = s.value("properties", Json::object());
                for(auto it = v.begin(); it != v.end(); ++it){
                    if(props.contains(it.key())){ check(it.value(), props.at(it.key()), depth + 1); }
                    else if(!s.value("additionalProperties", true)){ invalid(); }
                }
            }
            if(v.is_array()){
                if((s.contains("minItems") && v.size() < s.at("minItems").get<size_t>())
                    || (s.contains("maxItems") && v.size() > s.at("maxItems").get<size_t>())){ invalid(); }
                if(s.value("uniqueItems", false)){
                    std::set<std::string> values;
                    for(const auto &entry: v){ if(!values.insert(entry.dump()).second){ invalid(); } }
                }
                if(s.contains("items")){ for(const auto &entry: v){ check(entry, s.at("items"), depth + 1); } }
            }
            if(v.is_string()){
                const auto &text = v.get_ref<const std::string &>();
                size_t chars = 0;
                for(const unsigned char c: text){ if((c & 0xc0) != 0x80){ ++chars; } }
                if(chars < s.value("minLength", size_t{0}) || chars > s.value("maxLength", size_t{maxResponseBytes})
                    || text.size() > s.value("x-maxUtf8Bytes", size_t{maxResponseBytes})){ invalid(); }
                if(s.contains("pattern") && !std::regex_match(text, std::regex(s.at("pattern").get<std::string>()))){ invalid(); }
                if(s.contains("x-decimalMaximum")){
                    uint64_t number = 0;
                    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number);
                    if(ec != std::errc{} || end != text.data() + text.size() || number > s.at("x-decimalMaximum").get<uint64_t>()){ invalid(); }
                }
            }
            if(v.is_number()){
                if(!v.is_number_integer() && !v.is_number_unsigned()){ invalid(); }
                if(s.contains("minimum") && v < s.at("minimum")){ invalid(); }
                if(s.contains("maximum") && v > s.at("maximum")){ invalid(); }
            }
        }
    }

    Json strictParse(std::string_view text)
    {
        if(text.empty() || text.size() > maxResponseBytes || text.find('\0') != std::string_view::npos){ invalid(); }
        std::vector<std::set<std::string>> keys;
        try{
            auto result = Json::parse(text, [&](int depth, Json::parse_event_t event, Json &value)
            {
                if(depth > static_cast<int>(maxJsonDepth)
                    || ((event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start)
                        && depth >= static_cast<int>(maxJsonDepth))){ invalid(); }
                if(event == Json::parse_event_t::object_start){ keys.emplace_back(); }
                if(event == Json::parse_event_t::key && (keys.empty() || !keys.back().insert(value.get<std::string>()).second)){ invalid(); }
                if(event == Json::parse_event_t::object_end){ keys.pop_back(); }
                if(event == Json::parse_event_t::value && value.is_number_float() && !std::isfinite(value.get<double>())){ invalid(); }
                return true;
            });
            return result;
        }
        catch(const Json::exception &){ invalid(); }
    }

    const Json &contract()
    {
        static const auto value = Json::parse(contractText);
        return value;
    }

    void validateSchema(const Json &v, const Json &s){ check(v, s, 0); }
    void validateDefinition(const Json &v, std::string_view name)
    {
        const auto &defs = contract().at("$defs");
        if(!defs.contains(std::string(name))){ invalid(); }
        validateSchema(v, defs.at(std::string(name)));
    }

    bool isAction(std::string_view method)
    {
        const auto &methods = contract().at("x-methods");
        const auto it = methods.find(std::string(method));
        return it != methods.end() && it->at("action").get<bool>();
    }

    Json decodeRequest(std::string_view text)
    {
        if(text.size() > maxRequestBytes){ invalid(); }
        auto value = strictParse(text);
        if(!value.is_object() || !value.contains("method") || !value.at("method").is_string()){ invalid(); }
        if(value.contains("v") && value.at("v") != 1){ throw ProtocolError("version_mismatch", "桥接协议版本不兼容"); }
        const auto method = value.at("method").get<std::string>();
        if(method == "hello" || method == "heartbeat"){
            validateDefinition(value, method + "Request");
        }
        else{
            const auto &methods = contract().at("x-methods");
            if(!methods.contains(method)){ invalid(); }
            validateDefinition(value, method + "Request");
        }
        return value;
    }

    std::string canonicalRequest(const Json &request)
    {
        auto copy = request;
        copy.erase("id");
        const auto &methods = contract().at("x-methods");
        const auto it = methods.find(copy.at("method").get<std::string>());
        if(it != methods.end()){
            const auto &params = it->at("params");
            if(params.contains("properties")){
                for(const auto &[name, definition]: params.at("properties").items()){
                    if(definition.contains("default") && !copy.at("params").contains(name)){
                        copy["params"][name] = definition.at("default");
                    }
                }
            }
        }
        return copy.dump();
    }

    uint32_t decodeLength(std::span<const uint8_t, 4> h, size_t limit)
    {
        const uint32_t n = (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3];
        if(n == 0 || n > limit){ invalid(); }
        return n;
    }

    std::vector<uint8_t> encodeFrame(const Json &value, size_t limit)
    {
        const auto data = value.dump();
        if(data.empty() || data.size() > limit){ invalid(); }
        const auto n = static_cast<uint32_t>(data.size());
        std::vector<uint8_t> result{uint8_t(n >> 24), uint8_t(n >> 16), uint8_t(n >> 8), uint8_t(n)};
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    Json errorResponse(const Json &id, std::string_view code, const Json &context)
    {
        return {{"v", 1}, {"id", id}, {"ok", false}, {"error", {{"code", code}, {"message", "桥接请求未完成"}, {"details", Json::object()}}}, {"context", context}};
    }
}
