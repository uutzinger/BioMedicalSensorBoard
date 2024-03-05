# PCB Manufacturing

## Parts
- LCSC for parts in China: https://www.lcsc.com/
- Digikey for parts in USA: https://www.digikey.com/en/products/
- Mouser for parts in USA: https://www.mouser.com/electronic-components/

## PCB Manufacturing and Assembly

- [PCBWAY](https://www.pcbway.com/quotesmt.aspx)
- [JLPCP](https://jlcpcb.com/)
- [JLPCB Part List](https://jlcpcb.com/parts)

## Steps in Eagle CAD for Computer Aided Manufacturing

1) Make sure your board has no errors (Tools->Errors)
2) Check on Manufacturing Tab the graphical representation of your manufactured board
3) CAM Processor -> Local CAM Jobs -> examples -> example_2_layer.cam
    Enable ZIP (this creates Gerber file archive)
4) In schematic run ULP -> Browse for eagle_bom.ulp in this GIT repository -> CSV export and comma separator, not semi colon (this creates bill of materials)
5) In board run ULP -> Browse for eagle_smt.ulp (this creates part placement instructions)
6) Edit BOM and adjust it to PCBWay example, choose manufacturers and part numbers, check availability and make sure foot print matches. Save as XLSX
7) Upload to PCBWay. 

For an other preferred manufacturer make sure BOM matches their requirement. Also make sure the manufacturing tolerances are in line with your design.
