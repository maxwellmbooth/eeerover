import os
os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "1"

import math
import pygame
import pygame.gfxdraw
from collections import deque

from networking import RoverConnection

# Config
ROVER_IP = "192.168.1.1"   # use "127.0.0.1" to test against fake_rover.py
ROVER_PORT = 4810
APP_PORT = 4811

WIDTH, HEIGHT = 900, 600
FPS = 60
CONNECTION_TIMEOUT = 2.0   # seconds with no telemetry => OFFLINE
DEADZONE = 0.08      # controller stick deadzone

# Palette
BG = (31, 31, 30)
PANEL = (38, 38, 38)
PANEL_HI = (49, 49, 49)
STROKE = (49, 49, 49)
TEXT = (216, 216, 214)
TEXT_DIM = (194, 193, 182)
ACCENT = (94, 234, 212)    # teal
GOOD = (74, 222, 128)      # green
BAD = (248, 113, 113)      # red
WARN = (251, 191, 36)      # amber
VIOLET = (167, 139, 250)
BLUE = (96, 165, 250)
CONSOLE_BG = (12, 12, 12)
CONSOLE_BORDER = (25, 24, 26)

# Rock types
ROCK_TYPES = {
    ("547", "Detected", "Down"): "Basaltoid",
    ("312", "Not detected", "Down"): "Gravion",
    ("312", "Detected", "Up"): "Regolix",
    ("547", "Not detected", "Up"): "Lunarite",
}

def classify_rock(tele):
    return ROCK_TYPES.get((tele["ir"], tele["us"], tele["mag"]), "Unknown")

# UI helper functions
def aa_circle(surf, x, y, r, color, border = None, bw = 0):
    """Anti-aliased filled circle (much smoother than pygame.draw.circle)."""
    x, y, r = int(x), int(y), int(r)
    pygame.gfxdraw.filled_circle(surf, x, y, r, color)
    pygame.gfxdraw.aacircle(surf, x, y, r, color)
    if border and bw:
        for i in range(bw):
            pygame.gfxdraw.aacircle(surf, x, y, r - i, border)

def panel(surf, rect, radius = 14, fill = PANEL, border = STROKE, bw = 1):
    pygame.draw.rect(surf, fill, rect, border_radius = radius)
    if bw:
        pygame.draw.rect(surf, border, rect, width = bw, border_radius = radius)

def text(surf, s, font, color, pos, anchor = "topleft"):
    img = font.render(str(s), True, color)
    r = img.get_rect(**{anchor: pos})
    surf.blit(img, r)
    return r

def deadzone(v, dz = DEADZONE):
    return 0.0 if abs(v) < dz else v

