"""Grab the exact frames the ESP32 sends to the LCD, one per mood and style, and save them as PNGs.

The firmware dumps its 320x200 canvas (RGB565, big-endian) on the 'c' serial command. This script pauses the
autonomy (eyes centered, no blinks), sets each mood, waits for the shape to settle, captures it, and also writes a
contact sheet of all frames. Needs the AutoEyes firmware; takes a few minutes at 115200 baud.

    pip install pyserial numpy opencv-python
    python tools/capture.py --port COM10 --out docs/img
"""
import argparse
import os
import time

import cv2
import numpy as np
import serial

MOODS = ["neutral", "happy", "sad", "angry", "sleepy", "surprised"]
STYLES = ["flat", "glossy"]


def read_until(ser, token, timeout=5.0):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        buf += ser.read(ser.in_waiting or 1)
        if token in buf:
            return buf
    raise TimeoutError(f"no {token!r} from board; got {buf[-200:]!r}")


def capture(ser, cmd=b"c"):
    """Send a frame command ('c', or 'n' in film mode) and return the frame the board sends back (BGR)."""
    ser.reset_input_buffer()
    ser.write(cmd)
    head = read_until(ser, b"CAP ", timeout=3.0)
    head = head[head.index(b"CAP "):]
    while b"\n" not in head:
        head += ser.read(ser.in_waiting or 1)
    line, rest = head.split(b"\n", 1)
    w, h = (int(v) for v in line.split()[1:3])
    need = w * h * 2
    data = rest
    end = time.time() + 30  # ~11 s per frame at 115200 baud
    while len(data) < need and time.time() < end:
        data += ser.read(need - len(data))
    if len(data) < need:
        raise TimeoutError(f"short frame: {len(data)}/{need} bytes")
    px = np.frombuffer(data[:need], dtype=">u2").reshape(h, w).astype(np.uint32)
    r = ((px >> 11) & 0x1F) * 255 // 31
    g = ((px >> 5) & 0x3F) * 255 // 63
    b = (px & 0x1F) * 255 // 31
    return np.dstack([b, g, r]).astype(np.uint8)  # BGR for OpenCV


def label(img, text):
    out = img.copy()
    cv2.putText(out, text, (8, 192), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1, cv2.LINE_AA)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="captures")
    ap.add_argument("--settle", type=float, default=1.0, help="seconds to let the mood shape ease in")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False  # reset; the boot line reports the saved style
        boot = read_until(ser, b")\n", timeout=6.0)
        start_style = "glossy" if b"style glossy" in boot else "flat"
        current = start_style
        ser.write(b"p")
        read_until(ser, b"paused")

        rows = []
        for style in STYLES:
            if style != current:
                ser.write(b"s")
                read_until(ser, f"style {style}".encode())
                current = style
            row = []
            for mi, mood in enumerate(MOODS):
                ser.write(str(mi).encode())
                read_until(ser, f"mood {mood}".encode())
                time.sleep(args.settle)
                img = capture(ser)
                path = os.path.join(args.out, f"eyes_{style}_{mood}.png")
                cv2.imwrite(path, img)
                print("saved", path)
                row.append(label(img, f"{style} / {mood}"))
            rows.append(np.hstack([np.pad(i, ((4, 4), (4, 4), (0, 0)), constant_values=60) for i in row]))
        sheet = np.vstack(rows)
        path = os.path.join(args.out, "eyes_contact_sheet.png")
        cv2.imwrite(path, sheet)
        print("saved", path)
        if current != start_style:
            ser.write(b"s")  # restore the saved style
        ser.write(b"r")


if __name__ == "__main__":
    main()
