import pygame
import socket
import json

rover_ip = "192.168.1.1"
rover_port = 4810
app_port = 4811

def run():
  pygame.init()
  screen = pygame.display.set_mode((800, 600))
  clock = pygame.time.Clock()

  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

  running = True
  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False
      if event.type == pygame.KEYDOWN:
        if event.key == pygame.K_w:
          sock.sendto(b"w", (rover_ip, rover_port))

    screen.fill((0, 0, 0))
    pygame.display.flip()
    clock.tick(60)

  pygame.quit()

if __name__ == "__main__":
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  sock.sendto(b"[controller] Controller connected.", (rover_ip, rover_port))
  run()