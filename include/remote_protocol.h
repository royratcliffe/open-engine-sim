#ifndef ATG_ENGINE_SIM_REMOTE_PROTOCOL_H
#define ATG_ENGINE_SIM_REMOTE_PROTOCOL_H

#include <string>
#include <string_view>

// Text protocol for the optional remote-control interface. Free of sockets and
// simulator types so it can be tested in isolation.
//
// Commands (one per datagram, case-sensitive):
//   throttle <0..1> | throttle off
//   starter on|off
//   ignition toggle
//   gear up|down
//   dyno toggle
//   subscribe | unsubscribe
struct RemoteCommand {
    enum class Type {
        Throttle,
        ThrottleRelease,
        Starter,
        IgnitionToggle,
        Gear,
        DynoToggle,
        Subscribe,
        Unsubscribe
    };

    Type type = Type::Subscribe;
    double value = 0.0; // throttle
    bool flag = false;  // starter state
    int direction = 0;  // gear change
};

struct RemoteTelemetry {
    double rpm = 0.0;
    double throttle = 0.0;
    double speedKph = 0.0;
    double torqueNm = 0.0;
    int gear = -1;
    bool ignition = false;
    bool starter = false;
    bool dyno = false;
};

bool parseRemoteCommand(std::string_view text, RemoteCommand &command);
std::string formatRemoteTelemetry(const RemoteTelemetry &telemetry);

#endif
