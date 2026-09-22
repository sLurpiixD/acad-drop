import minimalmodbus, time

psu = minimalmodbus.Instrument('COM7', 1)
psu.serial.baudrate = 9600
psu.serial.timeout  = 2
psu.mode            = minimalmodbus.MODE_RTU
time.sleep(0.1)

# PSU Programming Interface Commands Functions

## PSU Send Switch Commands Functions
def psu_Connect():
    # Handshake — must use FC 06, not FC 16
    psu.write_register(0, 1, functioncode=6)
    return 1

def psu_Disconnect():
    psu.write_register(0, 0, functioncode=6)
    return 0

def psu_OutputON():
    psu.write_register(27, 1, functioncode=6)
    return 1

def psu_OutputOFF():
    psu.write_register(27, 0, functioncode=6)
    return 0

def psu_BuzzerON():
    psu.write_register(75, 1, functioncode=6)
    return 1


def psu_BuzzerOFF():
    psu.write_register(75, 0, functioncode=6)
    return 0

# PSU Send Float Commands Functions

def psu_SetVoltage(v):
    psu.write_float(1, v)
    return v

def psu_SetCurrent(a):
    psu.write_float(3, a)
    return a

## PSU Read Commands Functions

def psu_ReadSetVoltage():
    return round(psu.read_float(1), 3)

def psu_ReadSetCurrent():
    return round(psu.read_float(3), 3)

def psu_ReadOutVoltage():
    return round(psu.read_float(29), 3)

def psu_ReadOutCurrent():
    return round(psu.read_float(31), 3)

def psu_ReadMode():
    return 'CV' if psu.read_register(33) == 0 else 'CC'

def psu_DeviceInfo():
    return psu.read_registers(68, 4)

def psu_BuzzerState():
    return psu.read_register(75)

# Advanced Features Command Functions:

def psu_ReadOVP():
    return round(psu.read_register(34), 3)

def psu_ReadOCP():
    return round(psu.read_register(35), 3)


# Main Sequence
# Handshake
psu_Connect()
print("PSU Connected")

# Edit dial set-points — works WITHOUT output ON
v_edit = psu_SetVoltage(10)
a_edit = psu_SetCurrent(0.1)

# Read dial set-points — works WITHOUT output ON
v_set = psu_ReadSetVoltage()
a_set = psu_ReadSetCurrent()
ovp_set = psu_ReadOVP()
ocp_set = psu_ReadOCP()
status = psu_ReadMode()
print(f"Dial set to:    {v_set}V  /  {a_set}A  /  mode:{status}  /  OVP: {ovp_set}V  /  OCP: {ocp_set}A")

# Read actual output
v_out = psu_ReadOutVoltage()
a_out = psu_ReadOutCurrent()
print(f"Actual output:  {v_out}V  /  {a_out}A")

psu_OutputON()
print("PSU OUTPUT ON")

# Read dial set-points — works WITHOUT output ON
v_set = psu_ReadSetVoltage()
a_set = psu_ReadSetCurrent()
ovp_set = psu_ReadOVP()
ocp_set = psu_ReadOCP()
status = psu_ReadMode()
print(f"Dial set to:    {v_set}V  /  {a_set}A  /  mode:{status}  /  OVP: {ovp_set}V  /  OCP: {ocp_set}A")

for i in range (0,7):
    # Read actual output
    v_out = psu_ReadOutVoltage()
    a_out = psu_ReadOutCurrent()
    status = psu_ReadMode()
    print(f"Actual output:  {v_out}V  /  {a_out}A /  mode:{status}")
    time.sleep(0.5)

psu_OutputOFF()
print("PSU OUTPUT OFF")

buzzer = psu_BuzzerON()
print(f'Buzzer: {psu_BuzzerState()}')

# Read device info
info = psu_DeviceInfo()
print(f"Model ID: {info[0]}  Max: {info[1]}V / {info[2]}A  Version: {info[3]/100:.2f}")

# Disconnect
psu_Disconnect()
print("PSU Disonnected")





'''
# th = [0, 1, 3, 27, 29, 31, 33, 34, 35, 36, 38, 40, 42, 44, 62, 64, 66, 68, 69, 70, 71, 75]

th = [276]



for i in th:
    try:
        print(f'{i}th register: {psu.read_register(i)}')
    except minimalmodbus.NoResponseError:
        print(f'{i}th register: PSU Not responding')
    except minimalmodbus.InvalidResponseError:
        print(f'{i}th register: PSU Invalid Respond')

    time.sleep(0.2)

    try:
        print(f'{i}th float: {psu.read_float(i)}')
    except minimalmodbus.NoResponseError:
        print(f'{i}th float: PSU Not responding')
    except minimalmodbus.InvalidResponseError:
        print(f'{i}th float: PSU Invalid Respond')

    time.sleep(0.2)
'''




