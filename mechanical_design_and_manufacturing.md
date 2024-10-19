# Mechanical Design and Manufacturing

All models in this project have been designed in OnShape. OnShape has a free academic license. Without an account you can still view the models. STL files are provided in the project. STL is a surface model and usually is not made to be edited. Although programs claim that you can export and convert into other formats, most export does not include your design process and you can not easily change the length or diameter of a component of an imported file.

The mechanical designs are not space optimized. They have beefy wall thickness and can accomodate larger items and more complex designs if necessary. They are robust for a classroom setting. All enclosures have the same size and all projects fit into the same dimensioned "brick". If the size is an issue for you, several electronic boards could be reduced to half the size and if optimized the enclosure could be reduced to 1/3. Largest project is the Airquality brick and its mostly full. The smallest project is the temperature and the IMU brick.

## Converting to STL
In OnShape select the parts on the left side menu and right click on a part. Export to STL. Download the STL file.

## 3D Printing the Models
Select your Slicer software. E..g. Prusa Slicer. Your printer configuration should be properly installed. If your printer uses input shapping (fast printing) you need to use proper printer settings, e.g. each Prusa printer has seperate driver depending if the firmware on the printer is using input shaping.

Import the STL file. Rotate the model to a surface resulting in the least amount of overhang. Avoid support material if you can. Depending on your printer bed, you might need a brim.Slice the model and inspect it. Export the g-code and load it on the printer.
