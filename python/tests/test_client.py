import socket
import threading
import unittest

from engine_sim_remote import RemoteClient, RemoteError, Telemetry

SAMPLE = (
    '{"rpm":1500.0,"throttle":0.250,"speed_kph":0.00,"torque_nm":1.50,'
    '"gear":2,"ignition":true,"starter":false,"dyno":false}\n'
)


class FakeSimulator:
    """Loopback UDP server that records commands and answers subscribe."""

    def __init__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(2.0)
        self.port = self.socket.getsockname()[1]
        self.received = []

    def receive(self, count):
        for _ in range(count):
            data, sender = self.socket.recvfrom(256)
            self.received.append(data.decode())
            if data == b"subscribe":
                self.socket.sendto(SAMPLE.encode(), sender)
        return self.received

    def close(self):
        self.socket.close()


class ClientTest(unittest.TestCase):
    def setUp(self):
        self.sim = FakeSimulator()
        self.addCleanup(self.sim.close)

    def test_commands_are_formatted(self):
        with RemoteClient(self.sim.port) as client:
            client.throttle(0.5)
            client.throttle(5)
            client.release_throttle()
            client.starter(True)
            client.starter(False)
            client.toggle_ignition()
            client.gear_up()
            client.gear_down()
            client.toggle_dyno()
            self.assertEqual(self.sim.receive(9), [
                "throttle 0.5000", "throttle 1.0000", "throttle off",
                "starter on", "starter off", "ignition toggle",
                "gear up", "gear down", "dyno toggle",
            ])

    def test_telemetry_subscribes_and_parses(self):
        with RemoteClient(self.sim.port) as client:
            worker = threading.Thread(target=self.sim.receive, args=(1,))
            worker.start()
            sample = client.telemetry()
            worker.join()
            self.assertEqual(sample, Telemetry(1500.0, 0.25, 0.0, 1.5, 2, True, False, False))
            self.assertEqual(self.sim.received, ["subscribe"])

    def test_close_unsubscribes(self):
        client = RemoteClient(self.sim.port)
        client.subscribe()
        client.close()
        self.assertEqual(self.sim.receive(2), ["subscribe", "unsubscribe"])

    def test_rejects_bad_input(self):
        with RemoteClient(self.sim.port) as client:
            with self.assertRaises(RemoteError):
                client.throttle(float("nan"))
            with self.assertRaises(RemoteError):
                client.throttle("high")
        with self.assertRaises(RemoteError):
            RemoteClient(0)
        with self.assertRaises(RemoteError):
            Telemetry.from_json("not json")


if __name__ == "__main__":
    unittest.main()
