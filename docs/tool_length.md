# Controlinator 3000 - Tool length

When a job uses multiple tools, you have to ensure that the Z position is preserved from one tool to the next. Probing for Z is not always an option, and is error-prone. The best way is to adjust the tool length offset (TLO) as the tool is replaced.

The TLO implementation in Grbl is not that good. You can set the value, however it is lost after a soft reset, which can happen quite often. There is very little indication when that happens.

The pendant software uses different approach to adjust the Z offset when the tool is changed. It requires a tool sensor to be installed on the CNC. A tool sensor is nothing more than a Z probe at a fixed location. In the pendant settings, enter the X and Y location of the sensor and select the probing method. The settings are similar to the Z probe.

You start your job as normal with the first tool. Use the regular Z probe to measure your work piece. Before switching to the second tool, select the Probe option from the main menu on the pendant, then select "Probe Ref Tool". You will use the tool sensor to measure the absolute height of the current tool. It will become the reference for all future tools.

Replace the tool in the spindle, and then select "Probe New Tool". The pendant will measure the new tool, determine the difference in the length compared to the reference tool, and will apply the difference to the current Z offset.

If you ever forget to probe the next tool and lose your reference, you will need to select "Probe Ref Tool" again. As long as you have a tool that matches your current work coordinate system, you can successfully use it as a reference for future tools.  
**Note:** the reference height will be lost if the OpenBuilds software restarts. Again, just make sure you have correct work Z, and set the current tool as a reference.

Since the pendant software will jog to the tool sensor location in machine coordinates, it requires that the machine is homed. If the pendant thinks that the machine has lost its homing position, it will show a warning message. You can ignore it if you think it is safe.

## Tool Sensor Variants

There are different ways to create a tool sensor. The important thing is that the sensor doesn't move between measurements.  
The sensor will be connected in parallel with the regular Z probe, meaning it will trigger the Z probe signal when the tool touches the sensor.

Here are few possible ways to do it:

1. Use a regular Z probe plate that is securely attached somewhere. You will need to connect the alligator cable to your spindle every time you measure the tool.

2. Use an off the shelf tool sensor, maybe something like this one: https://www.amazon.com/RATTMMOTOR-Automatic-Normally-Setting-Engraving/dp/B08LK93NTC  
I have not tried such a device myself. Those are nice as they have a secondary contact that is triggered if the button is pushed too far. You can wire it to the E-Stop to shut down everything before any damage is done. There is also an option to connect compressed air to blow the chips off of the tool during probing.

3. Make one yourself using a push button. This is what I ended up with.  
The advantage of using a button over Z plate is that you don't need to connect the alligator cable. You can have the sensor out of the way in the far corner of the machine.
![Tool sensor](/assets/images/tool_sensor.jpg)

You can find the 3D models here: https://github.com/ivomirb/Controlinator-3000/tree/master/Models  
There is a top and bottom part, connected with M3 threaded inserts.  
It uses a 16mm push button like this one: https://www.aliexpress.us/item/2251832680756878.html  
A larger button will work even better because it will be able to handle larger tools, like a spoilboard flattening cutter.
