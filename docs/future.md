# Issues and future developments

There are a few things that can be improved in future versions. In no particular order:

One issue is that the buttons are disproportionately large compared to the screen, which leads to misalignment. Possibly the use of smaller buttons or a larger screen can improve this.

You can use buttons with built-in lights. This way the buttons that are inactive can be turned off, making it easier to associate a button with its label, even if they are not exactly aligned. There aren’t enough pins to control each light individually, however an 8-bit shift register like 74HC595 can be used to drive 8 LEDs with only 3 digital pins.

I would have liked a handwheel with fewer number of clicks per rotation for greater precision. The commonly available optical rotary encoders have 100 clicks. Even the “25 pulses per rotation” models still have 100 mechanical clicks, which is kind of useless.

A move from 8-bit Arduino to a more powerful microcontroller like ESP32 can improve the frame rate and responsiveness. A faster chip could be able to drive a larger TFT screen as well. They usually run at 3.3V, so some extra circuitry will be needed to interface with the 5V handwheel signal.

ESP32 boards come with Wi-Fi and other connectivity features, opening the possibility for a wireless pendant.

A buzzer could be added to warn about dangerous operations, inform of alarms, or confirm a button press. The PCB exposes pin D11 for such extensions.

The commonly available small joysticks have a very large outer dead zone. Basically, pushing the stick half-way maxes out the output and pushing it further doesn’t have any effect. I tried comparing samples from different suppliers and they all have the same problem. Likely they all use the same potentiometers. There may be better models out there. Possibly a higher-quality joystick can be salvaged from a game controller.

Instead of a handheld pendant, you may create a box that is attached to your CNC permanently. Change the layout of the controls to serve your design.

Currently the pendant is only compatible with OpenBuilds Control. It is possible to port the Javascript macro to other platforms that supports plugins. For example UGS can have plugins written in Java and bCNC can be extended with Python.
