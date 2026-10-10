#include <gtest/gtest.h>

#include "../include/remote_protocol.h"

TEST(RemoteProtocol, ParsesThrottleAndClampsRange) {
    RemoteCommand c;
    ASSERT_TRUE(parseRemoteCommand("throttle 0.25", c));
    EXPECT_EQ(c.type, RemoteCommand::Type::Throttle);
    EXPECT_DOUBLE_EQ(c.value, 0.25);
    ASSERT_TRUE(parseRemoteCommand("throttle 7", c));
    EXPECT_DOUBLE_EQ(c.value, 1.0);
    ASSERT_TRUE(parseRemoteCommand("throttle off\n", c));
    EXPECT_EQ(c.type, RemoteCommand::Type::ThrottleRelease);
}

TEST(RemoteProtocol, ParsesDiscreteCommands) {
    RemoteCommand c;
    ASSERT_TRUE(parseRemoteCommand("starter on", c));
    EXPECT_TRUE(c.flag);
    ASSERT_TRUE(parseRemoteCommand("gear down", c));
    EXPECT_EQ(c.direction, -1);
    ASSERT_TRUE(parseRemoteCommand("ignition toggle", c));
    EXPECT_EQ(c.type, RemoteCommand::Type::IgnitionToggle);
    ASSERT_TRUE(parseRemoteCommand("subscribe", c));
    EXPECT_EQ(c.type, RemoteCommand::Type::Subscribe);
}

TEST(RemoteProtocol, RejectsMalformedInput) {
    RemoteCommand c;
    EXPECT_FALSE(parseRemoteCommand("", c));
    EXPECT_FALSE(parseRemoteCommand("throttle", c));
    EXPECT_FALSE(parseRemoteCommand("throttle abc", c));
    EXPECT_FALSE(parseRemoteCommand("throttle nan", c));
    EXPECT_FALSE(parseRemoteCommand("starter maybe", c));
    EXPECT_FALSE(parseRemoteCommand("gear up extra", c));
    EXPECT_FALSE(parseRemoteCommand(std::string(200, 'a'), c));
}

TEST(RemoteProtocol, FormatsTelemetryAsJsonLine) {
    RemoteTelemetry t;
    t.rpm = 1500.0;
    t.gear = 2;
    t.ignition = true;
    const std::string text = formatRemoteTelemetry(t);
    EXPECT_NE(text.find("\"rpm\":1500.0"), std::string::npos);
    EXPECT_NE(text.find("\"gear\":2"), std::string::npos);
    EXPECT_NE(text.find("\"ignition\":true"), std::string::npos);
    EXPECT_EQ(text.back(), '\n');
}
