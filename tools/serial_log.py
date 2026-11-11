#!/usr/bin/env python3
"""serial_log.py - log a serial port with timestamps, and survive a USB pull.

Usage (PowerShell, in the repo):
    py tools\\serial_log.py --port COM5 --out docs\\secure_boot\\hwlog_example.txt

Stop with Ctrl+C. Each line is written as: seconds since the logger started,
then the text. The time is when the end of the line arrived, not when its first
byte was sent. If the port disappears (USB pulled) the logger writes a
[LOGGER] port lost line and keeps retrying; when the port is back it writes
[LOGGER] port open and carries on. Text half-received at the moment of a pull
is written out as a [LOGGER] partial line so nothing is silently dropped.
Needs pyserial.
"""
import argparse
import datetime
import time


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True, help="serial port, for example COM5")
    ap.add_argument("--baud", type=int, default=115200, help="serial speed in baud")
    ap.add_argument("--out", required=True, help="log file to append to")
    a = ap.parse_args()
    import serial  # imported here so that --help works without pyserial installed

    t0 = time.monotonic()
    with open(a.out, "a", encoding="utf-8", newline="\n") as f:

        def emit(text):
            line = f"{time.monotonic() - t0:9.3f}  {text}"
            print(line, flush=True)
            f.write(line + "\n")
            f.flush()

        emit(f"[LOGGER] start {datetime.datetime.now().isoformat(timespec='seconds')} "
             f"port={a.port} baud={a.baud}")
        ser = None
        buf = b""
        try:
            while True:
                if ser is None:
                    try:
                        ser = serial.Serial(a.port, a.baud, timeout=0.2)
                        emit("[LOGGER] port open")
                    except (serial.SerialException, OSError):
                        time.sleep(0.2)
                        continue
                try:
                    data = ser.read(256)
                except (serial.SerialException, OSError):
                    emit("[LOGGER] port lost")
                    if buf:
                        emit("[LOGGER] partial line: " + buf.decode("utf-8", errors="replace"))
                        buf = b""
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    continue
                if not data:
                    continue
                buf += data
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    emit(raw.decode("utf-8", errors="replace").rstrip("\r"))
        except KeyboardInterrupt:
            emit("[LOGGER] stopped by user")


if __name__ == "__main__":
    main()
