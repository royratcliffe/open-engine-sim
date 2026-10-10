#include "../include/remote_control.h"

#include "../include/engine_sim_application.h"
#include "../include/remote_protocol.h"
#include "../include/units.h"

#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketLength = int;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketLength = socklen_t;
#endif

namespace {
constexpr auto kTelemetryInterval = std::chrono::milliseconds(50);

RemoteTelemetry collectTelemetry(EngineSimApplication &application) {
    RemoteTelemetry t;
    Simulator *simulator = application.getSimulator();
    if (simulator == nullptr || simulator->getEngine() == nullptr) return t;

    Engine *engine = simulator->getEngine();
    t.rpm = engine->getRpm();
    t.throttle = application.getThrottle();
    t.torqueNm = units::convert(simulator->getFilteredDynoTorque(), units::Nm);
    t.ignition = engine->getIgnitionModule()->m_enabled;
    t.starter = simulator->m_starterMotor.m_enabled;
    t.dyno = simulator->m_dyno.m_enabled;
    if (simulator->getTransmission() != nullptr) t.gear = simulator->getTransmission()->getGear();
    if (simulator->getVehicle() != nullptr) {
        t.speedKph = units::convert(simulator->getVehicle()->getSpeed(), units::km / units::hour);
    }
    return t;
}

void apply(EngineSimApplication &application, const RemoteCommand &command) {
    switch (command.type) {
    case RemoteCommand::Type::Throttle: application.setTouchThrottle(command.value, true); break;
    case RemoteCommand::Type::ThrottleRelease: application.setTouchThrottle(0.0, false); break;
    case RemoteCommand::Type::Starter: application.setTouchStarterHeld(command.flag); break;
    case RemoteCommand::Type::IgnitionToggle: application.toggleIgnition(); break;
    case RemoteCommand::Type::Gear: application.changeGear(command.direction); break;
    case RemoteCommand::Type::DynoToggle: application.toggleDynamometer(); break;
    default: break;
    }
}
} // namespace

RemoteControl::~RemoteControl() { close(); }

bool RemoteControl::open(std::uint16_t port) {
    close();
#if defined(_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    m_wsaStarted = true;
#endif
    const Handle handle = static_cast<Handle>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (handle == kInvalid) {
        close();
        return false;
    }
    m_socket = handle;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        close();
        return false;
    }

#if defined(_WIN32)
    u_long nonBlocking = 1;
    if (::ioctlsocket(handle, FIONBIO, &nonBlocking) != 0) {
#else
    if (::fcntl(handle, F_SETFL, ::fcntl(handle, F_GETFL, 0) | O_NONBLOCK) != 0) {
#endif
        close();
        return false;
    }
    return true;
}

void RemoteControl::close() {
    if (m_socket != kInvalid) {
#if defined(_WIN32)
        ::closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = kInvalid;
    }
#if defined(_WIN32)
    if (m_wsaStarted) WSACleanup();
    m_wsaStarted = false;
#endif
    m_subscribed = false;
}

void RemoteControl::poll(EngineSimApplication &application) {
    if (!isOpen()) return;

    for (int i = 0; i < 64; ++i) {
        char buffer[128];
        sockaddr_in sender{};
        SocketLength senderLength = sizeof(sender);
        const int received = static_cast<int>(::recvfrom(m_socket, buffer,
            static_cast<int>(sizeof(buffer)) - 1, 0,
            reinterpret_cast<sockaddr *>(&sender), &senderLength));
        if (received <= 0) break;

        RemoteCommand command;
        if (!parseRemoteCommand(std::string_view(buffer, static_cast<size_t>(received)), command)) continue;

        if (command.type == RemoteCommand::Type::Subscribe) {
            std::memcpy(m_subscriber, &sender, sizeof(sender));
            m_subscriberLength = static_cast<int>(senderLength);
            m_subscribed = true;
        } else if (command.type == RemoteCommand::Type::Unsubscribe) {
            m_subscribed = false;
        } else {
            apply(application, command);
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_subscribed && now - m_lastTelemetry >= kTelemetryInterval) {
        m_lastTelemetry = now;
        const std::string message = formatRemoteTelemetry(collectTelemetry(application));
        ::sendto(m_socket, message.data(), static_cast<int>(message.size()), 0,
            reinterpret_cast<const sockaddr *>(m_subscriber),
            static_cast<SocketLength>(m_subscriberLength));
    }
}
