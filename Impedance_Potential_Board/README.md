# MAX30001G Design

## Diagrams

### Block Diagram of MAX 30001G

![Block](./assets/Blockdiagram.png)

### External Configurations

![External](./Design.svg)

## Board Configuration

- We can operate the board to measure ECG or Impedance or both. 
- We can operate the board to use only two leads, four leads or six leads. 
- We can configure the board to measure impedance of a calibration resistor that is soldered onto the board (100 Ohm).

We have the following external connectors:
- ECG, 3,5mm audio
    - ECG_N (middle of plug) e.g. left arm
    - ECG_P (proximal side of plug) e.g. right arm
    - VCM (common mode) (tip of plug) e.g. leg

- Impedance, 3.5mm audio
    - BI_N
    - BI_P
    - GND (tip of plug)

- Driver, 3.5mm audio
    - DRV_P
    - DRV_N
    - GND (tip of plug)

##  Board Designs

Typical values and conditions of external components

Name      | typical value | Comment
----------|---------------|--------
C_ECG     | 2.2nF         | datasheet
C_ECG-G   | 10pF          | datasheet, 47pF protocentral
EP_EN     | open          | disable ECG
R_ECG     | 0,51k,200k Ohm | 0, 200k shown in datasheet (200k when ECG conencted with BIOZ), 51k protocentral, 0 Ohm evaluation kit
R_ECG-B   | 51k           | evaluation kit
C_ECG_B   | 47nF          | evaluation kit
ECG_UNBAL | open          | disable balancing by closing
||
EP_BP     | open          | Impedance and ECG is on same plug
EN_BN     | open          | Impedance and ECG is on same plug
||
C_BIN     | 47pF          | datasheet
C_BIN_G   | 10pF          | datasheet
BP_BN     | open          | disable BioZ
R_BIN     | 0, 200 Ohm    | 200 protocentral where ECG is connected to BIN, 0 evaluation kit
R_CAL     | 100 Ohm       | evaluation kit
RP        | open          | if closed no external impedance
RN        | open          | "
||
BP_DP     | open          | Impedance and Driver on same plug
BN_DN     | open          | Impedance and Driver on same plug
||
C_DRV     | 200nF, 47nF   | 47nF protocentral, 47nF evaluation kit
||
R_VCM     | 0 Ohm,10k,200k| 10k protocentral, evaluation kit has buffer driver cirtuit, 200k datasheet
R_VCM-B   | 51k, none     | none potocentral & evaluation kit
C_VCM-B   | 47nF, none    | none protocontral & evaluation kit, 47nF datasheet
VMC_UNBAL | open          | 

Specifically: 

### Protocentral 30001 Break Out
Single Connector

- VCM **10k** in series, no capacitor in series
- VBG same circuit and capacitors
- RBIAS same circuit, resistor value not given
- CAPN, CAPP same circuit, same capacitor
- DRVN, DRVP same circuit **47nF** in stead of 220nF
- DRVN, DRVP, hardwired to ECGN and ECGP
- BINN, BINP same design, no calibration resistor, 
- BINN, BINP hardwired to ECGN and ECGP
- ECGN, ECGP same design 47pF to GND instead of 10pF, 51k inline but no capacitor
- OVDD on 1.8V, logic level conversion to 3.3V
- no over voltage protection

### Protocentral tinyECG
ECG and BIOZ different connectors, DRV and BIN on the same connector

- VCM **10k** in series instead of 51k, no capacitor in series
- VBG same circuit and capacitors
- RBIAS same circuit, resistor value not given
- CAPN, CAPP same circuit, same capacitor
- DRVN, DRVP same circuit **47nF** in stead of 220nF
- BINN, BINP same design, no calibration resistor, 
- BINN, BINP hardwired to DRVN and DRVP
- BINN, BINP **200** Ohm resistor instead of 0 Ohm R19, R20
- ECGN, ECGP same design **47pF** to GND instead of 10pF, same inline resistor but no capacitor
- OVDD on 3.3V
- no over voltage protection

### Evluation Kit
- VCM **10k** in series instead of 51k, no capacitor in series, additional offset option
- VBG same circuit and capacitors
- RBIAS same circuit, resistor value not given
- CAPN, CAPP same circuit, same capacitor
- DRVN, DRVP same circuit, 47nF in general example, 220nF BioZ, GSR
- BINN, BINP same design
- BINN, BINP jumper option to DRVN and DRVP
- BINN, BINP 0 Ohm R19, R20
- ECGN, ECGP same design, same inline resistor and capacitor
- ECGN, ECGP jumper option to BINN, BINP
- OVDD on 3.3V
- no over voltage protection

### Datasheet typical ECG and GSR
- VCM **200k** in series instead of 51k, no capacitor in series
- VBG same circuit and capacitors
- RBIAS same circuit, same 324k resistor
- CAPN, CAPP same circuit, same capacitor
- DRVN, DRVP same circuit, 47nF 
- BINN, BINP separate connector
- BINN, BINP same design
- BINN, BINP resistor not specified Ohm R19, R20
- ECGN, ECGP same design
- ECGN, ECGP connected to BINN, BINP
- OVDD up to 3.6V

