import pygame
import socket
import json

rover_ip = "192.168.1.1"
rover_port = 4810
app_port = 4811

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def run():
  pygame.init()

  joystick = None
  if pygame.joystick.get_count() > 0:
    joystick = pygame.joystick.Joystick(0)
    joystick.init()
  
  screen = pygame.display.set_mode((800, 600))
  clock = pygame.time.Clock()

  running = True
  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False

    if joystick:
      throttle = -joystick.get_axis(1)
      steering = joystick.get_axis(0)
      msg = f"{throttle:.2f},{steering:.2f}".encode()
      sock.sendto(msg, (rover_ip, rover_port))

      if joystick.get_button(0):
        sock.sendto(b"w", (rover_ip, rover_port))

    screen.fill((0, 0, 0))
    pygame.display.flip()
    clock.tick(60)

  pygame.quit()

if __name__ == "__main__":
  sock.sendto(b".", (rover_ip, rover_port))
  run()