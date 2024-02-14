import spidev
import RPi.GPIO as GPIO
import time

TIAGAIN = 0x000000
TIA_AMB_GAIN = 0x000005
LEDCNTRL = 0x00114
CONTROL0 = 0x000000
LEDIVAL = 0x00
LED2VAL = 0x01
ALEDIVAL = 0x02
ALED2VAL = 0x03
LED1ABSVAL = 0x04
LED2ABSVAL = 0x05

count = 10

def AFE4490Init():
    AFE4490Write(CONTROL0, 0x000000)

def AFE4490Write(address, data):
    write_data = [address, (data >> 16) & 0xFF, (data >> 8) & 0xFF, data & 0xFF]
    GPIO.output(8, GPIO.LOW)
    spi.xfer2(write_data)
    GPIO.output(8, GPIO.HIGH)

def AFE4490Read(address):
    read_data = [address | 0x80, 0, 0, 0]
    GPIO.output(8, GPIO.LOW)
    spi.xfer2(read_data)
    GPIO.output(8, GPIO.HIGH)
    return (read_data[1] << 16) | (read_data[2] << 8) | read_data[3]

spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 1000000
spi.mode = 1
spi.bits_per_word = 8

GPIO.setmode(GPIO.BOARD)
GPIO.setup(8, GPIO.OUT)
GPIO.setup(9, GPIO.IN)

AFE4490Init()

while True:
    for i in range(count):
        AFE4490Write(TIAGAIN, 0x000000)
    AFE4490Write(TIA_AMB_GAIN, 0x000005)
    AFE4490Write(LEDCNTRL, 0x00114)
    time.sleep(1)
    if GPIO.input(9) == 1:
        print("SOMI is connected, Reading register is proceed!")
        IRheartsignal = AFE4490Read(LEDIVAL)
        Redheartsignal = AFE4490Read(LED2VAL)
        IRdc = IRheartsignal - mean(IRheartsignal, count)
        Reddc = Redheartsignal - mean(Redheartsignal, count)
        difIRheartsig_dc = IRheartsignal - IRdc
        difREDheartsig_dc = Redheartsignal - Reddc
        powdifIR = pow(difIRheartsig_dc, 2.0)
        powdifRed = pow(difREDheartsig_dc, 2.0)
        IRac = powdifIR / 1
        Redac = powdifRed / 1
        Ratio = (Redac / Reddc) / (IRac / IRdc)
        Sp0percentage = 100 * Ratio
        print("Red Data: ", AFE4490Read(LED2VAL))
        print("Red Ambient Data: ", AFE4490Read(ALED2VAL))
        print("Red Different: ", AFE4490Read(LED2ABSVAL))
        print("IR Data: ", AFE4490Read(LEDIVAL))
        print("IR Ambient: ", AFE4490Read(ALEDIVAL))
        print("IR Different: ", AFE4490Read(LED1ABSVAL))
        print("Ratio: ", Ratio)
        if Sp0percentage > 100 or Sp0percentage < 0:
            print("Sp02")
        else:
            print("Sp02: ", Sp0percentage, "%")
    else:
        print("SOMI is not connected, check the wire!")

import numpy as np

uch_spo2_table = np.array([95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
                           99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                           100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
                           97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
                           90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
                           80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
                           66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
                           49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
                           28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
                           3, 2, 1])

BUFFER_SIZE = 100
MA4_SIZE = 4

an_x = np.zeros(BUFFER_SIZE, dtype=int)
an_y = np.zeros(BUFFER_SIZE, dtype=int)


def estimate_spo2(pun_ir_buffer, n_ir_buffer_length, pun_red_buffer, pn_spo2, pch_spo2_valid, pn_heart_rate,
                  pch_hr_valid):
    global an_x, an_y

    un_ir_mean = np.mean(pun_ir_buffer)

    an_x = -1 * (pun_ir_buffer - un_ir_mean)

    for k in range(BUFFER_SIZE - MA4_SIZE):
        an_x[k] = np.sum(an_x[k:k + MA4_SIZE]) / MA4_SIZE

    n_th1 = np.mean(an_x)

    if n_th1 < 30:
        n_th1 = 30
    if n_th1 > 60:
        n_th1 = 60


for k in range(BUFFER_SIZE):
    if an_x[k] < -n_th1:
        an_x[k] = -n_th1
    elif an_x[k] > n_th1:
        an_x[k] = n_th1

for k in range(BUFFER_SIZE):
    if k == 0:
        an_y[k] = an_x[k]
    elif k == 1:
        an_y[k] = 0.5 * an_x[k] + 0.5 * an_x[k - 1]
    elif k > 1:
        an_y[k] = 0.33 * an_x[k] + 0.67 * an_x[k - 1] + 0.0 * an_x[k - 2]

n_y_dc_mean = np.mean(an_y)

for k in range(BUFFER_SIZE):
    an_y[k] = an_y[k] - n_y_dc_mean

n_y_ac_mean = np.mean(an_y)

n_nume = np.sum(an_x * an_y)
n_denom = np.sum(an_x * an_x)

if n_denom > 0:
    ratio = n_nume / n_denom
    ratio = np.clip(ratio, 0, 1)
    spo2_value = -45.060 * ratio * ratio + 30.354 * ratio + 94.845

    n_c_ratio = np.clip((un_ir_mean / np.mean(pun_red_buffer)), 0, 2.5)
    n_spo2_diff = np.abs(spo2_value - uch_spo2_table)
    idx_min = np.argmin(n_spo2_diff)

    if n_c_ratio > 0.4 and n_c_ratio < 2.5:
        n_heart_rate = (idx_min * 4) + 60
        if n_heart_rate < 40 or n_heart_rate > 240:
            n_heart_rate = pn_heart_rate
        else:
            pn_heart_rate = n_heart_rate
            pch_hr_valid = True
    else:
        n_heart_rate = pn_heart_rate
        pch_hr_valid = False

    pn_spo2[0] = spo2_value
    pch_spo2_valid[0] = True
else:
    pn_spo2[0] = 0
    pch_spo2_valid[0] = False

return pn_spo2[0], pch_spo2_valid[0], pn_heart_rate, pch_hr_valid


