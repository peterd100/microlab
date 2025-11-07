# Simple Snake Game in Python 3 for Beginners
# By @TokyoEdTech
# modified by Yan Luo

import turtle
import time
import random
import serial

serialDevFile = '/dev/cu.usbmodem111201'
ser = serial.Serial(serialDevFile, 9600, timeout=0.05)

delay = 0.1
score = 0
high_score = 0
ppa = 10
is_golden = False

# Screen
wn = turtle.Screen()
wn.title("Snake Game by @TokyoEdTech (mod by YL)")
wn.bgcolor("green")
wn.setup(width=600, height=600)
wn.tracer(0)

# Head
head = turtle.Turtle()
head.speed(0)
head.shape("square")
head.color("black")
head.penup()
head.goto(0,0)
head.direction = "stop"

# Food
food = turtle.Turtle()
food.speed(0)
food.shape("circle")
food.color("red")
food.penup()
food.goto(0,100)

segments = []

# Pen
pen = turtle.Turtle()
pen.speed(0)
pen.shape("square")
pen.color("white")
pen.penup()
pen.hideturtle()
pen.goto(0, 260)
pen.write("Score: 0  High Score: 0  P/A: 10", align="center", font=("Courier", 24, "normal"))

def go_up():
    if head.direction != "down": head.direction = "up"
def go_down():
    if head.direction != "up": head.direction = "down"
def go_left():
    if head.direction != "right": head.direction = "left"
def go_right():
    if head.direction != "left": head.direction = "right"

def move():
    if head.direction == "up": head.sety(head.ycor() + 20)
    if head.direction == "down": head.sety(head.ycor() - 20)
    if head.direction == "left": head.setx(head.xcor() - 20)
    if head.direction == "right": head.setx(head.xcor() + 20)

wn.listen()
wn.onkey(go_up, "w")
wn.onkey(go_down, "s")
wn.onkey(go_left, "a")
wn.onkey(go_right, "d")

def safe_readline():
    try:
        line = ser.readline()
        if not line: return None
        return line.decode('utf-8', errors='ignore').strip()
    except:
        return None

try:
    while True:
        wn.update()

        # === SERIAL INPUT ===
        cmd = safe_readline()
        if cmd:
            if cmd in 'wasd':
                {'w': go_up, 'a': go_left, 's': go_down, 'd': go_right}[cmd]()
            elif cmd == 'S':
                ppa = 20
                is_golden = True
                food.color("gold")
                pen.clear()
                pen.write(f"Score: {score}  High Score: {high_score}  P/A: {ppa}", align="center", font=("Courier", 24, "normal"))

        # === BORDER ===
        if abs(head.xcor()) > 290 or abs(head.ycor()) > 290:
            time.sleep(1)
            head.goto(0,0)
            head.direction = "stop"
            for s in segments: s.goto(1000, 1000)
            segments.clear()
            score = 0
            delay = 0.1
            ppa = 10
            is_golden = False
            food.color("red")
            pen.clear()
            pen.write(f"Score: {score}  High Score: {high_score}  P/A: {ppa}", align="center", font=("Courier", 24, "normal"))

        # === FOOD COLLISION ===
        if head.distance(food) < 20:
            ser.write(b'E')  # Fixed: send 'E' to trigger buzzer

            # 1. Score
            score += ppa
            if score > high_score: high_score = score

            # 2. Reset golden BEFORE moving
            if is_golden:
                ppa = 10
                is_golden = False

            # 3. Move to new position
            x = random.randint(-280, 280)
            y = random.randint(-280, 280)
            food.goto(x, y)

            # 4. FORCE RED AFTER MOVE
            food.color("red")

            # 5. Grow
            seg = turtle.Turtle()
            seg.speed(0); seg.shape("square"); seg.color("grey"); seg.penup()
            segments.append(seg)

            delay = max(0.05, delay - 0.001)

            # 6. Update display
            pen.clear()
            pen.write(f"Score: {score}  High Score: {high_score}  P/A: {ppa}",
                      align="center", font=("Courier", 24, "normal"))

        # === BODY FOLLOW ===
        for i in range(len(segments)-1, 0, -1):
            segments[i].goto(segments[i-1].pos())
        if segments:
            segments[0].goto(head.pos())

        move()

        # === SELF COLLISION ===
        for s in segments:
            if s.distance(head) < 20:
                time.sleep(1)
                head.goto(0,0)
                head.direction = "stop"
                for seg in segments: seg.goto(1000, 1000)
                segments.clear()
                score = 0
                delay = 0.1
                ppa = 10
                is_golden = False
                food.color("red")
                pen.clear()
                pen.write(f"Score: {score}  High Score: {high_score}  P/A: {ppa}", align="center", font=("Courier", 24, "normal"))

        time.sleep(delay)

except KeyboardInterrupt:
    ser.close()
    wn.bye()
