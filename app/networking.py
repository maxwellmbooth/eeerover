import socket
import threading
import time


class RoverConnection:
    def __init__(self, rover_ip, rover_port, app_port, timeout = 2.0, keys = ("age", "ir", "us", "mag")):
        self.rover_addr = (rover_ip, rover_port)
        self.app_port = app_port
        self.timeout = timeout
        self.keys = list(keys)

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._lock = threading.Lock()
        self._telemetry = {k: "--" for k in self.keys}
        self._last_rx = 0.0
        self._running = False
        self._thread = None

    # bind socket and start rx loop in thread
    def start(self):
        self._sock.bind(("0.0.0.0", self.app_port))
        self._sock.settimeout(1.0)
        self._running = True
        self._thread = threading.Thread(target = self._rx_loop, daemon = True)
        self._thread.start()

    # send control packet
    def send(self, throttle, steering, info):
        msg = f"{throttle:.2f},{steering:.2f},{info}".encode()
        try:
            self._sock.sendto(msg, self.rover_addr)
        except OSError:
            pass

    # return telemetry and if online
    def snapshot(self):
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
                parts = data.decode(errors = "replace").strip().split(",")
                with self._lock:
                    for i, k in enumerate(self.keys):
                        if i < len(parts):
                            self._telemetry[k] = parts[i] if parts[i] else "--"
                    self._last_rx = time.time()
            except socket.timeout:
                pass
            except ConnectionResetError:
                pass   # Windows: send to an unreachable rover resets the next recv -- ignore
            except OSError:
                if not self._running:
                    break
                self._reopen()   # socket went bad -> rebuild it and carry on

    def _reopen(self):
        try:
            self._sock.close()
        except OSError:
            pass
        while self._running:
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.bind(("0.0.0.0", self.app_port))
                s.settimeout(1.0)
                self._sock = s
                return
            except OSError:
                time.sleep(0.5)   # port not free yet -> wait and retry
