import numpy as np
import scipy.signal as signal
import matplotlib.pyplot as plt

# Digital Stethoscope Frequency Range: 50-1500 Hz
# High Pass Filter:  35 Hz
# Low Pass Filter: 2000 Hz

# https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7305673/
# https://www.degruyter.com/document/doi/10.1515/cdbme-2019-0066/pdf

# Define the parameters
# 
fc_l = 2000  # Cutoff frequency for LPF
fc_h =   35  # Cutoff frequency for HPF
fs =  44100  # Sampling Frequency

# Design 2nd order LPF
b_l, a_l = signal.butter(2, fc_l / (fs / 2), btype='low')

# Design 2nd order HPF
b_h, a_h = signal.butter(2, fc_h / (fs / 2), btype='high')

# Multiply the transfer functions to create a bandpass filter
b_bpf = np.polymul(b_l, b_h)
a_bpf = np.polymul(a_l, a_h)

# Frequency response of the filters
w, h_l = signal.freqz(b_l, a_l, worN=fs)
w, h_h = signal.freqz(b_h, a_h, worN=fs)
w, h_bpf = signal.freqz(b_bpf, a_bpf, worN=fs)

# Convert from radians/sample to Hz
frequencies = w * fs / (2 * np.pi)

# Define -3 dB point
minus_3db = -3

# Plot the magnitude and phase responses
plt.figure(figsize=(12, 12))

# LPF magnitude response
plt.subplot(3, 2, 1)
plt.plot(frequencies, 20 * np.log10(np.abs(h_l)), label='LPF')
plt.axhline(y=minus_3db, color='r', linestyle='--', label='-3 dB Line')
plt.axvline(x=fc_l, color='g', linestyle='--', label='Cutoff Frequency')
plt.title('Low Pass Filter Magnitude Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Amplitude [dB]')
plt.grid()
plt.legend()

# LPF phase response
plt.subplot(3, 2, 2)
plt.plot(frequencies, np.angle(h_l), label='LPF')
plt.title('Low Pass Filter Phase Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Phase [radians]')
plt.grid()
plt.legend()

# HPF magnitude response
plt.subplot(3, 2, 3)
plt.plot(frequencies, 20 * np.log10(np.abs(h_h)), label='HPF')
plt.axhline(y=minus_3db, color='r', linestyle='--', label='-3 dB Line')
plt.axvline(x=fc_h, color='g', linestyle='--', label='Cutoff Frequency')
plt.title('High Pass Filter Magnitude Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Amplitude [dB]')
plt.grid()
plt.legend()

# HPF phase response
plt.subplot(3, 2, 4)
plt.plot(frequencies, np.angle(h_h), label='HPF')
plt.title('High Pass Filter Phase Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Phase [radians]')
plt.grid()
plt.legend()

# BPF magnitude response
plt.subplot(3, 2, 5)
plt.plot(frequencies, 20 * np.log10(np.abs(h_bpf)), label='BPF')
plt.axhline(y=minus_3db, color='r', linestyle='--', label='-3 dB Line')
plt.axvline(x=fc_h, color='g', linestyle='--', label='HPF Cutoff Frequency')
plt.axvline(x=fc_l, color='b', linestyle='--', label='LPF Cutoff Frequency')
plt.title('Band Pass Filter Magnitude Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Amplitude [dB]')
plt.grid()
plt.legend()

# BPF phase response
plt.subplot(3, 2, 6)
plt.plot(frequencies, np.angle(h_bpf), label='BPF')
plt.title('Band Pass Filter Phase Response')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Phase [radians]')
plt.grid()
plt.legend()

plt.tight_layout()
plt.savefig('filter_responses.svg', format='svg')
plt.show()

# Calculate DC Gain
dc_gain = np.sum(b_bpf) / np.sum(a_bpf)
print("DC Gain:", dc_gain)

# Calculate the group delay
frequencies_group_delay, group_delay = signal.group_delay((b_bpf, a_bpf), fs=fs)
plt.figure()
plt.plot(frequencies_group_delay, group_delay)
plt.title('Group Delay')
plt.xlabel('Frequency [Hz]')
plt.ylabel('Group Delay [samples]')
plt.grid()
plt.savefig('group_delay.svg', format='svg')
plt.show()

# Calculate the impulse response
t, impulse_response = signal.dimpulse((b_bpf, a_bpf, 1/fs))
impulse_response = impulse_response[0].flatten()
plt.figure()
plt.stem(t, impulse_response)
plt.title('Impulse Response')
plt.xlabel('Samples')
plt.ylabel('Amplitude')
plt.grid()
plt.savefig('impulse_response.svg', format='svg')
plt.show()

# Calculate the step response
t, step_response = signal.dstep((b_bpf, a_bpf, 1/fs))
step_response = step_response[0].flatten()
plt.figure()
plt.stem(t, step_response)
plt.title('Step Response')
plt.xlabel('Samples')
plt.ylabel('Amplitude')
plt.grid()
plt.savefig('step_response.svg', format='svg')
plt.show()

# Save the coefficients to a file
np.savetxt('filter_coefficients.txt', np.array([b_bpf, a_bpf]), delimiter=',', fmt='%1.10f')
