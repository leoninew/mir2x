#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

namespace aibridge
{
    using Json = nlohmann::json;
    inline constexpr size_t maxRequestBytes = 64 * 1024;
    inline constexpr size_t maxResponseBytes = 1024 * 1024;
    inline constexpr size_t maxJsonDepth = 16;

    class ProtocolError: public std::runtime_error
    {
        public:
            const std::string code;
            ProtocolError(std::string code, std::string message);
    };

    Json strictParse(std::string_view text);
    const Json &contract();
    void validateSchema(const Json &value, const Json &schema);
    void validateDefinition(const Json &value, std::string_view definition);
    Json decodeRequest(std::string_view text);
    bool isAction(std::string_view method);
    std::string canonicalRequest(const Json &request);
    std::vector<uint8_t> encodeFrame(const Json &value, size_t limit = maxResponseBytes);
    uint32_t decodeLength(std::span<const uint8_t, 4> header, size_t limit = maxRequestBytes);
    Json errorResponse(const Json &id, std::string_view code, const Json &context = nullptr);
}
