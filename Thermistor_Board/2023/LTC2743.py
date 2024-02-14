#LTC Temp drivers
import smbus
import math
#put all relevant registers here

#beginning the bus set-up
bus = smbus.SMBus(1)
Address = 0x14 #or 0x54

def readLTC(address):
    data = bus.read_word_data(address, 0x00)
    print("data")
