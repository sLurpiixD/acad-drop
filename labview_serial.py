"""
labview_serial.py
-----------------
Reads sensor data from ESP32-S3 over USB Serial and writes it
to a shared CSV file that LabVIEW can read in real time.

Also accepts commands from LabVIEW via a command file:
  - labview_command.txt → ESP32 reads this and forwards to serial

LabVIEW Integration:
  - Read data : Use LabVIEW "Read From Spreadsheet File" on 'live_data.csv'
  - Send cmds : Write "FAN:75\n" or "TEC:1\n" to 'labview_command.txt'
"""

import serial
import serial.tools.list_ports
import csv
import time
import os
import sys

# =============================================================
#  CONFIGURATION — Edit these as needed
# =============================================================
SERIAL_PORT   = "COM3"       # Change to your ESP32 port (e.g. /dev/ttyUSB0 on Linux)
BAUD_RATE     = 115200
CSV_OUTPUT    = "live_data.csv"       # LabVIEW reads this file
CMD_FILE      = "labview_command.txt" # LabVIEW writes commands here
LOG_FILE      = "data_log.csv"        # Full session log

# =============================================================
#  AUTO-DETECT PORT (optional helper)
# =============================================================
def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "USB" in p.description or "CP210" in p.description or "CH340" in p.description:
            return p.device
    return None

# =============================================================
#  MAIN
# =============================================================
def main():
    global SERIAL_PORT

    # Try auto-detect if default port not found
    if not os.path.exists(SERIAL_PORT) and sys.platform != "win32":
        detected = find_esp32_port()
        if detected:
            SERIAL_PORT = detected
            print(f"Auto-detected ESP32 on: {SERIAL_PORT}")

    print(f"Connecting to ESP32 on {SERIAL_PORT} at {BAUD_RATE} baud...")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    except serial.SerialException as e:
        print(f"ERROR: Could not open port {SERIAL_PORT}: {e}")
        sys.exit(1)

    time.sleep(2)  # Wait for ESP32 to boot
    print("Connected! Starting data acquisition...\n")

    # Read and print the CSV header from ESP32
    header_line = ser.readline().decode("utf-8", errors="ignore").strip()
    print(f"Header: {header_line}")
    headers = header_line.split(",")

    # Initialize log file with header
    with open(LOG_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(headers)

    # Clear command file
    open(CMD_FILE, "w").close()

    print(f"Logging to: {LOG_FILE}")
    print(f"Live data:  {CSV_OUTPUT}")
    print(f"Commands:   {CMD_FILE}")
    print("-" * 60)

    try:
        while True:
            # --- 1. Check for LabVIEW commands ---
            if os.path.exists(CMD_FILE):
                with open(CMD_FILE, "r") as f:
                    cmd = f.read().strip()
                if cmd:
                    ser.write((cmd + "\n").encode("utf-8"))
                    print(f">> Sent command: {cmd}")
                    # Clear after sending
                    open(CMD_FILE, "w").close()

            # --- 2. Read data line from ESP32 ---
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
            if not raw or raw.startswith("ACK") or raw.startswith("ERR"):
                if raw:
                    print(f"<< ESP32: {raw}")
                continue

            values = raw.split(",")
            if len(values) != len(headers):
                continue  # Skip malformed lines

            # --- 3. Print to console ---
            row_dict = dict(zip(headers, values))
            print(
                f"[{values[0]}ms] "
                f"V={values[1]}V  I={values[2]}A  P={values[3]}W  "
                f"Hot={values[4]}°C  Cold={values[5]}°C  "
                f"Amb={values[6]}°C  RPM={values[7]}"
            )

            # --- 4. Write to live CSV (single row — LabVIEW polls this) ---
            with open(CSV_OUTPUT, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(headers)
                writer.writerow(values)

            # --- 5. Append to full log ---
            with open(LOG_FILE, "a", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(values)

    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        ser.close()
        print("Serial port closed.")

if __name__ == "__main__":
    main()
