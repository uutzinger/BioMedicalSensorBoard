import spidev
import math
import time
noop1 = 0x00
noop2 = 0x7F
status = 0x01
int1 = 0x02
int2 = 0x03
mngrint = 0x04
mngrdyn = 0x05
swrst = 0x08
synch = 0x09
fiforst = 0x0A
info = 0x0F #will not provide data if following a power cycle or swrst event
cnfggen = 0x10
cnfgcal = 0x12
cnfgemux = 0x14
cnfgecg = 0x15
cnfgrtor1 = 0x1D
cnfgrtor2 = 0x1E
ecgfifobrst = 0x20
ecgfifo = 0x21
rtor = 0x25

#example spi bus for 8 bit
#spibus = spidev.SpiDev()
#spibus.open(0, 1) #bus is zero, chip select is 1
#spibus.mode = 0b01
#spibus.max_speed_hz = 800000
#spibus.xfer([0x40, reg add, bits in hex]) the 0x40 is for a write call a 0x41 is a read call
#if reading the last is a don't care so we can set to 0x0

#create functions to be called later

"""
#possible method to transfer top, middle, and bottom bits; subject to test

def writeadd(address, data):
    spibus.xfer([address]) #sends address
    spibus.xfer([(data >> 16) & 0xFF])
    spibus.xfer([(data >> 8) & 0xFF])
    spibus.xfer([data & 0xFF])

"""

def regwrite(address, data): #this function is used to write to registers of the max30003
    #the address is the hex value, the data is the bits we want to send
    spibus.xfer([0x40, address, data])

def swReset():
    regwrite(swrst, 0x000000)
    time.sleep(100)

def maxsynch(): #can be used to synch max30003 to clock
    regwrite(synch, 0x000000)

def readreg(address): #used to read from a desired address
    #reminder that the data value sent is a don't care
    spibus.xfer([0x41, address, 0x0])

def readinfo(): #used to check info of the max30003
    #read from the info register
    dat = spibus.xfer([0x41, 0x0F, 0x0])
    if(dat & 0xf0) == 0x50:
        print("max30003 is ready")
        print("Rev ID: ")
        print((dat & 0xf0))
        return True
    else:
        print("max3003 read info err")
        return False

def readECG(num_samples):
    i = 0
    while i < (num_samples * 3):
        dat = spibus.xfer([0x41, 0x20, 0x0])

def max3003begin():
    swReset()
    time.sleep(0.1)
    regwrite(cnfggen, 0x081007)
    time.sleep(0.1)
    regwrite(cnfgcal, 0x720000)
    time.sleep(0.1)
    regwrite(cnfgemux, 0x0B0000)
    time.sleep(0.1)
    regwrite(cnfgecg, 0x805000)
    time.sleep(0.1)
    regwrite(cnfgrtor1, 0x3fc600)
    maxsynch()
    time.sleep(0.1)

def setSamprate(samprate):
    readreg(cnfgecg)
    if samprate == 128:
        regwrite(cnfgecg, 0x800000)
    if samprate == 256:
        regwrite(cnfgecg, 0x400000)
    if samprate == 512:
        regwrite(cnfgecg, 0x000000)
    else:
        print("Wrong sampling rate, please choose between 128, 256, or 512.")

def getECGSamp():
    dat0 = readreg(ecgfifo)

