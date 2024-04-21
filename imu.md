# Inertial Measurement Unit

An IMU usually consits of an acceleromter and a gyroscope to measure orientation of gravity and acceleration of the system as well as rotation of the system. 
Since gravity is strong, orientation of the device is usually accurate, however rotation in repsect to north, south, east and west requires a compass.
The magnetic field is often disturbed indoors through metal reinforcement in the concret and motors. 
Furthermore gyration at the level of earth's roation is too small to be measured with a MEMS based gyroscope.
Therfore a drift in the calculated pose of the device should be expected.
To compute translation and speed, gravity subtracted acceleration needs to be caclculated. 
However the integration of a signal with a strong background (gravity) leaads to run off, especially if position is computed with an integration of speed.

This IMU board is useful to measure physical activty. Movements can be categorized and a fall detected. Altitude can be estimated with the pressure sensor.
A step counter as well as a level can be built.

This board is based on latest IMU and pressure sensors and uses SPI interface accomodating a sampling rate exceeding 1000.

Code should inclde IMU arithmetics using quaternions as well as a Kalman filter for AHRS (additude and heading reference system) based pose estimation.

