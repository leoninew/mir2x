#include "transport.hpp"
#include "instancefile.hpp"
#include "policy.hpp"
#include <asio.hpp>
#include <algorithm>
#include <array>
#include <deque>
#include <limits>

namespace aibridge
{
    using Clock = std::chrono::steady_clock;
    using Tcp = asio::ip::tcp;

    struct AiBridgeTransport::Impl
    {
        struct Connection: std::enable_shared_from_this<Connection>
        {
            Impl &owner;
            Tcp::socket socket;
            uint64_t id;
            bool auth = false;
            bool stopped = false;
            std::array<uint8_t, 4> header{};
            std::string body;
            std::deque<std::shared_ptr<std::vector<uint8_t>>> output;
            size_t outputBytes = 0;
            size_t rateCount = 0;
            Clock::time_point created = Clock::now();
            Clock::time_point lastActivity = created;
            Clock::time_point rateWindow = created;
            Clock::time_point writeStarted = created;

            Connection(Impl &o, Tcp::socket s, uint64_t n): owner(o), socket(std::move(s)), id(n) {}

            void stop()
            {
                if(stopped){ return; }
                stopped = true;
                asio::error_code ec;
                socket.cancel(ec);
                socket.close(ec);
                output.clear();
                outputBytes = 0;
                if(auth){
                    // Only one authenticated connection exists; coalesce until consumed.
                    owner.disconnected = id;
                    owner.requests.clear();
                    owner.priorityStop.reset();
                    owner.active.reset();
                }
            }

            void readHeader()
            {
                if(stopped){ return; }
                asio::async_read(socket, asio::buffer(header), [self = shared_from_this()](auto ec, size_t)
                {
                    if(self->stopped){ return; }
                    if(ec){ self->stop(); return; }
                    try{ self->body.resize(decodeLength(self->header)); }
                    catch(const ProtocolError &){ self->stop(); return; }
                    self->readBody();
                });
            }

            void readBody()
            {
                asio::async_read(socket, asio::buffer(body), [self = shared_from_this()](auto ec, size_t)
                {
                    if(self->stopped){ return; }
                    if(ec){ self->stop(); return; }
                    self->receive();
                    self->readHeader();
                });
            }

            bool send(const Json &value)
            {
                if(stopped){ return false; }
                std::shared_ptr<std::vector<uint8_t>> frame;
                try{ frame = std::make_shared<std::vector<uint8_t>>(encodeFrame(value)); }
                catch(const std::exception &){ stop(); return false; }
                if(frame->size() > Limits::outgoingBytes - outputBytes){ stop(); return false; }
                const bool idle = output.empty();
                outputBytes += frame->size();
                output.push_back(std::move(frame));
                if(idle){ writeNext(); }
                return true;
            }

            void writeNext()
            {
                if(stopped || output.empty()){ return; }
                writeStarted = Clock::now();
                auto frame = output.front();
                asio::async_write(socket, asio::buffer(*frame), [self = shared_from_this(), frame](auto ec, size_t)
                {
                    if(self->stopped){ return; }
                    if(ec){ self->stop(); return; }
                    self->outputBytes -= frame->size();
                    self->output.pop_front();
                    self->writeNext();
                });
            }

            static bool secretEqual(std::string_view a, std::string_view b)
            {
                if(a.size() != b.size()){ return false; }
                unsigned difference = 0;
                for(size_t i = 0; i < a.size(); ++i){ difference |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]); }
                return difference == 0;
            }

