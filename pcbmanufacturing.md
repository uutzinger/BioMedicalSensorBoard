# PCB Manufacturing

## Parts
- LCSC for parts in China: https://www.lcsc.com/
- Digikey for parts in USA: https://www.digikey.com/en/products/
- Mouser for parts in USA: https://www.mouser.com/electronic-components/

## PCB Manufacturing and Assembly

- [PCBWAY](https://www.pcbway.com/quotesmt.aspx)
- [JLPCP](https://jlcpcb.com/)
- [JLPCB Part List](https://jlcpcb.com/parts)

### Costs PCB Way

- Minimum number of PC Boardss is 5 and costs $45.18
- Minimum number for assempty is 1
- Assembly of 1 to 20 costs $88
- Assembly of 50 costs $154
- Parts costs are separate
- Through hole component costs exta
- Shipping costs are often equivalent to a discount you receive with your order

### PCB Way Assembly Service Settings
- Turnkey
- Single pieces
- Top Side board
- Quantity (assembled) e.g. 2
- Contains sensitive components: No
- Number of unique parts: See Bill of Materials (number of individual lines)
- Number of SMD parts: Total number of parts
- Number of BGA/QFP: Parts with very small pads
- Through hole parts: You can solder yourself
- Board type: single pieces
- Different designs: 1
- Size: 70x70 (all boards in this project)
- Quantity 5
- Layers: 2
- Material: FR-4, TG 150-60
- Thickness 1.6
- Min spacing 6mil
- Min hole size 0.3, except for micro via boards (BIOZ)
- Soldermark: choose your color (white)
- Edge connector: No
- Surface finish: Immersion Gold
- Thickness of immersion: 1U
- Via process: tenting
- Finished copper: 1 oz
- Remove product #: No, or pay extra fee to not have PCB print their part number on the board

-------------------------------------
-------------------------------------

## Eagle CAD settings

### Grid
-Size: 1mm
-Alt: 0.1mm

### DRC
Units are mil, second value given below is for Waver Level Packaging (very small ICs)

- Layers: Core 1.5mm
- Clearance: Different Signals: Wire-Wire:6, Wire-Pad:6/3. Wire-Via:6/3, Pad-Pad:6, Pad-Via:6/3, Via-Via:6/3
- Clearance: Same Signals: Smd-Smd:6/3, Pad-Smd:6/3,Via-Smd:6/0
- Distance: Copper/Dimension:40, Drill-Hole:6
- Sizes: Min:6/4, Drill:0.35/0.1mm, MicroVia:9.99/0.1mm, BlineViaRatio:0.5/0.05
- Annular Ring: Pads Top:10/3,25%,20 Inner:20/3.25%.20 Bottom:10/3,25%,20
- Annular Ring: Vias: Outer:8/2,25%,20 Inner:8/2,25%,20
- Annular Ring: MicroVias: Outer:4/3,25%,20 Inner:4/3,25%,20
- Shapes Smds Roundness 0,0%,0
- Shapes Pads Top: as in lib, Bottom: as in lib, First: not special, Elongation 100,100
- Supply: Termal isolation: 10
- Masks: Stop:4/3,100%,4 Cream:0,0,0 Limit 15
- Misc: Check font, check restrict, check names, check stubs, differential pairs 10mm, gap factor 2.5

## Steps in Eagle CAD for Computer Aided Manufacturing

The BOM needs to matches the manufacturers requirement. 
Your design needs to match manufacturing tolerances. 
For micro vias or WLP you likely need to select advanced manufacturing settings at the manufacturers website.

1) Make sure your board has no errors (Tools->Errors)
2) Check on Manufacturing Tab the graphical representation of your manufactured board
3) CAM Processor -> Local CAM Jobs -> examples -> example_2_layer.cam
    Enable ZIP (this creates Gerber file archive)
4) Upload. 

-------------------------------------
-------------------------------------

