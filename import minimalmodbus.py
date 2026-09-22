import serial
import minimalmodbus
import time

com_port = 'COM3'

print("--- TEST 1: The 'Silent' Modbus Registers ---")
baudrates = [9600, 115200]
slave_ids = [1, 2]

for baud in baudrates:
    for slave in slave_ids:
        print(f"Trying Baud: {baud} | Slave: {slave}...", end="\r")
        try:
            psu = minimalmodbus.Instrument(com_port, slave)
            psu.serial.baudrate = baud
            psu.serial.timeout = 0.5
            
            # Read Register 18 (0x0012). This is the standard Kuaiqu Voltage register
            val = psu.read_register(18, 0)
            print(f"\n\n!!! BINGO !!! It IS Modbus!")
            print(f"Baudrate: {baud} | Slave ID: {slave}")
            print(f"Register 18 (Voltage) reads: {val}")
            exit()
        except minimalmodbus.NoResponseError:
            pass # Ignored us, keep trying
        except Exception as e:
            # If it throws any other error, it means it answered!
            print(f"\n\n!!! BINGO !!! The PSU answered!")
            print(f"Baudrate: {baud} | Slave ID: {slave}")
            print(f"It threw an error: {e}, but connection is SUCCESSFUL.")
            exit()

print("\n\nModbus failed. Testing ASCII / SCPI Protocol...")

print("\n--- TEST 2: ASCII Text Commands ---")
try:
    with serial.Serial(com_port, 9600, timeout=1) as ser:
        # Standard SCPI and Custom Kuaiqu text commands
        commands = [b"*IDN?\r\n", b"VSET?\r\n", b"VOUT?\r\n"]
        for cmd in commands:
            print(f"Sending {cmd}...")
            ser.write(cmd)
            time.sleep(0.3)
            response = ser.read_all()
            if response:
                print(f"\n!!! BINGO !!! It uses Text Strings! Response: {response}")
                exit()
        print("No ASCII response either.")
except Exception as e:
    print(f"Serial Error: {e}")

print("\nBoth tests failed. Is the power supply screen turned on? Are you 100% sure the ESP32 is unplugged and COM6 is the Kuaiqu?")