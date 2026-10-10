# engine-sim-remote

A dependency-free Python client (3.8+) for the Open Engine Simulator's UDP
remote-control and telemetry interface.

## Enable the interface

The simulator's remote interface is off by default. Start the desktop app with
a port to bind `127.0.0.1` (loopback only, no authentication):

```powershell
$env:ENGINE_SIM_REMOTE_PORT = "49555"
.\engine-sim-desktop.exe
```

## Install

```sh
pip install ./python
```

Or, without installing, run with `PYTHONPATH=python`.

## Quick start

```python
from engine_sim_remote import RemoteClient

with RemoteClient(49555) as car:
    car.toggle_ignition()
    car.starter(True)
    car.throttle(0.6)
    print(car.latest())
    car.starter(False)
    car.release_throttle()
```

## API

### `RemoteClient(port, host="127.0.0.1", timeout=2.0)`

Raises `RemoteError` if `port` is outside 1-65535. `timeout` is the default
receive timeout in seconds. The client is a context manager; `close()`
unsubscribes (if subscribed) and closes the socket.

Control methods are fire-and-forget: the simulator sends no acknowledgement, and
UDP delivery is not guaranteed.

| Method | Effect |
|---|---|
| `throttle(value)` | Hold the throttle at `value`, clamped to 0..1. Raises `RemoteError` for non-finite or non-numeric values. |
| `release_throttle()` | Return throttle control to the keyboard/UI. |
| `starter(on)` | Engage or release the starter motor. |
| `toggle_ignition()` | Toggle the ignition. This is a toggle, not a set; read `Telemetry.ignition` for the current state. |
| `gear_up()` / `gear_down()` | Change gear by one. |
| `toggle_dyno()` | Toggle the dynamometer. |

Telemetry methods:

| Method | Effect |
|---|---|
| `subscribe()` / `unsubscribe()` | Start or stop the telemetry stream (about 20 samples/s) to this client. |
| `telemetry(timeout=None)` | Next sample; subscribes first if needed. Raises `socket.timeout` if none arrives. |
| `latest(timeout=None)` | Discards queued samples and returns the newest. |
| `stream()` | Generator of samples; ends on timeout or when the socket closes. |

The simulator keeps one subscriber at a time; the most recent `subscribe` wins.

### `Telemetry`

Frozen dataclass:

| Field | Type | Unit / meaning |
|---|---|---|
| `rpm` | float | Engine speed |
| `throttle` | float | 0..1 |
| `speed_kph` | float | Vehicle speed |
| `torque_nm` | float | Filtered dynamometer torque |
| `gear` | int | Current gear (`-1` = neutral) |
| `ignition` | bool | |
| `starter` | bool | |
| `dyno` | bool | Dynamometer enabled |

### `RemoteError`

Raised for invalid arguments and malformed telemetry.

## Wire protocol

One ASCII command per datagram (max 64 characters): `throttle <0..1>`,
`throttle off`, `starter on|off`, `ignition toggle`, `gear up|down`,
`dyno toggle`, `subscribe`, `unsubscribe`. Telemetry is one JSON object per
datagram, newline-terminated. Malformed commands are ignored.

## Tests

```sh
cd python
PYTHONPATH=. python -m unittest discover -s tests
```
