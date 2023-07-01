# Controlinator 3000 - Usage

Here’s an explanation of most of the pendant screens and how to operate them.

As a general rule, if a button label has a down arrow next to it, then it requires the button to be pressed for 1 second to activate. A button with the > symbol opens a sub-menu.

The top of most screens shows the current machine status (Idle, Jog, Run).

## Main screen
![Main screen](/assets/images/main_screen.png)

It shows the current X/Y/Z values. Use the [MCS]/[WCS] button in the top-right to switch between machine and work coordinates.

Press and hold the Home button to initiate homing. If the machine is considered already homed, then you will be asked to confirm if you want to do this again.

Press X, Y or Z button to enter jog mode.

Press the joystick button, or X and Y at the same time to enter joystick jog mode.

Press Probe, Job or Macro to enter the corresponding sub-menus.

## Jog screen
![Jog screen](/assets/images/jog_screen.png)

If you select X, Y or Z, you are in single axis jog mode. Use the wheel to move the selected axis by discrete increments dependent on the current rate.

Short click the Step button to cycle between the available step options. They are configurable from the settings.

Long click on the Step button will align the axis to the nearest multiple of the step value. For example if you have selected 0.50 mm, then it will round to the nearest half millimeter. The rounding depends on the currently selected coordinate system. A rounded value in one of them is not necessarily round in in the other.

Press and hold To 0 to jog to the origin of that axis. The actual position depends on the currently selected coordinate system. If 0 is outside of the safe jogging area, the position will be limited to a safe value.

Press and hold Set 0 to set the current 0 for the axis. This is only available for the work coordinate system.

If you press both X and Y together, or if you click the joystick button, you will enter XY jog mode. Use the joystick to move.

If there is no input for 10 seconds, the jog screen will be dismissed and go back to the main screen. This is to avoid accidental movement when the pendant is put down.

## Probe screen
![Probe screen](/assets/images/probe_screen.png)

The probe screen allows you to measure the workpiece Z or the tool length. For the tool features to be available, they need to be configured from the settings and the machine needs to be homed.

Press the Z Up or Z Down buttons to move up and down. Be careful when moving down to avoid hitting something. There is a safety feature that will stop going down if the probe is touched, however it requires that the probe wire is connected and even then, there is still some latency that can cause damage.

Press and hold the Probe button to initiate the probe sequence. It can be a single pass or a dual pass, as selected from the settings.

When measuring a reference tool, the located Z value is simply remembered and doesn’t affect anything. When measuring a new tool, the new Z value will be subtracted from the reference tool and the difference will be applied to the work Z coordinate. This will allow you to retain the same work Z in relation to the new tool.

The reference value is retained by the PC for as long as the OpenBuilds software is running. If the software is closed, the value is lost and you will have to measure the current tool again as reference.

## Job screen
![Job screen](/assets/images/job_screen.png)

You can enter this screen from the main menu, either to start a new job or to manage one that is already running.

On the left it shows the current work space coordinates.

At the bottom you see the current Feed rate and Speed overrides (in percent). Press the corresponding button to select an override and then use the wheel to adjust the rates between 10% and 200%. This can be done before the job starts or while it is running.

The Run button starts the job. Hold down for 1 second to activate.

Press the Pause button to pause the job. This may also stop the spindle if it is configured in the settings. If not, you will get a button “RPM 0” which will allow you to stop the spindle while the job is paused.

Press and hold Resume to resume a paused job. If the spindle was stopped by the pendant, it will spin up for 4 seconds and then the movement will resume.

Press STOP to stop the job immediately.

## Macro screen

The macro screen allows you to execute up to 7 macro functions defined in the settings. Some buttons might require a press and hold before activating. Such items have the down arrow symbol.
