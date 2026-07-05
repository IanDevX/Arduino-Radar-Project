import serial
import time
import pygame
import math
import sys

# ---------------- SERIAL ----------------
ser = serial.Serial('COM3', 115200, timeout=0)
time.sleep(2)

# ---------------- PYGAME ----------------
pygame.init()

WIDTH, HEIGHT = 800, 600
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Ultrasonic Radar")

clock = pygame.time.Clock()
font = pygame.font.SysFont(None, 36)
small_font = pygame.font.SysFont(None, 20)  # for range/angle tick labels

center_x, center_y = WIDTH // 2, HEIGHT - 50
radar_radius = 300
MAX_RANGE_CM = 40  # keep in sync with the Arduino's MAX_RANGE_CM

# ---------------- STATE ----------------
latest_angle = 0.0
latest_distance = -1.0
serial_buffer = ""
motor_running = False  # local mirror of what we've told the Arduino

# accumulated (angle_rad, distance_cm) for the current pass
points = []
prev_angle = None    # previous angle, used to detect direction
prev_direction = 0   # +1 = increasing angle, -1 = decreasing angle

# ---------------- BACKGROUND GRID (built once, blitted every frame) ----------------
GRID_SPACING = 40
GRID_COLOR = (0, 90, 0, 50)

grid_surface = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
for gx in range(0, WIDTH, GRID_SPACING):
    pygame.draw.line(grid_surface, GRID_COLOR, (gx, 0), (gx, HEIGHT), 3)
for gy in range(0, HEIGHT, GRID_SPACING):
    pygame.draw.line(grid_surface, GRID_COLOR, (0, gy), (WIDTH, gy), 3)

# ---------------- RANGE RINGS (radius fraction, cm value label) ----------------
RANGE_RINGS = [
    (1 / 3, round(MAX_RANGE_CM * (1 / 3))),
    (2 / 3, round(MAX_RANGE_CM * (2 / 3))),
    (1.0,   MAX_RANGE_CM),
]

ANGLE_TICKS = [0, 30, 60, 90, 120, 150, 180]

# ---------------- DRAW FUNCTION ----------------


def draw_point(angle_rad, distance_cm):

    distance_cm = min(distance_cm, MAX_RANGE_CM)
    r = (distance_cm / MAX_RANGE_CM) * radar_radius

    if distance_cm <= 0:
        r = radar_radius

    x = center_x + math.cos(angle_rad) * r
    y = center_y - math.sin(angle_rad) * r

    x_max = center_x + math.cos(angle_rad) * radar_radius
    y_max = center_y - math.sin(angle_rad) * radar_radius

    pygame.draw.line(screen, (20, 150, 2), (center_x,
                     center_y), (int(x), int(y)), 5)

    if distance_cm > 0:
        pygame.draw.line(screen, (189, 55, 45), (int(x), int(y)),
                         (int(x_max), int(y_max)), 5)


def draw_radar_backdrop():
    screen.blit(grid_surface, (0, 0))

    spoke_surface = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    for deg in ANGLE_TICKS:
        rad = math.radians(deg)
        x_end = center_x + math.cos(rad) * radar_radius
        y_end = center_y - math.sin(rad) * radar_radius
        pygame.draw.line(spoke_surface, (0, 120, 0, 60),
                         (center_x, center_y), (x_end, y_end), )
    screen.blit(spoke_surface, (0, 0))

    for frac, cm_value in RANGE_RINGS:
        ring_radius = radar_radius * frac
        pygame.draw.circle(screen, (255, 255, 255),
                           (center_x, center_y), int(ring_radius), 1)

        label_surface = small_font.render(f"{cm_value}cm", True, (0, 200, 0))
        label_x = center_x + ring_radius - label_surface.get_width() - 4
        label_y = center_y - 18
        screen.blit(label_surface, (label_x, label_y))

    pygame.draw.line(screen, (255, 255, 255),
                     (center_x - radar_radius, center_y),
                     (center_x + radar_radius, center_y), 1)

    for deg in ANGLE_TICKS:
        rad = math.radians(deg)
        label_radius = radar_radius + 18
        lx = center_x + math.cos(rad) * label_radius
        ly = center_y - math.sin(rad) * label_radius

        label_surface = small_font.render(f"{deg}°", True, (0, 200, 0))
        rect = label_surface.get_rect(center=(lx, ly))
        screen.blit(label_surface, rect)


def update_state(angle_rad, distance_cm):
    global latest_angle, latest_distance, prev_angle, prev_direction

    latest_angle = angle_rad
    latest_distance = distance_cm

    if prev_angle is not None:
        delta = angle_rad - prev_angle
        if abs(delta) > 1e-6:
            direction = 1 if delta > 0 else -1
            if prev_direction != 0 and direction != prev_direction:
                points.clear()
            prev_direction = direction

    prev_angle = angle_rad

    points.append((angle_rad, distance_cm))


# ---------------- MAIN LOOP ----------------
running = True
want_to_quit = False

while running:
    screen.fill((0, 0, 0))

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            want_to_quit = True
        elif event.type == pygame.KEYDOWN:
            if event.key == pygame.K_SPACE:
                try:
                    ser.write(b'T')
                    motor_running = not motor_running
                except Exception:
                    pass

    if want_to_quit and latest_angle == 0.0:  # only quit when the sweep is at the top
        motor_running = False
        ser.write(b'T')
        running = False
        continue

    if ser.in_waiting:
        try:
            serial_buffer += ser.read(ser.in_waiting).decode(errors="ignore")

            if "\n" in serial_buffer:
                lines = serial_buffer.split("\n")
                serial_buffer = lines[-1]
                complete_lines = [l.strip() for l in lines[:-1] if l.strip()]

                for line in complete_lines:
                    if "," in line:
                        angle_deg, distance = line.split(",")
                        update_state(math.radians(
                            float(angle_deg)), float(distance))

        except Exception:
            pass

    end_x = center_x + math.cos(latest_angle) * radar_radius
    end_y = center_y - math.sin(latest_angle) * radar_radius

    pygame.draw.line(screen, (11, 92, 0),
                     (center_x, center_y),
                     (end_x, end_y), 4)

    pygame.draw.circle(screen, (0, 255, 0), (int(end_x), int(end_y)), 5)
    pygame.draw.circle(screen, (0, 255, 0), (int(end_x), int(end_y)), 9, 1)

    for angle_rad, distance_cm in points:
        draw_point(angle_rad, distance_cm)

    draw_radar_backdrop()

    status_text = "RUNNING (SPACE to pause)" if motor_running else "PAUSED - press SPACE to start"
    status_color = (0, 255, 0) if motor_running else (200, 200, 0)
    text_surface = font.render(status_text, True, status_color)
    screen.blit(text_surface, (10, 10))

    pygame.display.flip()
    clock.tick(60)

pygame.quit()
sys.exit()