### Datasheet typical simple ECG
- ECG direct with protection resistor, no capacitor network
- no DRV and BIN connection

### Jumpers

There are 4 sets of jumper:

1) The following jumpers are available to reduce the numnber of required leads. They create direct conenction between the pins of the plug and the chip IO:

    For single plug operation ECG&BioZ
    - $ECG_N$ to $BI_N$
    - $ECG_P$ to $BI_P$

    For single plug BioZ operation
    - $DRV_N$ to $BI_N$
    - $DRV_P$ to $BI_P$

2) The unblance filters for Input and Offset can be bypassed. Those are the two input pins for ECG, Bioimedance and VCM :

    - $VCM_{UB}$: $VCM$ unbalance bypass
    - $EN_{UB}$: $ECG_N$ unbalance bypass
    - $EP_{UB}$: $ECG_P$ unbalance bypass

3) We can also disable input:

    - $ECG_P$ to $ECG_N$ (this shorts N and P terminal together, for BIOZ measurements only operation)
    - $BI_P$ to $BI_N$ (this shorts N and P terminal together, for ECG measurements only operation)

4) We can calibrate BIOZ with a 100 Ohm resistor:

    - $R_N$, $R_P$ connect $BI_N$, $BI_P$ to 100 Ohm (R18), for testing purose only, once conencted no external impedance can be measured.

If $V_{CM}$ is used for ECG right leg bias, internal lead bias needs to be disabled. Resistor can be 10k..200kOhm.

For specific hardware configuration setup the following jumpers should be set:

### Configuration Options

Configuration          | $ECG_P$ to $ECG_N$ | $DRV_N$ to $BI_N$ | $DRV_P$ to $BI_P$ | $ECG_N$ to $BI_N$ | $ECG_P$ to $BI_P$ | $R_N$ | $R_P$ | $VCM_{UB}$ | $EN _{UB}$ | $EP_{UB}$ |
-----------------------|--------------------|-------------------|-------------------|-------------------|-------------------|-------|-------|------------|------------|-----------|
(1) 100 Ohm Test       |                    | X                 | X                 |                   |                   | X     | X     |            |            |           | 
(2) ECG, R to R        |                    |                   |                   |                   |                   |       |       | *          |            |           | 
(3) ECG & Resp 2 leads |                    | X                 | X                 | X                 | X                 |       |       | *          |            |           | 
(4) ECG & Resp 4 leads |                    |                   |                   | X                 | X                 |       |       | *          |            |           | 
(5) GSR                | X                  | X                 | X                 |                   |                   |       |       |            |            |           | 
(6) BIOZ 2 leads       | X                  | X                 | X                 | X                 | X                 |       |       |            |            |           | 
(7) BIOZ 4 leads       | X                  |                   |                   |                   |                   |       |       |            |            |           | 
(8) BIOZ & ECG 2 leads |                    | X                 | X                 | X                 | X                 |       |       | *          |            |           | 
(9) BIOZ & ECG 4 leads |                    | X                 | X                 |                   |                   |       |       | *          |            |           | 
(10) BIOZ & ECG 6 leads|                    |                   |                   |                   |                   |       |       | *          |            |           | 

(X) = solder jumper closed

(*) Might need to replace R23 with 200kOhm and enable VCM bypass by soldering the jumper close.

### Lead Connections

(1) **100 Ohm BioZ Test**

    - 100 Ohm test for Driver and Impedance (BioZ)
    - No external BIOZ leads possible
    - ECG Measurement leads on ECG connector possible
    - BioZ frequency is FMSTR/64 = 500Hz
    - Increase frequency or change internal lower filter cut off

(2) **ECG** 
    - Measurement Leads on ECG connector.

(3) **2 Leads ECG**

    - Measurement Leads on ECG connector.

(4) **4 Leads ECG**

    - Driver Leads to Driver connector.
    - Measurement Leads to ECG or Impedance connector.

(5) **GSR, no ECG**

    - Measurement Leads on Impedance connector.
    - Low current frequency generator at 125, 250 or 500Hz.
    - BIOZ analog high pass needs to be below current frequency. 
    - 220nF blocking filter inline in $DRV_P$ and $DRV_N$, other circuits might have 47nF

(6) **2 Leads Bio Z**

    - BioZ Measurement connector connected to Driver connector

(7) **4 Leads Bio Z**

    - Driver Leads to Driver connector.
    - Measurement Leads to Impedance connector.

(8) **2 leads ECG & Bio Z**

    - You can not measure simultanously but you can reconfigure in software to measure one or the other
    - BIN and ECG and DRV connected

(9) **4 leads ECG & Bio Z**

    - You can not measure simultanously but you can reconfigure in software to measure one or the other
    - ECG separate, BIN & DRV connected

(9) **6 leads ECG & Bio Z**

    - You can not measure simultanously but you can reconfigure in software to measure one or the other
    - No jumper