# UI widgets
def status_pill(surf, right_x, cy, online, fonts):
    label = "ONLINE" if online else "OFFLINE"
    col = GOOD if online else BAD
    img = fonts["label"].render(label, True, col)
    pad, dot_r, gap, h = 14, 5, 8, 30
    w = pad + dot_r * 2 + gap + img.get_width() + pad
    rect = pygame.Rect(0, 0, w, h)
    rect.midright = (right_x, cy)
    panel(surf, rect, radius = h // 2, fill = PANEL_HI)
    cx_dot = rect.x + pad + dot_r
    aa_circle(surf, cx_dot, rect.centery, dot_r, col)
    surf.blit(img, (cx_dot + dot_r + gap, rect.centery - img.get_height() // 2))

def value_card(surf, rect, label, value, sub, accent, fonts):
    panel(surf, rect)
    x, y, w, h = rect
    pygame.draw.rect(surf, accent, (x, y + 18, 4, 22), border_radius=2)   # accent bar
    text(surf, label.upper(), fonts["label"], TEXT_DIM, (x + 20, y + 18))
    text(surf, value, fonts["value"], TEXT, (x + 18, y + 50))
    text(surf, sub, fonts["sub"], TEXT_DIM, (x + 20, y + h - 28))

class VirtualJoystick:
    """Mouse-draggable when no controller; mirrors the gamepad otherwise."""
    def __init__(self, cx, cy, radius, knob_r = 28):
        self.cx, self.cy = cx, cy
        self.radius = radius
        self.knob_r = knob_r
        self.dragging = False
        self.x = 0.0   # -1..1 left/right -> steering
        self.y = 0.0   # -1..1 up/down -> throttle = -y

    @property
    def throttle(self):
        return -self.y

    @property
    def steering(self):
        return self.x

    def set_from_controller(self, steering, throttle):
        self.x = max(-1.0, min(1.0, steering))
        self.y = max(-1.0, min(1.0, -throttle))

    def _from_mouse(self, pos):
        dx, dy = pos[0] - self.cx, pos[1] - self.cy
        dist = math.hypot(dx, dy)
        if dist > self.radius:
            dx, dy = dx / dist * self.radius, dy / dist * self.radius
        self.x, self.y = dx / self.radius, dy / self.radius

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if math.hypot(event.pos[0] - self.cx, event.pos[1] - self.cy) <= self.radius + self.knob_r:
                self.dragging = True
                self._from_mouse(event.pos)
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1 and self.dragging:
            self.dragging = False
            self.x = self.y = 0.0   # spring back to centre on release
        elif event.type == pygame.MOUSEMOTION and self.dragging:
            self._from_mouse(event.pos)

    def draw(self, surf, active):
        aa_circle(surf, self.cx, self.cy, self.radius, PANEL_HI, STROKE, 2)
        pygame.draw.line(surf, STROKE, (self.cx - self.radius, self.cy), (self.cx + self.radius, self.cy), 1)
        pygame.draw.line(surf, STROKE, (self.cx, self.cy - self.radius), (self.cx, self.cy + self.radius), 1)
        kx = self.cx + self.x * self.radius
        ky = self.cy + self.y * self.radius
        aa_circle(surf, kx, ky, self.knob_r, TEXT if active else TEXT_DIM, BG, 2)

class Console:
    """Scrolling text monitor. Call write(line) to add a line; it auto-scrolls."""
    def __init__(self, rect, max_lines=200):
        self.rect = pygame.Rect(rect)
        self.lines = deque(maxlen=max_lines)

    def write(self, line):
        self.lines.append(str(line))

    def draw(self, surf, font):
        panel(surf, self.rect, fill = CONSOLE_BG, border = CONSOLE_BORDER)
        pad = 14
        line_h = font.get_linesize()
        visible = max(1, (self.rect.height - 2 * pad) // line_h)
        y = self.rect.y + pad
        for ln in list(self.lines)[-visible:]:
            text(surf, ln, font, TEXT_DIM, (self.rect.x + pad, y))
            y += line_h

# App main
def app_init(rover):
    pygame.init()
    pygame.joystick.init()

    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("mrover26")
    clock = pygame.time.Clock()

    family = "Segoe UI,Helvetica Neue,Arial"
    fonts = {
        "title": pygame.font.SysFont(family, 26, bold = True),
        "label": pygame.font.SysFont(family, 14, bold = True),
        "value": pygame.font.SysFont(family, 34, bold = True),
        "sub": pygame.font.SysFont(family, 13),
        "small": pygame.font.SysFont(family, 14),
        "mono": pygame.font.SysFont("Consolas,Menlo,Courier New,monospace", 13),
    }

    pygame_joystick = pygame.joystick.Joystick(0) if pygame.joystick.get_count() > 0 else None
    joystick = VirtualJoystick(cx = 726, cy = 248, radius = 95)
    console = Console((24, 428, 534, HEIGHT - 428 - 24))   # strip under the card grid

    info = 0
    was_online = False
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running, info = False, 1
            elif event.type == pygame.JOYDEVICEADDED:
                pygame_joystick = pygame.joystick.Joystick(event.device_index)
            elif event.type == pygame.JOYDEVICEREMOVED:
                pygame_joystick = None
            elif pygame_joystick is None:
                joystick.handle_event(event)   # on-screen stick only when no pad

        if pygame_joystick:   # read control source
            throttle = deadzone(-pygame_joystick.get_axis(1))
            steering = deadzone(pygame_joystick.get_axis(0))
            joystick.set_from_controller(steering, throttle)
            source = "GAMEPAD"
        else:
            throttle, steering = joystick.throttle, joystick.steering
            source = "MOUSE"

        rover.send(throttle, steering, info)
        tele, online = rover.snapshot()
        if online != was_online:
            console.write("link up" if online else "link lost")
            was_online = online

        screen.fill(BG)   # draw frame

        bar = pygame.Rect(24, 20, WIDTH - 48, 52)   # top bar
        panel(screen, bar, radius = 14, fill = PANEL)
        text(screen, "mrover26", fonts["title"], TEXT, (bar.x + 20, bar.centery), "midleft")
        status_pill(screen, bar.right - 16, bar.centery, online, fonts)

        cw, ch, gap, x0, y0 = 258, 150, 18, 24, 92   # telemetry card grid
        cards = [
            ("Age", tele["age"], "billion years", ACCENT),
            ("Radioactivity", tele["ir"], "IR pulses /s", WARN),
            ("Ultrasound", tele["us"], "40 kHz echo", VIOLET),
            ("Magnetic", tele["mag"], "field direction", BLUE),
        ]
        
        for i, (lab, val, sub, acc) in enumerate(cards):
            col, row = i % 2, i // 2
            rect = (x0 + col * (cw + gap), y0 + row * (ch + gap), cw, ch)
            value_card(screen, rect, lab, val, sub, acc, fonts)

        joystick_panel = pygame.Rect(576, 92, 300, 318)   # joystick panel
        panel(screen, joystick_panel, radius = 14)
        text(screen, "MANUAL CONTROL", fonts["label"], TEXT_DIM, (joystick_panel.x + 20, joystick_panel.y + 18))
        joystick.draw(screen, active = (pygame_joystick is not None or joystick.dragging))

        badge_col = ACCENT if pygame_joystick else TEXT_DIM   # source badge + numeric readout
        text(screen, source, fonts["small"], badge_col, (joystick_panel.right - 20, joystick_panel.y + 18), "topright")
        text(screen, f"throttle  {throttle:+.2f}", fonts["small"], TEXT_DIM, (joystick_panel.centerx, joystick_panel.bottom - 56), "midtop")
        text(screen, f"steering  {steering:+.2f}", fonts["small"], TEXT_DIM, (joystick_panel.centerx, joystick_panel.bottom - 34), "midtop")

        rock_type_panel = pygame.Rect(576, 428, 300, 148)   # rock classification panel
        panel(screen, rock_type_panel, radius = 14)
        text(screen, "ROCK TYPE", fonts["label"], TEXT_DIM, (rock_type_panel.x + 20, rock_type_panel.y + 18))
        text(screen, classify_rock(tele), fonts["value"], TEXT, (rock_type_panel.centerx, rock_type_panel.centery + 14), "center")

        console.draw(screen, fonts["mono"])

        pygame.display.flip()
        clock.tick(FPS)

    rover.send(0.0, 0.0, info)
    rover.close()
    pygame.quit()


if __name__ == "__main__":
    rover = RoverConnection(ROVER_IP, ROVER_PORT, APP_PORT, timeout = CONNECTION_TIMEOUT)
    rover.start()
    rover.send(0.0, 0.0, 1)

    try:
        app_init(rover)
    except KeyboardInterrupt:
        rover.send(0.0, 0.0, 1)
        rover.close()
        pygame.quit()
