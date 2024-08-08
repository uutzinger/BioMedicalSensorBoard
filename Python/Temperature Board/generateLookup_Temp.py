import numpy as np

def calctemp(Vdiff, A, B, C, Vin, R1, R2, R3):
    # Calculating the actual resistance of the thermistor based on the measurements
    numerator = R3 * (Vin * R2 - Vdiff * (R1 + R2))
    denominator = Vin * R1 + Vdiff * (R1 + R2)
    R4 = numerator / denominator

    # Steinhart-Hart Equation
    lnR = np.log(R4)
    Tinv = (A + (B * lnR) + (C * (lnR ** 3)))
    T = 1.0 / Tinv
    T -= 273.15  # Convert Kelvin to Centigrade

    return T * 100.0  # Convert temperature to centidegrees Celsius


# Define the parameters for the Steinhart-Hart equation and Wheatstone bridge
A = 0.001111285538
B = 0.0002371215953
C = 0.00000007520676806
Vin = 3530
R1 = 9960
R2 = 9970
R3 = 9960

# Generate the lookup table
Vdiff_range = range(-1000, 1001)  # Example range of Vdiff values in mV
lookup_table = {Vdiff: int(calctemp(Vdiff, A, B, C, Vin, R1, R2, R3)) for Vdiff in Vdiff_range}

# Save the lookup table to a file
import json
with open('lookup_table.json', 'w') as f:
    json.dump(lookup_table, f)

# Example of how to use the lookup table
Vdiff = 500  # Example Vdiff value
temperature = lookup_table.get(Vdiff, None)
if temperature is not None:
    print(f"Temperature for Vdiff {Vdiff} mV: {temperature / 100.0} °C")
else:
    print("Vdiff value out of range")
