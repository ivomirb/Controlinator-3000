# Controlinator 3000 - Software

The source code can be found here: https://github.com/ivomirb/Controlinator-3000  
It is published under the MIT public license.

All code is included except for the U8G2 library. You will need to install it separately in the Arduino IDE.  
If you are using a different pulse encoder wheel, you might need to install this library as well: https://github.com/gfvalvo/NewEncoder  
The Controlinator source code contains a trimmed-down version of that library that is limited to support one type of wheel devices.

Compile the code, and then upload it with the USB cable or the UPDI port. Be careful of the polarity when connecting the UPDI. If you plug it in backwards, the microcontroller will likely be safe because of the resistor, however your programmer device will be shorted to ground and may get damaged.

On the OpenBuilds side, create a new Javascript macro and paste the contents of PendantMacro.js inside it. Set the macro to auto-start and then restart OpenBuilds from the tray icon.

## Javascript settings
At the top of the Javascript file you will find a few values to fine tune the behavior. Read the comments and decide if you wish to change them.

## C++ settings
Look at the Config.h file for a few available settings. You can configure the code based on the available memory and what kind of performance you are looking for.

## Pendant emulator

The emulator is a Desktop application that emulates the physical controls of the device and shares the majority of the code with the microcontroller. This allows for faster development using Visual Studio as editor and debugger.

The emulator also contains a piece of code that generates font.h from the font bitmap. You need to run it if you change the font or add more special symbols.

You will need to establish a serial connection between OpenBuilds and the emulator. I used a software called “HHD Virtual Serial Port Tools” to create a pair of connected ports COM13 and COM14. The emulator runs on COM14 (hard-coded in Serial.cpp) and the Javascript macro connects on COM13.  
There is another software “com0com” which should provide similar functionality, though I was unable to get it to work.

An even more lazy solution is to eliminate the CNC altogether. I uploaded a version of Grbl onto a standalone Arduino. OpenBuilds recognizes it as a valid machine. You can use the 3d view and other controls in OpenBuilds to monitor the current state of that virtual CNC.

Of course, emulation is not the same as the real thing, and final testing should be done with the actual pendant and a real CNC machine. But it is quite sufficient for building the user interface and for coding the first pass of the functionality.