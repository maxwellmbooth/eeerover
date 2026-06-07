import socket
import threading
import time


class RoverLink:
    """UDP link to the rover: a background receive thread plus a send helper.

    The GUI never touches sockets directly. It calls send() each frame and
    reads the latest telemetry with snapshot(), which is safe to call from
    the main loop while the receive thread runs in the background.
    """

    def __init__(self, rover_ip, rover_port, app_port, timeout=2.0,
                 keys=("age", "ir", "us", "mag")):
        self.rover_addr = (rover_ip, rover_port)
        self.app_port = app_port
        self.timeout = timeout       # seconds with no telemetry => offline
        self.keys = list(keys)       # field order in the rover's packet

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._lock = threading.Lock()
        self._telemetry = {k: "--" for k in self.keys}
        self._last_rx = 0.0
        self._running = False
        self._thread = None

    def start(self):
        """Bind the socket and start receiving telemetry in the background."""
        self._sock.bind(("0.0.0.0", self.app_port))
        self._sock.settimeout(1.0)
        self._running = True
        self._thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._thread.start()

    def send(self, throttle, steering, info):
        """Send one control packet. A dropped packet is ignored, not raised."""
        msg = f"{throttle:.2f},{steering:.2f},{info}".encode()
        try:
            self._sock.sendto(msg, self.rover_addr)
        except OSError:
            pass

    def snapshot(self):
        """Return (telemetry_copy, online). Safe to call from the GUI thread."""
        with self._lock:
            tele = dict(self._telemetry)
            online = (time.time() - self._last_rx) < self.timeout
        return tele, online

    def close(self):
        self._running = False
        self._sock.close()

    def _rx_loop(self):
        while self._running:
            try:
                data, _ = self._sock.recvfrom(512)
                parts = data.decode().strip().split(",")
                with self._lock:
                    for i, k in enumerate(self.keys):
                        if i < len(parts):
                            self._telemetry[k] = parts[i] if parts[i] else "--"
                    self._last_rx = time.time()
            except socket.timeout:
                pass
            except OSError:
                break   # socket closed by close()
