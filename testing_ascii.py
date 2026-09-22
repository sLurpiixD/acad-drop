import serial, time

usbDevWithPath = 'COM3'

ser = serial.Serial(port = usbDevWithPath, baudrate =  9600, timeout = 1 )

ser.flush()

### CODE RECREATION

# Fundamental Send/Receive Commands 

def psu_write(cmd):
    ser.write(cmd.encode())

def psu_read_decode():
    data = ser.read_until(b">").decode()
    return data

# PSU Send Commands

def psu_Connect():
    cmd = '<09100000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def psu_Disconnect():
    cmd = '<09200000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def psu_output_on():
    cmd = '<07000000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def psu_output_off():
    cmd = '<08000000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

# PSU Read Data

def get_voltage():
    cmd = '<02000000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def get_current():
    cmd = '<04000000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def get_firmwareVersion():
    cmd = '<06000000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)
 
# Hardcoded Set Voltage/Current

def set_voltage():
    cmd = '<01010000000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)

def set_current():
    cmd = '<03000100000>'
    print(cmd)
    psu_write(cmd)
    data = psu_read_decode()
    print(data)
    

# Main Sequence

# Connect PC to PSU
psu_Connect()

# Set Voltage / Current (10V / 0.1A)
set_voltage()
set_current()

# Can't request readset voltage/current; OVP/OCP


psu_output_on()

# Read Output Voltage/Current for every 500ms

for i in range (0,5):
    
    get_voltage()
    time.sleep(0.1)
    get_current()
    time.sleep(0.5)

psu_output_off()

# can't request device info: model ID, max V, max A

get_firmwareVersion()

psu_Disconnect()
