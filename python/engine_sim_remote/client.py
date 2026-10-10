"""UDP client for the simulator's loopback remote-control interface.

Start the simulator with ``ENGINE_SIM_REMOTE_PORT=<port>`` and connect with
``RemoteClient(port)``. The protocol is one text command per datagram; after
``subscribe`` the simulator streams JSON telemetry lines to this socket.
"""

import json
import math
import socket
from dataclasses import dataclass
from typing import Iterator, Optional


class RemoteError(Exception):
    """Raised for invalid arguments or unusable telemetry."""


@dataclass(frozen=True)
class Telemetry:
    rpm: float
    throttle: float
    speed_kph: float
    torque_nm: float
    gear: int
    ignition: bool
    starter: bool
    dyno: bool

    @classmethod
    def from_json(cls, text: str) -> "Telemetry":
        try:
            data = json.loads(text)
            return cls(
                rpm=float(data["rpm"]),
                throttle=float(data["throttle"]),
                speed_kph=float(data["speed_kph"]),
                torque_nm=float(data["torque_nm"]),
                gear=int(data["gear"]),
                ignition=bool(data["ignition"]),
                starter=bool(data["starter"]),
                dyno=bool(data["dyno"]),
            )
        except (ValueError, KeyError, TypeError) as error:
            raise RemoteError(f"malformed telemetry: {text!r}") from error


class RemoteClient:
    """UDP client for the simulator's loopback remote-control interface."""

    def __init__(self, port: int, host: str = "127.0.0.1", timeout: float = 2.0):
        """Initialize a client connected to the simulator on ``host:port``."""
        if not 0 < port <= 65535:
            raise RemoteError(f"invalid port: {port}")
        self._address = (host, port)
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.settimeout(timeout)
        self._subscribed = False

    def __enter__(self) -> "RemoteClient":
        """Return the client for use as a context manager."""
        return self

    def __exit__(self, *exc) -> None:
        """Close the socket when leaving the context manager."""
        self.close()

    def close(self) -> None:
        """Close the UDP socket and unsubscribe if necessary."""
        if self._socket.fileno() != -1:
            if self._subscribed:
                try:
                    self._send("unsubscribe")
                except OSError:
                    pass
            self._socket.close()

    def _send(self, command: str) -> None:
        """Send a single ASCII command datagram to the simulator."""
        self._socket.sendto(command.encode("ascii"), self._address)

    # Controls.
    # Commands are fire-and-forget: the simulator sends no reply.
    # They pass through the loopback interface to the simulator.
    # All control commands are sent as plain ASCII strings.

    def throttle(self, value: float) -> None:
        """Hold the throttle at ``value`` (0..1) until released."""
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise RemoteError(f"throttle must be a finite number: {value!r}")
        self._send(f"throttle {min(max(float(value), 0.0), 1.0):.4f}")

    def release_throttle(self) -> None:
        """Release the throttle so it is driven by the engine logic again."""
        self._send("throttle off")

    def starter(self, on: bool) -> None:
        """Enable or disable the starter motor."""
        self._send("starter on" if on else "starter off")

    def toggle_ignition(self) -> None:
        """Toggle the ignition state."""
        self._send("ignition toggle")

    def gear_up(self) -> None:
        """Shift the transmission up one gear."""
        self._send("gear up")

    def gear_down(self) -> None:
        """Shift the transmission down one gear."""
        self._send("gear down")

    def toggle_dyno(self) -> None:
        """Toggle the dyno mode."""
        self._send("dyno toggle")

    # Telemetry.

    def subscribe(self) -> None:
        """Request that the simulator begin streaming telemetry packets."""
        self._send("subscribe")
        self._subscribed = True

    def unsubscribe(self) -> None:
        """Stop the simulator from streaming telemetry packets."""
        self._send("unsubscribe")
        self._subscribed = False

    def telemetry(self, timeout: Optional[float] = None) -> Telemetry:
        """Return the next telemetry sample, subscribing if necessary.

        Raises ``socket.timeout`` if nothing arrives in time.
        """
        if not self._subscribed:
            self.subscribe()
        previous = self._socket.gettimeout()
        if timeout is not None:
            self._socket.settimeout(timeout)
        try:
            data, _ = self._socket.recvfrom(1024)
        finally:
            self._socket.settimeout(previous)
        return Telemetry.from_json(data.decode("ascii", "replace"))

    def latest(self, timeout: Optional[float] = None) -> Telemetry:
        """Discard queued samples and return the freshest one."""
        sample = self.telemetry(timeout)
        previous = self._socket.gettimeout()
        self._socket.settimeout(0.0)
        try:
            while True:
                try:
                    data, _ = self._socket.recvfrom(1024)
                except (BlockingIOError, socket.timeout):
                    break
                # Update the sample with the latest telemetry data.
                # Keep receiving until no more data is available to get the latest sample.
                # Decode as ASCII, replacing any invalid characters.
                sample = Telemetry.from_json(data.decode("ascii", "replace"))
        finally:
            self._socket.settimeout(previous)
        return sample

    def stream(self) -> Iterator[Telemetry]:
        """Yield telemetry samples until the socket times out or is closed."""
        while True:
            try:
                yield self.telemetry()
            except (socket.timeout, OSError):
                return
