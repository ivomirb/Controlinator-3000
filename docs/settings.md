# Controlinator 3000 - Settings

Once the macro is running, it will add a Pendant button to the toolbar. From there you can connect the device, disconnect it, and edit the settings.

![Pendant settings](/assets/images/settings.png)

## Main Settings
**Connect Automatically** – check it if you want the pendant to auto-connect when OpenBuilds starts up

**Pendant Name** – set a name to be displayed on the pendant’s title screen. This can only be edited when the pendant is connected. The name is stored in the pendant’s EEPROM

**Display Units** – select if the pendant display should show the coordinates in inches or millimeters. It also affects the units for the wheel jog rate. It doesn’t change any other units in the pendant settings, which area always in mm

**Wheel Click Steps** – enter a list of movement distances for each click of the hand wheel. You can cycle between them using the Step button on the pendant. They don’t necessarily need to be in order. Pick an order that works for you, assuming that the first value in the list will be selected on startup

**Stop Spindle on Pause** – when this is enabled, pausing a job from the pendant (but not from the OpenBuilds toolbar) will automatically stop the spindle. This is done by triggering the door alarm in Grbl. Depending on the Grbl configuration, it may also raise the spindle to the park height. When you resume the job, the spindle will spin up for 4 seconds before it starts to move

**Jog Checklist** – enter up to 3 reminders to be shown on the pendant when you start a job. You will need to click the corresponding button to check off each one before you can continue

**Enable Safe Jogging Area** – this allows you to set an area in machine coordinates that is considered safe for jogging. Movement with the handwheel or the joystick will keep the machine inside the safe range

## Z Probe
**Z Probe Sequence** – select from “Single Pass”, “Dual Pass” or “Use default dialog”. Single and Dual pass allow you to select the travel distance and feed rate for the probe operation. Dual pass does 2 probings at different feed rates to improve accuracy.
When “Use default dialog” is selected, initiating a probe operation from the pendant will open the default dialog in OpenBuilds. It will allow you to use the extra settings for more complicated probing operations that are not available via the pendant alone. Of course, you will need to interact with the PC to access the extra settings

**Down Jog Speed** – select an override % rate for lowering the tool when the Z Down button is pressed on the pendant. Pick a speed that allows you to get close to the probe safely without running into it. The Z Up button is always at full speed because it is considered safe to go up

**Plate Thickness** – enter the thickness of your Z probe plate

**Use Exact Probe Result** – when probing, after the tool touches the probe, it would continue to travel for a few more steps before it stops. Open Builds and most other Grbl software use the final stop height. Check this box to use the original touch height instead. It can be more accurate

**Seek/Retract** – enter the travel distance and feed rate for each of the phases of the Z probing sequence

## Tool Probe
**Tool Probe Sequence** – choose between “Disabled”, “Single Pass” and “Dual Pass”. Single and Dual pass work the same way as in the Z probe settings

**Probe Location** – enter the X and Y location in machine coordinates of your tool sensor. Enter a safe Z location for approaching the sensor. When measuring the tool, the pendant will raise to Z first, then jog to the X/Y location right above the sensor

## Macros
Select up to 7 existing OpenBuilds macros to activate from the pendant. Enter the name of the macro in the first column. Auto-complete should help you find the right name.

Enter a short label in the second column. This is the name you will see on the pendant. There is limited space, so only 8 characters are allowed. If you leave it blank, you will get a generic name MACRO1, MACRO2, and such.

If you check the Hold checkbox, the pendant will require you to press and hold the button for 1 second before the macro is activated. Use this for dangerous operations that should not be triggered accidentally.

**Note on interactive macros:** Even though the pendant is limited to 7 macros, you can create powerful Javascript macros that can offer a lot of functionality. Use the following code:
`var ShowPendantDialog = $('#pendant').prop('ShowDialog');`
This will allow the macro to use the `ShowPendantDialog` function that shows a message on the pendant with 2 button options. Search for ShowPendantDialog in PendantMacro.js for documentation on how to use it and to find a few examples.


## Calibrate Joystick
This page allows you to calibrate the joystick. It shows the current joystick X and Y values, ranging from 0 to 1023. The green box shows the area that the joystick can reach. The red box is the inner dead zone, where the joystick is considered to be at rest.

You can edit the values manually at the bottom.

You can click the Calibrate Joystick button, which will begin the calibration sequence.

First, move the joystick to all extremes, and press OK on the pendant. This will determine how far the joystick can reach.

Afterwards, there are 8 tests to determine the dead zone. Push the stick away, then release it, then press OK. Do this a few times in different directions.

The calibration page is only available when the pendant is connected. The calibration values are stored in the pendant’s EEPROM, as they are tied to the actual joystick hardware.