            void receive()
            {
                Json message;
                try{ message = decodeRequest(body); }
                catch(const ProtocolError &error){
                    if(auth){
                        Json responseId = nullptr;
                        try{
                            const auto parsed = strictParse(body);
                            if(parsed.is_object() && parsed.contains("id") && parsed.at("id").is_string()){
                                const auto &idText = parsed.at("id").get_ref<const std::string &>();
                                if(!idText.empty() && idText.size() <= 64){ responseId = idText; }
                            }
                        }catch(const ProtocolError &){}
                        const auto now = Clock::now();
                        if(now - rateWindow >= std::chrono::seconds(1)){ rateWindow = now; rateCount = 0; }
                        if(++rateCount > Limits::requestsPerSecond){ stop(); }
                        else{ send(errorResponse(responseId, error.code)); }
                    }
                    else{ stop(); }
                    return;
                }
                const auto method = message.at("method").get<std::string>();
                if(!auth){
                    const auto &info = owner.file->info();
                    if(method != "hello" || !owner.active.expired() || owner.disconnected.has_value()
                        || message.at("params").at("instance_id") != info.instance_id
                        || !secretEqual(message.at("params").at("token").get<std::string>(), info.token)){
                        stop(); return;
                    }
                    auth = true;
                    owner.active = shared_from_this();
                    lastActivity = Clock::now();
                    send({{"v", 1}, {"id", message.at("id")}, {"ok", true}, {"data", {{"instance_id", info.instance_id}, {"protocol_version", 1}}}, {"context", nullptr}});
                    return;
                }
                if(method == "hello"){ stop(); return; }
                const auto now = Clock::now();
                lastActivity = now;
                if(now - rateWindow >= std::chrono::seconds(1)){ rateWindow = now; rateCount = 0; }
                if(method != "game_stop" && ++rateCount > Limits::requestsPerSecond){
                    send(errorResponse(message.at("id"), "busy")); return;
                }
                if(method == "heartbeat"){
                    send({{"v", 1}, {"id", message.at("id")}, {"ok", true}, {"data", {{"alive", true}}}, {"context", nullptr}});
                    return;
                }
                if(method == "game_stop"){
                    if(owner.priorityStop){ send(errorResponse(message.at("id"), "busy")); }
                    else{ owner.priorityStop = BridgeRequest{id, std::move(message)}; }
                    return;
                }
                if(owner.requests.size() >= Limits::pendingRequests){ send(errorResponse(message.at("id"), "busy")); return; }
                owner.requests.push_back({id, std::move(message)});
            }
        };

        asio::io_context io;
        Tcp::acceptor acceptor{io};
        std::unique_ptr<InstanceFile> file;
        std::optional<Policy> policy;
        std::vector<std::shared_ptr<Connection>> connections;
        std::weak_ptr<Connection> active;
        std::deque<BridgeRequest> requests;
        std::optional<BridgeRequest> priorityStop;
        std::optional<uint64_t> disconnected;
        uint64_t nextId = 0;
        bool running = false;

        void accept()
        {
            acceptor.async_accept([this](auto ec, Tcp::socket socket)
            {
                if(!running){ return; }
                if(!ec){
                    const auto pending = std::count_if(connections.begin(), connections.end(), [](const auto &c){ return !c->stopped && !c->auth; });
                    if(pending < static_cast<ptrdiff_t>(Limits::pendingConnections) && nextId != std::numeric_limits<uint64_t>::max()){
                        auto c = std::make_shared<Connection>(*this, std::move(socket), ++nextId);
                        connections.push_back(c);
                        c->readHeader();
                    }
                    else{
                        asio::error_code ignored;
                        socket.close(ignored);
                    }
                }
                if(running && ec != asio::error::operation_aborted){ accept(); }
            });
        }

        void close() noexcept
        {
            running = false;
            asio::error_code ec;
            acceptor.cancel(ec);
            acceptor.close(ec);
            for(auto &c: connections){ c->stop(); }
            io.stop();
            requests.clear();
            priorityStop.reset();
            if(file){ file->cleanup(); file.reset(); }
            policy.reset();
        }
    };

    AiBridgeTransport::AiBridgeTransport(): m_impl(std::make_unique<Impl>()) {}
    AiBridgeTransport::~AiBridgeTransport(){ close(); }

    void AiBridgeTransport::start(const Options &options)
    {
        close();
        const auto lastId = m_impl->nextId;
        // Destroy cancelled handlers before constructing a new transport generation.
        m_impl = std::make_unique<Impl>();
        m_impl->nextId = lastId;
        try{
            m_impl->policy = Policy::load(options.policyPath);
            m_impl->acceptor.open(Tcp::v4());
            m_impl->acceptor.bind({asio::ip::make_address_v4("127.0.0.1"), 0});
            m_impl->acceptor.listen(4);
            m_impl->file = InstanceFile::create(options.directory, m_impl->acceptor.local_endpoint().port(), options.build);
            m_impl->running = true;
            m_impl->accept();
        }
        catch(...){ close(); throw; }
    }

    void AiBridgeTransport::close() noexcept{ if(m_impl){ m_impl->close(); } }

    size_t AiBridgeTransport::pollBudget(size_t maxHandlers, std::chrono::microseconds budget)
    {
        if(!m_impl->running){ return 0; }
        maxHandlers = std::min(maxHandlers, size_t{16});
        budget = std::min(budget, std::chrono::microseconds{2000});
        const auto start = Clock::now();
        for(auto &c: m_impl->connections){
            if((!c->auth && start - c->created >= std::chrono::seconds(3))
                || (c->auth && start - c->lastActivity >= std::chrono::seconds(6))
                || (!c->output.empty() && start - c->writeStarted >= std::chrono::seconds(6))){ c->stop(); }
        }
        m_impl->io.restart();
        size_t count = 0;
        while(count < maxHandlers && Clock::now() - start < budget){
            if(!m_impl->io.poll_one()){ break; }
            ++count;
        }
        std::erase_if(m_impl->connections, [](const auto &c){ return c->stopped; });
        return count;
    }

    std::vector<BridgeRequest> AiBridgeTransport::takeRequests(size_t limit)
    {
        limit = std::min(limit, size_t{8});
        std::vector<BridgeRequest> result;
        if(limit && m_impl->priorityStop){ result.push_back(std::move(*m_impl->priorityStop)); m_impl->priorityStop.reset(); }
        while(result.size() < limit && !m_impl->requests.empty()){
            result.push_back(std::move(m_impl->requests.front()));
            m_impl->requests.pop_front();
        }
        return result;
    }

    bool AiBridgeTransport::sendResponse(uint64_t id, const Json &response)
    {
        auto c = m_impl->active.lock();
        return c && c->id == id && c->send(response);
    }

    std::vector<uint64_t> AiBridgeTransport::takeDisconnected()
    {
        std::vector<uint64_t> result;
        if(m_impl->disconnected){ result.push_back(*m_impl->disconnected); m_impl->disconnected.reset(); }
        return result;
    }
    const InstanceInfo *AiBridgeTransport::instance() const noexcept{ return m_impl->file ? &m_impl->file->info() : nullptr; }
    const Policy *AiBridgeTransport::policy() const noexcept{ return m_impl->policy ? &*m_impl->policy : nullptr; }
    bool AiBridgeTransport::authenticated() const noexcept{ return !m_impl->active.expired(); }
    size_t AiBridgeTransport::queuedRequests() const noexcept{ return m_impl->requests.size() + (m_impl->priorityStop ? 1 : 0); }
    size_t AiBridgeTransport::queuedOutputBytes() const noexcept{ auto c = m_impl->active.lock(); return c ? c->outputBytes : 0; }
}
