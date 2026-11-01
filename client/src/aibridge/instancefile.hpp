#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace aibridge
{
    struct InstanceInfo
    {
        int v = 1;
        std::string instance_id;
        std::string token;
        std::string pid;
        std::string started_at;
        std::uint16_t port = 0;
        std::string build;
    };

    class InstanceFile
    {
    public:
        static std::unique_ptr<InstanceFile> create(
            const std::filesystem::path &absoluteDirectory,
            std::uint16_t loopbackPort, std::string build);
        ~InstanceFile();

        const InstanceInfo &info() const noexcept;
        const std::filesystem::path &path() const noexcept;
        bool cleanup() noexcept;

        InstanceFile(const InstanceFile &) = delete;
        InstanceFile &operator=(const InstanceFile &) = delete;

    private:
        struct Impl;
        explicit InstanceFile(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> m_impl;
    };
}
