# Electronic Design

All electronic designs were comleted with EagleCAD. The files can also be imported into KiCAD as EagleCAD will no longer be supported by its owner.

Files ready for submission to PCB manfacutreres are in each project subfolder.

## Eagle CAD settings

### Grid
-Size: 1mm
-Alt: 0.1mm

### DRC (Design Review ...)

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

## From CAD to Manufacturing

The BOM needs to matches the manufacturers requirement. 
Your design needs to match manufacturing tolerances. 
For micro vias or WLP you likely need to select advanced manufacturing settings at the manufacturers website.

1) Make sure your board has no errors (Tools->Errors)
2) Check on Manufacturing Tab the graphical representation of your manufactured board
3) CAM Processor -> Local CAM Jobs -> examples -> example_2_layer.cam
    Enable ZIP (this creates Gerber file archive)
4) Upload. 

