#include "../include/remote_protocol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) tokens.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) tokens.push_back(std::move(current));
    return tokens;
}

double finite(double value) { return std::isfinite(value) ? value : 0.0; }
} // namespace

bool parseRemoteCommand(std::string_view text, RemoteCommand &command) {
    if (text.size() > 64) return false;
    const std::vector<std::string> t = tokenize(text);
    if (t.empty() || t.size() > 2) return false;

    if (t[0] == "subscribe" && t.size() == 1) {
        command.type = RemoteCommand::Type::Subscribe;
        return true;
    }
    if (t[0] == "unsubscribe" && t.size() == 1) {
        command.type = RemoteCommand::Type::Unsubscribe;
        return true;
    }
    if (t.size() != 2) return false;

    if (t[0] == "throttle") {
        if (t[1] == "off") {
            command.type = RemoteCommand::Type::ThrottleRelease;
            return true;
        }
        char *end = nullptr;
        const double value = std::strtod(t[1].c_str(), &end);
        if (end == t[1].c_str() || *end != '\0' || !std::isfinite(value)) return false;
        command.type = RemoteCommand::Type::Throttle;
        command.value = std::clamp(value, 0.0, 1.0);
        return true;
    }
    if (t[0] == "starter" && (t[1] == "on" || t[1] == "off")) {
        command.type = RemoteCommand::Type::Starter;
        command.flag = t[1] == "on";
        return true;
    }
    if (t[0] == "ignition" && t[1] == "toggle") {
        command.type = RemoteCommand::Type::IgnitionToggle;
        return true;
    }
    if (t[0] == "gear" && (t[1] == "up" || t[1] == "down")) {
        command.type = RemoteCommand::Type::Gear;
        command.direction = t[1] == "up" ? 1 : -1;
        return true;
    }
    if (t[0] == "dyno" && t[1] == "toggle") {
        command.type = RemoteCommand::Type::DynoToggle;
        return true;
    }
    return false;
}

std::string formatRemoteTelemetry(const RemoteTelemetry &s) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
        "{\"rpm\":%.1f,\"throttle\":%.3f,\"speed_kph\":%.2f,\"torque_nm\":%.2f,"
        "\"gear\":%d,\"ignition\":%s,\"starter\":%s,\"dyno\":%s}\n",
        finite(s.rpm), finite(s.throttle), finite(s.speedKph), finite(s.torqueNm), s.gear,
        s.ignition ? "true" : "false", s.starter ? "true" : "false", s.dyno ? "true" : "false");
    return buffer;
}
