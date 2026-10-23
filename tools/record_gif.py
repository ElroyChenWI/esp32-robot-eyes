"""Record a GIF of the eyes, frame by frame, straight from the ESP32's frame buffer.

The AutoEyes firmware has a film mode: the clock stops, and each 'n' advances it by 50 ms, draws, and sends that
frame. So the GIF plays at a smooth 20 fps no matter how slow the serial link is. A timeline of serial commands
(moods, glances, blinks) is played along the way. Takes a few minutes for a 10 s clip.

    pip install pyserial numpy opencv-python pillow
    python tools/record_gif.py --port COM10 --out docs/img/eyes_demo.gif
"""
import argparse
import os
import sys
import time

import numpy as np
import serial
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capture import capture, read_until  # noqa: E402

STEP_S = 0.05  # must match FILM_STEP_MS in AutoEyes

# (time in seconds, serial commands sent before drawing that frame)
DEMO = [
    (0.0, b"0x"),
    (0.6, b"h"),
    (1.4, b"l"),
    (2.2, b"x"),
    (2.5, b"b"),
    (3.0, b"1"),   # happy
    (4.4, b"0"),
    (4.7, b"k"),
    (5.2, b"x3"),  # angry
    (6.4, b"2"),   # sad
    (7.6, b"5"),   # surprised
    (8.6, b"4"),   # sleepy
    (9.8, b"0b"),
    (10.6, b""),   # end
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("--out", default="eyes_demo.gif")
    ap.add_argument("--style", choices=["flat", "glossy"], default="flat")
    ap.add_argument("--fast-baud", type=int, default=921600, help="0 to stay at 115200")
    args = ap.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=0.2)
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    boot = read_until(ser, b")\n", timeout=6.0)
    start_style = "glossy" if b"style glossy" in boot else "flat"

    if args.fast_baud:
        ser.write(b"+")
        time.sleep(0.2)
        ser.baudrate = args.fast_baud
        ser.reset_input_buffer()
    ser.write(b"p")
    read_until(ser, b"paused")
    if args.style != start_style:
        ser.write(b"s")
        read_until(ser, f"style {args.style}".encode())
    ser.write(b"f")
    read_until(ser, b"film on")

    end_t = DEMO[-1][0]
    n_frames = int(round(end_t / STEP_S))
    frames, cue = [], 0
    t0 = time.time()
    for i in range(n_frames):
        t = i * STEP_S
        while cue < len(DEMO) and DEMO[cue][0] <= t + 1e-6:
            ser.write(DEMO[cue][1])
            time.sleep(0.02)
            cue += 1
        bgr = capture(ser, b"n")
        frames.append(Image.fromarray(np.ascontiguousarray(bgr[:, :, ::-1])))
        if i % 20 == 0:
            print(f"frame {i}/{n_frames}  ({time.time() - t0:.0f}s)")

    ser.write(b"f")  # film off
    if args.style != start_style:
        ser.write(b"s")  # restore the saved style
    ser.write(b"r")
    ser.close()

    pal = [f.convert("P", palette=Image.ADAPTIVE, colors=64) for f in frames]
    pal[0].save(args.out, save_all=True, append_images=pal[1:], duration=int(STEP_S * 1000), loop=0,
                optimize=True, disposal=1)
    print(f"saved {args.out}: {len(frames)} frames, {os.path.getsize(args.out) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
