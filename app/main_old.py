import os
os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "1"

import threading
import pygame
import socket
import json
import time

rover_ip = "192.168.1.1"
rover_port = 4810
app_port = 4811

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

telemetry = {"mag":"", "ir":""}

def app_init():
  pygame.init()

  joystick = None
  if pygame.joystick.get_count() > 0:
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
  
  screen = pygame.display.set_mode((800, 600))
  clock = pygame.time.Clock()

  throttle = 0.0
  steering = 0.0
  info = 0
  msg = f"{throttle:.2f},{steering:.2f},{info}".encode()

  running = True
  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False
        info = 2

    if joystick:
      throttle = -joystick.get_axis(1)
      steering = joystick.get_axis(0)

    msg = f"{throttle:.2f},{steering:.2f},{info}".encode()
    try:
      sock.sendto(msg, (rover_ip, rover_port))
    except OSError:
      pass

    screen.fill((0, 0, 0))
    
    font = pygame.font.SysFont("Arial", 24)
    mag = telemetry["mag"]
    text = font.render(f"{mag}", True, (255, 255, 255))
    screen.blit(text, (50, 50))

    font = pygame.font.SysFont("Arial", 24)
    ir = telemetry["ir"]
    text = font.render(f"{ir}", True, (255, 255, 255))
    screen.blit(text, (50, 150))

    pygame.display.flip()
    clock.tick(60)

  pygame.quit()

def udp_rx_loop():
  while True:
    try:
      data, _ = sock.recvfrom(512)
      data_parts = data.decode().split(",")

      telemetry["mag"] = data_parts[0]
      telemetry["ir"] = data_parts[1]
    except socket.timeout:
      pass


if __name__ == "__main__":
  sock.bind(("0.0.0.0", app_port))
  sock.settimeout(1.0)

  udp_rx_thread = threading.Thread(target = udp_rx_loop, daemon = True)
  udp_rx_thread.start()

  time.sleep(0.1)

  msg = f"{0.0:.2f},{0.0:.2f},{1}".encode()
  sock.sendto(msg, (rover_ip, rover_port))

  app_init()