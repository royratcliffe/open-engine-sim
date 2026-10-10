#ifndef ATG_ENGINE_SIM_REMOTE_CONTROL_H
#define ATG_ENGINE_SIM_REMOTE_CONTROL_H

#include <chrono>
#include <cstdint>

class EngineSimApplication;

// Opt-in UDP remote control and telemetry, bound to the loopback interface.
// Polled from the host's main loop; it owns no threads and never touches the
// audio path.
class RemoteControl {
    public:
        RemoteControl() = default;
        ~RemoteControl();
        RemoteControl(const RemoteControl &) = delete;
        RemoteControl &operator=(const RemoteControl &) = delete;

        bool open(std::uint16_t port);
        void close();
        bool isOpen() const { return m_socket != kInvalid; }
        void poll(EngineSimApplication &application);

    private:
#if defined(_WIN32)
        using Handle = std::uintptr_t;
        static constexpr Handle kInvalid = ~static_cast<Handle>(0);
#else
        using Handle = int;
        static constexpr Handle kInvalid = -1;
#endif
        Handle m_socket = kInvalid;
        bool m_wsaStarted = false;
        bool m_subscribed = false;
        // Opaque storage for the subscriber's sockaddr_in.
        alignas(8) unsigned char m_subscriber[32] = {};
        int m_subscriberLength = 0;
        std::chrono::steady_clock::time_point m_lastTelemetry{};
};

#endif
