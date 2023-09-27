// OpenBuilds CONTROL Javascript macro for interfacing with the Controlinator 3000 pendant
// https://github.com/ivomirb/Controlinator-3000

// Normally the Arduino Nano is programmed via the USB cable. This requires the Arduino to reset when a serial connection is initiated.
// If this is the case, set the value to true. The PC will wait 2 seconds after opening the connection before trying to communicate.
// You can connect a capacitor between RST and GND, which will disable the reset and improve the speed of the initial connection.
// If this is the case, set the value to false.
// Also if you use a microcontroller that is programmed via the UPDI pin instead of USB, set this to false as well.
const ARDUINO_RESETS_ON_CONNECT = true;

// This flag causes the Z Down button in the probing screen to use the G38.3 command instead of $J, which will stop if the probe is touched.
// It is not entirely safe because plunging into a hard probe at jogging speeds is never safe, but might reduce the damage to the probe or the endmill.
// It will work better if the probe uses a button with some overtravel capability, like many button-based tool sensors.
// The downside is the loss of some responsiveness for the Up and Down buttons.
const SAFE_PROBE_JOG = true;

// The step size for changing the feed or speed override. Smaller number allows for more precise control
const WHEEL_OVERRIDE_STEP = 5; // 5% per click

// This value controls how much travel time to submit to Grbl ahead of time. Longer value makes the jog smoother, but less responsive.
const WHEEL_JOG_AHEAD_TIME = 600; // up to 600ms of jog time submitted to Grbl

// This value controls how long after you stop turning the wheel the jog will stop. Smaller number will make the stop
// more responsive and prevent long runaway jog movements. It comes at the expense of lost clicks of the wheel.
// To avoid losing any clicks (the machine will move the exact number of clicks), use a very large number like 100000
const WHEEL_JOG_STOP_TIME = 100; // the jog will stop 100ms after the last click

// These must match Main.h
const PENDANT_VERSION = "1.1";
const PENDANT_BAUD_RATE = 38400;

// Must match Input.h
const JOYSTICK_STEPS = 100;

const COM_LOG_LEVEL = 0; // 0 - none, 1 - commands, 2 - everything, 3 - everything+ACK

const {
	SerialPort
} = require('serialport')
const {
	ReadlineParser
} = require('@serialport/parser-readline');

var g_PendantPort;

// Special characters
const CHAR_UNCHECKED = String.fromCharCode(0x01);
const CHAR_CHECKED = String.fromCharCode(0x02);
const CHAR_HOLD = String.fromCharCode(0x03);
const CHAR_ACK = String.fromCharCode(0x1F);
const MAX_MESSAGE_LENGTH = 50; // send up to 50 bytes to the pendant and then wait for ACK. Arduino has only 64 bytes of buffer for the serial connection

var g_StatusCounter = 0;
var g_JobProgress;
// Status cache - store the last sent value to avoid spamming when nothing changes
var g_LastStatusStr = undefined;
var g_LastStatus2Str = undefined;

var g_PendantSettings = GetDefaultSettings();
var g_PendantRomSettings = GetDefaultRomSettings();

var g_TloRefZ = undefined; // absolute Z of the tool probe measured with the reference tool
var g_ProbeZDownFeed = 50; // feed rate for the Z Down button (in % of $112)
var g_ProbeJog; // -1 - up, 1 - down
var g_LastProbeZ;

var g_SafeStopPending = 0; // 0 - ignore, 1 - clear on idle, 2 - don't clear on idle
var g_LastBusyTime = undefined;
var g_LastUpdateTime = undefined;
var g_bOkEventSupported = false; // the stock Control sofware doesn't send "ok" event. a custom version might

// This table must match the names and order in MachineStatus.h
const g_StatusMap =
{
	"Unknown": 0,
	"Disconnected": 1,

	"Jog": 2,
	"Run": 3,
	"Check": 4,
	"Home": 5,
	"Running": 6,
	"Resuming": 7,
	"Door:3": 8,

	"Idle": 9,
	"Hold:0": 10,
	"Hold:1": 11,
	"Door:0": 12,
	"Door:1": 13,
	"Door:2": 14,
	"Alarm": 15,
	"Sleep": 16,
	"Stopped": 17,
	"Paused": 18,
};

// Constructs a container with the default settings
function GetDefaultSettings()
{
	return {
		// main settings
		autoConnect: true,
		displayUnits: "mm",
		wheelStepsMm: [1, 0.1, 0.01],
		wheelStepsIn: [0.1, 0.01, 0.001],
		pauseSpindle: false,
		jobChecklist: "Remove Probe|Turn On Spindle",
		safeLimitsEnabled: true,
		safeLimitsX: {min: 0, max: 0},
		safeLimitsY: {min: 0, max: 0},
		safeLimitsZ: {min: 0, max: 0},

		// Z probe
		zProbe: {
			style: "single",
			downSpeed: 50,
			seek1: {travel:    15, feed:  100},
			retract1: {travel:  5, feed: 1000},
			seek2: {travel:     1, feed:   50},
			retract2: {travel:  5, feed: 1000},
			thickness: undefined,
			useProbeResult: false,
		},

		// tool probe
		toolProbe: {
			style: "disable",
			downSpeed: 50,
			seek1: {travel:    15, feed:  100},
			retract1: {travel:  5, feed: 1000},
			seek2: {travel:     1, feed:   50},
			retract2: {travel:  5, feed: 1000},
			location: {X: 0, Y: 0, Z: 0},
		},

		// macros
		macro1: { name: "", label: "", hold: false},
		macro2: { name: "", label: "", hold: false},
		macro3: { name: "", label: "", hold: false},
		macro4: { name: "", label: "", hold: false},
		macro5: { name: "", label: "", hold: false},
		macro6: { name: "", label: "", hold: false},
		macro7: { name: "", label: "", hold: false},
	}
}

function EscapeHtml(string)
{
	return string.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;').replaceAll("'", '&#039;');
}

// Constructs a container with the default settings stored in the pendant's ROM
function GetDefaultRomSettings()
{
	return {
		pendantName: "Controlinator 3000",
		calibrationX: [0, 1023, 512-64, 512+64],
		calibrationY: [0, 1023, 512-64, 512+64],
	};
}

function ClearStatusCache()
{
	g_LastStatusStr = undefined;
	g_LastStatus2Str = undefined;
}

// Generates a string for the main status
function GenerateStatusString(s)
{
	// main status string - STATUS:<grbl status>|workX,workY,workZ|feed%,rpm%,realFeed,realRpm
	var status;
	if (s.comms.connectionStatus == 0)
	{
		status = g_StatusMap.Disconnected;
	}
	else if (s.comms.connectionStatus == 5 && g_ErrorListeners == undefined)
	{
		status = g_StatusMap.Alarm;
	}
	else
	{
		status = g_StatusMap[s.comms.runStatus];
		if (status == g_StatusMap.Running && (g_JogXYLocation != undefined || g_JogWQueue != undefined || g_ProbeJog != undefined))
		{
			status = g_StatusMap.Jog;
		}
	}

	var str = "STATUS:" + status + "|"
		+ s.machine.position.work.x.toFixed(3) + ","
		+ s.machine.position.work.y.toFixed(3) + ","
		+ s.machine.position.work.z.toFixed(3) + "|"
		+ (g_TargetFeedRate != undefined ? g_TargetFeedRate : s.machine.overrides.feedOverride.toFixed(0)) + ","
		+ (g_TargetSpeedRate != undefined ? g_TargetSpeedRate : s.machine.overrides.spindleOverride.toFixed(0)) + ","
		+ s.machine.overrides.realFeed.toFixed(0) + ","
		+ s.machine.overrides.realSpindle.toFixed(0);
	if (status == g_StatusMap.Run && g_JobProgress != undefined)
	{
		str += "|" + g_JobProgress.toFixed(0);
	}
	return str;
}

// Generates a string for the secondary status (values that change less often)
function GenerateStatus2String(s)
{
	var tlo = 0;
	if (g_PendantSettings.toolProbe.style != "disable")
	{
		tlo = g_TloRefZ == undefined ? 1 : 3;
		if (Math.abs(s.machine.position.work.x + s.machine.position.offset.x - g_PendantSettings.toolProbe.location.X) <= 1 &&
				Math.abs(s.machine.position.work.y + s.machine.position.offset.y - g_PendantSettings.toolProbe.location.Y) <= 1)
		{
			tlo += 4;
		}
	}

	// secondary status string - STATUS2:[J][H][P]<tlo>offsetX,offsetY,offsetZ
	var str = "STATUS2:"
		+ (lastJobStartTime ? "J" : "")
		+ (s.machine.modals.homedRecently ? "H" : "")
		+ (s.machine.inputs.includes('P') ? "P" : "")
		+ tlo
		+ s.machine.position.offset.x.toFixed(3) + ","
		+ s.machine.position.offset.y.toFixed(3) + ","
		+ s.machine.position.offset.z.toFixed(3);
	return str;
}

// Sends the status strings to the pendant
function PushStatus(status)
{
	if (g_PendantPort)
	{
		var statusStr = GenerateStatusString(status);
		if (g_LastStatusStr != statusStr)
		{
			WritePort(statusStr);
			g_LastStatusStr = statusStr;
		}

		statusStr = GenerateStatus2String(status);
		if (g_LastStatus2Str != statusStr)
		{
			WritePort(statusStr);
			g_LastStatus2Str = statusStr;
		}
	}
}

// Processes the status from the machine
function HandleStatus(status)
{
	g_StatusCounter++;
	g_LastUpdateTime = Date.now();
	if (status.comms.runStatus == "Idle")
	{
		// update wheel jog location on every idle
		if (g_SafeStopPending == 1) { g_SafeStopPending = 0; }
		g_JogCancelPendingTime = undefined;
	}
	else
	{
		g_LastBusyTime = Date.now();
		if (g_SafeStopPending != 0 && status.comms.runStatus == "Hold:0")
		{
			socket.emit('stop', {stop: true, jog: false, abort: false});
			g_SafeStopPending = 0;
		}
	}

	// send to pendant
	PushStatus(status);
}

// Handles progress update
function HandleQueueStatus(data)
{
	var left = data[0]
	var total = data[1]
	var done = total - left;
	g_JobProgress = done / total * 100;
}

// Returns true if the machine was idle for at least 500ms, and the last status update was no more than 300ms ago
function IsStableIdle()
{
	if (g_LastUpdateTime != undefined)
	{
		var t = Date.now();
		return ((g_LastBusyTime == undefined) || (t - g_LastBusyTime >= 500)) && t - g_LastUpdateTime <= 300;
	}
	return false;
}

// Handles the "ok" event from the Control software. The stock version of the software doesn't send that event.
// You will need a custom version that does
function HandleOk(command)
{
	if (typeof(command) == 'string')
	{
		g_bOkEventSupported = true;
		if (g_JogXYState != undefined && command.startsWith("$J="))
		{
			UpdateJogXY();
		}
	}
}

// Handles the job completion event
function HandleJobComplete(data)
{
	if (COM_LOG_LEVEL >= 1 && (data.jobStartTime || data.jobCompletedMsg.length > 0))
	{
		console.log("JOB COMPLETED", data);
	}

	g_JobProgress = undefined;
	if (g_PendantPort == undefined)
	{
		return;
	}

	if (!g_bAdvancedJogXY)
	{
		// if the "ok" event is not supported, we use a heuristic to detect when a command is processed and it is safe to cancel the current jog
		if (data.failed && g_JogCancelPendingTime != undefined)
		{
			if (Date.now() - g_JogCancelPendingTime < 300)
			{
				socket.emit('stop', {stop: false, jog: true, abort: false});
			}
			g_JogCancelPendingTime = undefined;
			return;
		}
	}

	if (!data.failed && data.jobStartTime && data.jobEndTime && laststatus.comms.runStatus != "Alarm")
	{
		// this is a real job
		var runTime = data.jobEndTime - data.jobStartTime;
		var seconds = Math.floor((runTime / 1000) % 60);
		var minutes = Math.floor((runTime / (1000 * 60)) % 60);
		var hours = Math.floor((runTime / (1000 * 60 * 60)) % 24);

		hours = (hours < 10) ? "0" + hours : hours;
		minutes = (minutes < 10) ? "0" + minutes : minutes;
		seconds = (seconds < 10) ? "0" + seconds : seconds;

		ShowPendantDialog({
			title: "Done",
			text: [
				"Job completed",
				"successfully",
				"in " + hours + "h " + minutes + "m " + seconds + "s"
				],
			rButton: ["OK"],
		});
		return;
	}

	if (data.jobCompletedMsg.startsWith("Probe Complete"))
	{
		// probing job
		var probeType = Number(data.jobCompletedMsg[14]);
		data.jobCompletedMsg = "";
		if (!data.failed)
		{
			HandleProbeJobComplete(probeType, g_LastProbeZ);
		}
		g_LastProbeZ = undefined;
	}
}

// Responds to a handshake command
function HandleHandshake()
{
	ClearStatusCache();
	PushSettings(false);
	PushStatus(laststatus);
}

const MAX_BUTTON_SIZE = 14; // must match the DialogScreen class
var g_NextDialogId = 1;
var g_DialogId; // needs to fit in 16 bits, can't be 0
var g_DialogLCallback;
var g_DialogRCallback;

// Shows a dialog on the pendant
// dialog.title - optional title for the dialog
// dialog.text - an array of up to 3 lines
//    if the line starts with CHAR_UNCHECKED, it shows a checkbox
//    lines can be up to 18 characters
//    lines starting with a checkbox or ^ are left-aligned
// dialog.lButton [string, callback] -  optional left button
//    if the button starts with CHAR_HOLD, it requires long hold
//    if the button starts with "!", the dialog waits for half second before it is dismissed (CHAR_HOLD is before ! if both are used)
//    if the button text is "@", it shows no text but is automatically executed when all checkboxes are marked
//    the button is hidden unless all checkboxes are marked
// dialog.rButton [string, callback] - optional right button
//    if the button starts with "!", the dialog waits for half second before it is dismissed
//    if the button ends with CHAR_HOLD, it requires long hold
// buttons are up to 14 characters each, but both combined should not exceed 16
function ShowPendantDialog(dialog)
{
	g_DialogId = g_NextDialogId;
	g_NextDialogId++;
	if (g_NextDialogId == 65536)
	{
		g_NextDialogId = 1;
	}
	var str = "DIALOG:" + g_DialogId + "|";
	if (dialog.title != undefined)
	{
		str += dialog.title.replaceAll('|', '_').substring(0, 18);
	}
	str += "|";
	for (var line = 0; line < 3; line++)
	{
		if (dialog.text && dialog.text.length > line)
		{
			str += dialog.text[line].replaceAll('|', '_').substring(0, 18);
		}
		str += "|";
	}
	if (dialog.lButton)
	{
		str += dialog.lButton[0].replaceAll(',', '_').substring(0, MAX_BUTTON_SIZE);
		g_DialogLCallback = dialog.lButton.length > 1 ? dialog.lButton[1] : undefined;
	}
	str += ",";
	if (dialog.rButton)
	{
		str += dialog.rButton[0].replaceAll(',', '_').substring(0, MAX_BUTTON_SIZE);
		g_DialogRCallback = dialog.rButton.length > 1 ? dialog.rButton[1] : undefined;
	}
	WritePort(str);
}

// Sends the new settings to the pendant
function PushSettings(pushRomSettings)
{
	// main settings
	if (g_PendantSettings.displayUnits == "inches")
	{
		var str = "UNITS:I";
		var steps = g_PendantSettings.wheelStepsIn;
		for (var idx = 0; idx < steps.length; idx++)
		{
			str += "|" + (steps[idx]*1000).toFixed(0);
		}
		WritePort(str);
	}
	else
	{
		var str = "UNITS:M";
		var steps = g_PendantSettings.wheelStepsMm;
		for (var idx = 0; idx < steps.length; idx++)
		{
			str += "|" + (steps[idx]*100).toFixed(0);
		}
		WritePort(str);
	}

	// macros
	var str = "";
	var flags = 0;
	for (var idx = 1; idx <= 7; idx++)
	{
		var macro = g_PendantSettings["macro" + idx];
		str += "|";
		if (macro.name != "")
		{
			var hold = macro.hold;
			if (hold)
			{
				flags |= 1 << (idx-1);
			}
			var label = macro.label.replaceAll('|', '_');
			if (label == "")
			{
				label = "MACRO " + idx;
			}
			str += label;
		}
	}
	WritePort("MACROS:" + flags + str + "|");

	// ROM settings
	if (pushRomSettings)
	{
		WritePort("NAME:" + g_PendantRomSettings.pendantName.substring(0, 18));
		WritePort("CALIBRATION:"
			+ g_PendantRomSettings.calibrationX[0] + ","
			+ g_PendantRomSettings.calibrationX[2] + ","
			+ g_PendantRomSettings.calibrationX[3] + ","
			+ g_PendantRomSettings.calibrationX[1] + ","
			+ g_PendantRomSettings.calibrationY[0] + ","
			+ g_PendantRomSettings.calibrationY[2] + ","
			+ g_PendantRomSettings.calibrationY[3] + ","
			+ g_PendantRomSettings.calibrationY[1]);
	}
}

// Issues feed hold, and then soft reset once Hold:0 is reached
function SafeStop(force)
{
	if (force || !IsStableIdle())
	{
		socket.emit('serialInject', '!'); // feed hold
		g_SafeStopPending = (force || laststatus.comms.runStatus == "Idle") ? 2 : 1;
	}
}

// Stops the current operation as gently as possible
function SmartStop()
{
	if (laststatus.comms.runStatus == "Jog")
	{
		socket.emit('stop', {stop: false, jog: true, abort: false});
	}
	else if (laststatus.comms.runStatus == "Run" || g_ProbeJog != undefined)
	{
		// pause, wait for hold0, then stop
		SafeStop(g_ProbeJog == -1);
	}
	else
	{
		// immediate abort in all other cases
		socket.emit('stop', {stop: false, jog: false, abort: true});
	}
}

// Returns true if value + delta is within the safe area
function IsSafeAbsoluteMove(value, delta, axis, bInches)
{
	if (!g_PendantSettings.safeLimitsEnabled)
	{
		return true;
	}

	var limits = GetSafeLimits(axis);
	var min = limits.min;
	var max = limits.max;
	if (min >= max) return false; // bad range
	if (bInches)
	{
		min /= 25.4;
		max /= 25.4;
	}
	return delta > 0 ? (value + delta <= max) : (value + delta >= min);
}

// Clamps value + delta to keep it within the safe range for the axis
function ClampSafeAbsoluteMove(value, delta, axis, bInches)
{
	if (!g_PendantSettings.safeLimitsEnabled)
	{
		return value + delta;
	}

	var limits = GetSafeLimits(axis);
	var min = limits.min;
	var max = limits.max;
	if (min >= max) return value; // bad range
	if (bInches)
	{
		min /= 25.4;
		max /= 25.4;
	}
	if (delta > 0)
	{
		return Math.min(value + delta, max);
	}
	else if (delta < 0)
	{
		return Math.max(value + delta, min);
	}

	return value;
}

// Jogging with the wheel
var g_JogAxis; // 'X', 'Y' or 'Z'
var g_JogStep;
var g_bJogInches;
var g_JogWLocation; // the last submitted target for g_JogAxis
var g_JogWQueue; // number of queued steps along g_JogAxis
var g_JogWTimer;
var g_LastWheelMoveTime;
var g_ErrorListeners;

// Jogging with joystick
var g_JogXYLocation; // last known joystick jog location in MCS. always in mm
var g_JogXYTimer;
var g_JogJoystick = {X: 0, Y: 0}; // last received joystick position
var g_JogCancelPendingTime = undefined; // to cancel jog on next job complete
var g_bAdvancedJogXY = false; // set to true if the "ok" event is supported. it makes the jogging a bit smoother and more responsive
var g_JogXYState = undefined; // true - running, false - stopping, undefined - inactive
var g_JogXYTime;

// Temporary replacement for the default 'toastError' listener, which ignores Error 8 during jogging.
function HandleJogWError(data)
{
	if (data.startsWith("error: 8"))
	{
		socket.emit('clearAlarm', 0);
		socket.emit('serialInject', "?");
		printLog("ERROR 8 HANDLED");
	}
	else if (g_ErrorListeners != undefined)
	{
		for (var i = 0; i < g_ErrorListeners.length; i++)
		{
			g_ErrorListeners[i](data);
		}
	}
}

// Begins jogging with the wheel
function BeginJogW()
{
	var axisL = g_JogAxis.toLowerCase();
	g_LastBusyTime = Date.now(); // force state as busy
	g_JogWLocation = laststatus.machine.position.work[axisL] + laststatus.machine.position.offset[axisL];

	// HACK: There is a bug in Grbl that sometimes a $J command gets into a Run state instead of Jog. The next $J
	// will trigger Error 8, because $J is not allowed during the Run state. Here we replace the entire list of
	// 'toastError' listeners with our own function, which ignores the error. The list will be restored in EndJogW.
	g_ErrorListeners = Array.from(socket.listeners('toastError'));
	socket.off('toastError');
	socket.on('toastError', HandleJogWError);
}

// Cleanup after jogging with the wheel
function EndJogW()
{
	if (g_JogWTimer != undefined)
	{
		clearInterval(g_JogWTimer);
		g_JogWTimer = undefined;
	}
	g_JogAxis = undefined;
	g_bJogInches = undefined;
	g_JogWLocation = undefined;
	g_JogWQueue = undefined;

	if (g_ErrorListeners != undefined)
	{
		socket.off('toastError');
		for (var i = 0; i < g_ErrorListeners.length; i++)
		{
			socket.on('toastError', g_ErrorListeners[i]);
		}
		g_ErrorListeners = undefined;
	}
}

// Called periodically to process the queue of wheel jog requests
function UpdateJogW()
{
	if (g_JogWQueue == 0 && IsStableIdle())
	{
		EndJogW();
		return;
	}

	if (Date.now() - g_LastWheelMoveTime > WHEEL_JOG_STOP_TIME)
	{
		// wheel stopped, clear the queue
		g_JogWQueue = 0;
		return;
	}

	var step = Math.abs(g_JogStep) * (g_bJogInches ? 25.4 : 1); // jog step in mm
	var feedT;
	switch (g_JogAxis)
	{
		case "X": feedT = grblParams["$110"]; break;
		case "Y": feedT = grblParams["$111"]; break;
		case "Z": feedT = grblParams["$112"]; break;
	}
	var feed = Math.min(feedT, step * 6000); // limit feed to 100 steps per second

	var maxDist = feedT * WHEEL_JOG_AHEAD_TIME / 60000; // travel distance to keep in the queue

	var axisL = g_JogAxis.toLowerCase();
	var current = laststatus.machine.position.work[axisL] + laststatus.machine.position.offset[axisL]; // current absolute position
	var dist = Math.abs(g_JogWLocation - current);

	// determine how many steps from the queue can be processed in WHEEL_JOG_AHEAD_TIME
	var count = 0;
	for (; count < g_JogWQueue; count++)
	{
		if (dist > 0.01 && dist + step > maxDist)
		{
			break; // have enough for WHEEL_JOG_AHEAD_TIME
		}
		dist += step;
	}

	if (count > 0)
	{
		// determine the target location
		var absValue = g_JogWLocation;
		if (g_bJogInches) { absValue /= 25.4; }

		var delta = 0;
		for (var i = 0; i < count; i++)
		{
			if (IsSafeAbsoluteMove(absValue, delta + g_JogStep, g_JogAxis, g_bJogInches))
			{
				delta += g_JogStep;
			}
			else
			{
				break;
			}
		}
		g_JogWQueue -= count;

		if (delta != 0)
		{
			absValue += delta;
			// absolute move in machine space, inches or mm
			var gcode = "$J=G90 G53 " + (g_bJogInches ? "G20 " : "G21 ") + g_JogAxis + absValue.toFixed(3) + " F" + feed.toFixed(0);
			sendGcode(gcode);
			g_JogWLocation = g_bJogInches ? absValue * 25.4 : absValue;
		}
	}
}

// Begins jogging with the joystick
function BeginJogXY()
{
	g_JogXYLocation =
	{
		X: laststatus.machine.position.work.x + laststatus.machine.position.offset.x,
		Y: laststatus.machine.position.work.y + laststatus.machine.position.offset.y,
	};

	g_bAdvancedJogXY = g_bOkEventSupported;
	if (g_bAdvancedJogXY)
	{
		g_JogXYTime = Date.now() - 200;
		g_JogXYState = true;
		UpdateJogXY();
	}
	else
	{
		// if the "ok" event is not supported, pre-fill the queue with tiny jogs to keep Grbl busy, as we don't quite
		// know when a command is processed and have to keep pushing jogs into the queue
		for (var i = 0; i < 20; i++)
		{
			var speed = (i+5)/25; // gradually increase the speed
			QueueJogXY(g_JogJoystick.X*speed, g_JogJoystick.Y*speed, 0.02);
		}
	}
}

// Queues a jog increment. dt - time duration in seconds
// Returns true if a command was queued
function QueueJogXY(joyX, joyY, dt)
{
	if (joyX == 0 && joyY == 0)
	{
		return false;
	}
	var feedX = grblParams["$110"] / 60; // mm/sec
	var feedY = grblParams["$111"] / 60; // mm/sec
	// requested move in mm
	var newX = ClampSafeAbsoluteMove(g_JogXYLocation.X, joyX * feedX * dt / JOYSTICK_STEPS, 'X', false);
	var newY = ClampSafeAbsoluteMove(g_JogXYLocation.Y, joyY * feedY * dt / JOYSTICK_STEPS, 'Y', false);
	var dx = newX - g_JogXYLocation.X;
	var dy = newY - g_JogXYLocation.Y;
	if (dx == 0 && dy == 0)
	{
		return false;
	}

	var feed = (Math.sqrt(dx * dx + dy * dy) / dt) * 60;
	// absolute machine move, mm
	var gcode = "$J=G53 G90 G21" + " X" + newX.toFixed(3) + " Y" + newY.toFixed(3) + " F" + feed.toFixed(0);
	sendGcode(gcode);
	g_JogXYLocation.X = newX;
	g_JogXYLocation.Y = newY;
	return true;
}

// Updates the advanced jog state after "ok" event is received
function UpdateJogXY()
{
	if (laststatus.comms.runStatus != "Idle" && laststatus.comms.runStatus != "Jog" && laststatus.comms.runStatus != "Running")
	{
		g_JogXYState = false;
	}
	if (g_JogXYState == true)
	{
		var t = Date.now();
		var dt = (t - g_JogXYTime) / 1000;
		g_JogXYTime = t;

		if (!QueueJogXY(g_JogJoystick.X, g_JogJoystick.Y, dt))
		{
			g_JogXYState = undefined;
		}
	}
	else if (g_JogXYState == false)
	{
		socket.emit('stop', {stop: false, jog: true, abort: false});
		g_JogXYState = undefined;
	}
}

// Handles the jog commands from the pendant
function HandleJogCommand(command)
{
	if (COM_LOG_LEVEL >= 2) { console.log("#JOG#", command); }
	// 0LX - go to work zero on X
	// RIGX0.010 - round X to 0.010 inches in global space (must be idle)
	// WMX2*0.10 - wheel X by 2*0.10mm (must be idle or jogging)
	// JXY-2,3 - joystick is at -2,3

	if (command[0] == '0')
	{
		// go to zero
		if (laststatus.comms.runStatus != "Idle") { return; }
		var space = command[1]; // L or G
		if (space != 'L' && space != 'G') { return; }
		var work = space == 'L';

		var axis = command[2]; // X/Y/Z
		if (axis < 'X' || axis > 'Z') { return; }
		var axisL = axis.toLowerCase();

		var pos = laststatus.machine.position;
		var globalValue = pos.work[axisL] + pos.offset[axisL];

		var newValue;
		if (work)
		{
			newValue = ClampSafeAbsoluteMove(globalValue, -pos.work[axisL], axis, false) - pos.offset[axisL];
		}
		else
		{
			newValue = ClampSafeAbsoluteMove(globalValue, -globalValue, axis, false);
		}

		var feedT;
		switch (axis)
		{
			case "X": feedT = grblParams["$110"]; break;
			case "Y": feedT = grblParams["$111"]; break;
			case "Z": feedT = grblParams["$112"]; break;
		}

		// absolute move, work or machine, mm
		// use max speed allowed by the axis
		var gcode = "$J=G90 G21 " + (work ? "" : "G53 ") + axis + newValue.toFixed(3) + " F" + feedT;
		sendGcode(gcode);
		return;
	}

	if (command[0] == 'A')
	{
		// align to nearest
		if (laststatus.comms.runStatus != "Idle") { return; }

		var units = command[1]; // M or I
		if (units != 'I' && units != 'M') { return; }
		var bInches = units == 'I';

		var space = command[2]; // L or G
		if (space != 'L' && space != 'G') { return; }
		var work = space == 'L';

		var axis = command[3]; // X/Y/Z
		if (axis < 'X' || axis > 'Z') { return; }
		var axisL = axis.toLowerCase();

		var step = Number(command.substring(4));

		var pos = laststatus.machine.position;
		var globalValue = pos.work[axisL] + pos.offset[axisL];
		if (bInches) { globalValue /= 25.4; }

		var oldValue = globalValue;
		if (work)
		{
			oldValue -= bInches ? (pos.offset[axisL] / 25.4) : pos.offset[axisL];
		}

		var roundedValue = Math.round(oldValue / step) * step;
		if (!IsSafeAbsoluteMove(globalValue, roundedValue - oldValue, axis, bInches))
		{
			// rounded position not safe, try rounding in the other direction
			roundedValue = (roundedValue  < oldValue ? Math.ceil(oldValue / step) : Math.floor(oldValue / step)) * step;
			if (!IsSafeAbsoluteMove(globalValue, roundedValue - oldValue, axis, bInches))
			{
				return;
			}
		}

		var feedT;
		switch (axis)
		{
			case "X": feedT = grblParams["$110"]; break;
			case "Y": feedT = grblParams["$111"]; break;
			case "Z": feedT = grblParams["$112"]; break;
		}

		// absolute move, work or machine, inches or mm
		var gcode = "$J=G90 " + (work ? "" : "G53 ") + (bInches ? "G20 " : "G21 ") + axis + roundedValue.toFixed(3) + " F" + feedT;
		sendGcode(gcode);
		return;
	}

	if (command[0] == 'W')
	{
		// hand wheel
		var units = command[1]; // M or I
		if (units != 'I' && units != 'M') { return; }
		var inches = units == 'I';

		var axis = command[2]; // X/Y/Z
		if (axis < 'X' || axis > 'Z') { return; }

		var mul = command.indexOf('*');
		var count = Number(command.substring(3, mul));
		var step = Number(command.substring(mul + 1));
		if (count < 0)
		{
			count = -count;
			step = -step;
		}

		if (axis != g_JogAxis || inches != g_bJogInches)
		{
			// axis or unit is switched, clear everything
			EndJogW();
			g_JogWQueue = 0;
			g_JogStep = step;
			g_JogAxis = axis;
			g_bJogInches = inches;
		}
		else if (step != g_JogStep)
		{
			// only the step has changed, just clear the queue
			g_JogWQueue = 0;
			g_JogStep = step;
		}

		g_JogWQueue += count;

		g_LastWheelMoveTime = Date.now();

		if (g_JogWTimer == undefined)
		{
			g_JogWLocation = undefined;
			if (IsStableIdle())
			{
				BeginJogW();
				UpdateJogW();
			}
			g_JogWTimer = setInterval(function()
			{
				if (g_JogWLocation == undefined && IsStableIdle())
				{
					BeginJogW();
				}
				if (g_JogWLocation != undefined)
				{
					UpdateJogW();
				}
			}, 100);
		}

		return;
	}

	if (command.startsWith("JXY"))
	{
		// joystick input
		var xy = command.substring(3).split(',');
		g_JogJoystick = {X: xy[0], Y: xy[1]};

		if (xy[0] == 0 && xy[1] == 0)
		{
			// joystick is released, stop immediately
			if (g_bAdvancedJogXY)
			{
				// if "ok" is supported, set the state to false, which will issue a cancel command on the next "ok" event
				if (g_JogXYState == true)
				{
					g_JogXYState = false;
				}
				if (g_JogXYTimer != undefined)
				{
					clearInterval(g_JogXYTimer);
					g_JogXYTimer = undefined;
				}
			}
			else
			{
				// otherwise stop immediately, and queue up a second stop when the job is completed, just in case
				if (g_JogXYLocation != undefined && !IsStableIdle())
				{
					socket.emit('stop', {stop: false, jog: true, abort: false});
					if (laststatus.comms.runStatus != "Idle")
					{
						g_JogCancelPendingTime = Date.now();
					}
				}
				g_JogXYLocation = undefined;
				if (g_JogXYTimer != undefined)
				{
					clearInterval(g_JogXYTimer);
					g_JogXYTimer = undefined;
				}
			}
			return;
		}

		if (g_JogXYTimer == undefined)
		{
			if (g_bOkEventSupported)
			{
				// if "ok" is supported, just begin the jog on the next stable idle. the "ok" event handler will keep the jogs coming
				if (g_JogXYState == undefined)
				{
					if (IsStableIdle())
					{
						BeginJogXY();
					}
					else
					{
						g_JogXYTimer = setInterval(function()
						{
							if (IsStableIdle())
							{
								BeginJogXY();
								clearInterval(g_JogXYTimer);
								g_JogXYTimer = undefined;
							}
						}, 100);
					}
				}
			}
			else
			{
				// otherwise start a persistent interval that will keep queueing jogs ever 100ms
				if (IsStableIdle()/* && (xy[0] * xy[0] + xy[1] * xy[1]) * 4 > JOYSTICK_STEPS * JOYSTICK_STEPS*/)
				{
					BeginJogXY();
				}
				g_JogXYTime = g_JogXYLocation == undefined ? undefined : Date.now();
				g_JogXYTimer = setInterval(function()
				{
					if (g_JogXYLocation == undefined)
					{
						if (laststatus.comms.runStatus == "Idle")
						{
							BeginJogXY();
							g_JogXYTime = Date.now();
						}
					}
					else
					{
						var t = Date.now();
						QueueJogXY(g_JogJoystick.X, g_JogJoystick.Y, (t - g_JogXYTime) / 1000);
						g_JogXYTime = t;
					}
				}, 100);
			}
		}
		return;
	}
}

// Shows the job menu
function HandleJobMenu()
{
	if ($('#runBtn').attr('disabled'))
	{
		ShowPendantDialog({
			text: ["No job is selected"],
			rButton: ["Back"],
		});
		return;
	}

	var OnEnterJobMenu = $('#pendant').prop('OnEnterJobMenu');
	if (OnEnterJobMenu != undefined)
	{
		OnEnterJobMenu(HandleJobMenuPart2);
	}
	else
	{
		HandleJobMenuPart2();
	}
}

function HandleJobMenuPart2()
{
	if (g_PendantSettings.jobChecklist && g_PendantSettings.jobChecklist.length > 0)
	{
		var text = g_PendantSettings.jobChecklist.split('|').map(x => CHAR_UNCHECKED + " " + x.substring(0, 16)).slice(0, 3);
		ShowPendantDialog({
			title: "Job Checklist",
			text: text,
			lButton: ["!@", function () { WritePort("JOBSCREEN"); }],
			rButton: ["Back"],
		});
	}
	else
	{
		WritePort("JOBSCREEN");
	}
}

// Handles the job commands
function HandleJobCommand(command)
{
	if (command == "START")
	{
		if (!$('#runBtn').attr('disabled'))
		{
			runJobFile()
		}
	}
	else if (command == "PAUSE")
	{
		socket.emit('pause');
		if (g_PendantSettings.pauseSpindle && laststatus.machine.overrides.realSpindle > 0)
		{
			socket.emit('serialInject', String.fromCharCode(0x84));
		}
	}
	else if (command == "RPM0")
	{
		socket.emit('serialInject', String.fromCharCode(0x84));
	}
	else if (command == "RESUME")
	{
		socket.emit('resume');
	}
	else if (command == "STOP")
	{
		SafeStop(true);
	}
}

function SetupProbeMode(probeType)
{
	// validate probe settings
	var settings  = probeType == 0 ? g_PendantSettings.zProbe : g_PendantSettings.toolProbe;
	var zProbeThickness = g_PendantSettings.zProbe.thickness != undefined ? g_PendantSettings.zProbe.thickness : localStorage.getItem('z0platethickness');
	if ((probeType != 0 || zProbeThickness >= 0) &&
		settings.seek1.travel > 0 && settings.seek1.feed &&
		settings.retract1.travel > 0 && settings.retract1.feed)
	{
		if (settings.style == "single")
		{
			return;
		}
		if (settings.seek2.travel > 0 && settings.seek2.feed &&
			settings.retract2.travel > 0 && settings.retract2.feed > 0)
		{
			return;
		}
	}
	ShowPendantDialog({
		title: "Probe",
		text: [
			"The probe setings",
			"are invalid."],
		rButton: ["OK"],
	});
}

// Handles a successful probe job
function HandleProbeJobComplete(probeType, probeZ)
{
	var showZ = false;
	if (probeType == 0 && g_PendantSettings.zProbe.useProbeResult)
	{
		// z-probe
		var zProbeThickness = g_PendantSettings.zProbe.thickness != undefined ? g_PendantSettings.zProbe.thickness : localStorage.getItem('z0platethickness');
		var gcode = "G10 P0 L2 Z" + (probeZ - zProbeThickness).toFixed(3);
		sendGcode(gcode);
		showZ = true;
	}
	else if (probeType == 1)
	{
		// reference tool - just remember the current Z
		g_TloRefZ = probeZ;
		showZ = true;
	}
	else if (probeType == 2)
	{
		// new tool - adjust work Z
		var delta = probeZ - g_TloRefZ;
		g_TloRefZ = probeZ;
		var gcode = "G10 P0 L2 Z" + (laststatus.machine.position.offset.z + delta).toFixed(3);
		sendGcode(gcode);
		showZ = true;
		if (window.LastWcs != undefined && typeof(window.LastWcs.Z) == "number")
		{
			window.LastWcs.Z += delta;
		}
	}

	var text = [];
	if (showZ)
	{
		text.push("^ Z = " + probeZ + " mm");
	}
	text.push("Disconnect probe");

	ShowPendantDialog({
		title: "Probe Complete",
		text: text,
		rButton: ["OK", function()
		{
			if (Metro.dialog.isOpen('#completeMsgModal'))
			{
				Metro.dialog.close('#completeMsgModal');
			}
		}],
	});
}

// Handles the Z probe commands
function HandleProbeCommand(command)
{
	if (command.startsWith("ENTER"))
	{
		var probeType = Number(command[5]);
		g_ProbeZDownFeed = Math.min(Math.max((probeType == 1 || probeType == 2) ? g_PendantSettings.toolProbe.downSpeed : g_PendantSettings.zProbe.downSpeed, 10), 100);
		if (probeType == 0 && g_PendantSettings.zProbe.style == "default")
		{
			if (!Metro.dialog.isOpen('#xyzProbeWindow'))
			{
				openProbeDialog();
				return;
			}
		}
		else
		{
			if (probeType == 1 && !laststatus.machine.modals.homedRecently)
			{
				ShowPendantDialog({
					title: "Tool Probe",
					text: [
						"Machine has to",
						"be homed before",
						"using tool probe."],
						lButton: ["!IGNORE", function() { WritePort("PROBESCREEN:" + probeType); SetupProbeMode(probeType); }],
						rButton: ["OK"],
					});
				return;
			}
			SetupProbeMode(probeType);
			return;
		}
	}
	else if (command == "CONNECT")
	{
		if (g_PendantSettings.zProbe.style == "default" && Metro.dialog.isOpen('#xyzProbeWindow'))
		{
			confirmProbeInPlace();
		}
		return;
	}
	else if (command == "CANCEL")
	{
		if (g_PendantSettings.zProbe.style == "default" && Metro.dialog.isOpen('#xyzProbeWindow'))
		{
			resetJogModeAfterProbe();
			Metro.dialog.close('#xyzProbeWindow');
		}
		return;
	}
	else if (command == "Z+")
	{
		if (laststatus.comms.runStatus != "Idle")
		{
			return;
		}
		if (g_PendantSettings.safeLimitsEnabled)
		{
			var limits = GetSafeLimits('Z');
			if (laststatus.machine.position.work.z + laststatus.machine.position.offset.z < limits.max)
			{
				gcode = "$J=G53 G21 G90 Z" + limits.max.toFixed(3) + " F" + grblParams["$112"];
				sendGcode(gcode);
			}
		}
		else
		{
			var gcode = "$J=G21 G91 Z100" + " F" + grblParams["$112"];
			sendGcode(gcode);
		}
		g_ProbeJog = 1;
	}
	else if (command == "Z-")
	{
		if (laststatus.comms.runStatus != "Idle" || laststatus.machine.inputs.includes('P'))
		{
			// not idle or the probe is touching. can't go any lower
			return;
		}
		var feed = grblParams["$112"] * g_ProbeZDownFeed / 100;
		var gcode;
		if (g_PendantSettings.safeLimitsEnabled)
		{
			var limits = GetSafeLimits('Z');
			if (laststatus.machine.position.work.z + laststatus.machine.position.offset.z > limits.min)
			{
				if (SAFE_PROBE_JOG)
				{
					gcode = "G38.3 G21 G90 Z" + (limits.min - laststatus.machine.position.offset.z).toFixed(3) + " F" + feed.toFixed(0);
				}
				else
				{
					gcode = "$J=G53 G21 G90 Z" + limits.min.toFixed(3) + " F" + feed.toFixed(0);
				}
			}
		}
		else
		{
			if (SAFE_PROBE_JOG)
			{
				gcode = "G38.3 G21 G91 Z-100" + " F" + feed.toFixed(0);
			}
			else
			{
				gcode = "$J=G21 G91 Z-100" + " F" + feed.toFixed(0);
			}
		}
		sendGcode(gcode);
		g_ProbeJog = -1;
		if (SAFE_PROBE_JOG)
		{
			socket.off('prbResult');
			socket.on('prbResult', function (probe)
			{
				// clear the probe jog if the probe is touched
				g_ProbeJog = undefined;
				socket.off('prbResult');
			});
		}
	}
	else if (command == "Z=")
	{
		if (g_ProbeJog != undefined)
		{
			if (SAFE_PROBE_JOG)
			{
				SmartStop();
			}
			else
			{
				socket.emit('stop', {stop: false, jog: true, abort: false});
			}
			g_ProbeJog = undefined;
		}
	}
	else if (command.startsWith("START"))
	{
		var probeType = Number(command[5]);
		if (probeType == 0 && g_PendantSettings.zProbe.style == "default")
		{
			if (Metro.dialog.isOpen('#xyzProbeWindow'))
			{
				runProbeNew();
				Metro.dialog.close('#xyzProbeWindow');
				return;
			}
		}
		else if (laststatus.comms.runStatus == "Idle")
		{
			// initiate seek 1
			var settings = probeType == 0 ? g_PendantSettings.zProbe : g_PendantSettings.toolProbe;
			var feed = Math.max(Math.min(settings.seek1.feed, grblParams["$112"]), 10);
			var gcode = "G38.3 G21 G91 Z-" + settings.seek1.travel.toFixed(3) + " F" + feed.toFixed(0);
			g_LastProbeZ = undefined;
			socket.off('prbResult');

			socket.emit('runJob', {
				data: gcode,
				isJob: false,
				completedMsg: "",
				fileName: ""
			});

			var pass = 1;
			socket.on('prbResult', function(probe)
			{
				if (probe.state == 0)
				{
					ShowPendantDialog({
						title: "Probe Failed",
						text: [
							"Disconnect probe"],
							rButton: ["OK"],
						});
					socket.off('prbResult');
					return;
				}

				if (pass == 1)
				{
					// retract 1
					pass = 2;
					var gcode = "G4 P0.4"; // wait a bit
					if (probeType == 0 && g_PendantSettings.zProbe.style == "single" && !g_PendantSettings.zProbe.useProbeResult)
					{
						var zProbeThickness = g_PendantSettings.zProbe.thickness != undefined ? g_PendantSettings.zProbe.thickness : localStorage.getItem('z0platethickness');
						gcode += "\nG10 G21 P0 L20 Z" + Number(zProbeThickness).toFixed(3);
					}
					var maxZ = GetSafeLimits('Z').max;
					var travel = settings.retract1.travel;
					var feed = Math.max(Math.min(settings.retract1.feed, grblParams["$112"]), 10);
					if (settings.style == "single" && g_PendantSettings.safeLimitsEnabled && travel > maxZ - probe.z)
					{
						if (probe.z < maxZ)
						{
							gcode += "\n$J=G21 G53 G90 Z" + maxZ.toFixed(3) + " F" + feed.toFixed(0);
						}
					}
					else if (travel > 0)
					{
						gcode += "\n$J=G21 G91 Z" + travel.toFixed(3) + " F" + feed.toFixed(0);
					}

					if (settings.style == "dual")
					{
						// seek 2
						feed = Math.max(Math.min(settings.seek2.feed, grblParams["$112"]), 10);
						gcode += "\nG38.3 G21 G91 Z-" + settings.seek2.travel.toFixed(3) + " F" + feed.toFixed(0);
						socket.emit('runJob', {
							data: gcode,
							isJob: false,
							completedMsg: "",
							fileName: ""
						});
					}
					else
					{
						// finished (single pass)
						g_LastProbeZ = probe.z;
						socket.emit('runJob', {
							data: gcode,
							isJob: false,
							completedMsg: "Probe Complete" + probeType,
							fileName: ""
						});
						socket.off('prbResult');
					}
				}
				else if (pass == 2)
				{
					// retract 2
					pass = 3;
					var gcode = "G4 P0.4"; // wait a bit
					if (probeType == 0 && !g_PendantSettings.zProbe.useProbeResult)
					{
						var zProbeThickness = g_PendantSettings.zProbe.thickness != undefined ? g_PendantSettings.zProbe.thickness : localStorage.getItem('z0platethickness');
						gcode += "\nG10 G21 P0 L20 Z" + Number(zProbeThickness).toFixed(3);
					}
					var maxZ = GetSafeLimits('Z').max;
					var travel = settings.retract2.travel;
					var feed = Math.max(Math.min(settings.retract2.feed, grblParams["$112"]), 10);
					if (g_PendantSettings.safeLimitsEnabled && travel > maxZ - probe.z)
					{
						if (probe.z < maxZ)
						{
							gcode += "\n$J=G21 G53 G90 Z" + maxZ.toFixed(3) + " F" + feed.toFixed(0);
						}
					}
					else if (travel > 0)
					{
						gcode += "\n$J=G21 G91 Z" + travel.toFixed(3) + " F" + feed.toFixed(0);
					}

					// finished (dual pass)
					g_LastProbeZ = probe.z;
					socket.emit('runJob', {
						data: gcode,
						isJob: false,
						completedMsg: "Probe Complete" + probeType,
						fileName: ""
					});
					socket.off('prbResult');
				}
			});
		}
	}
	else if (command == "GOTOSENSOR")
	{
		// go to tool probe
		var feedZ = grblParams["$112"];
		var feedXY = grblParams["$110"];
		var gcode = "$J=G53 G21 G90 Z" + g_PendantSettings.toolProbe.location.Z.toFixed(3) + " F" + feedZ;
		gcode += "\n$J=G53 G21 G90 X" + g_PendantSettings.toolProbe.location.X.toFixed(3) + " Y" + g_PendantSettings.toolProbe.location.Y.toFixed(3) + " F" + feedXY;
		sendGcode(gcode);
	}
}

var g_TargetFeedRate;
var g_TargetSpeedRate;
var g_FeedSpeedRateTimer;
var g_FeedSpeedStatusCounter;

// A callback to periodically update the feed and speed rate
function FeedSpeedRateUpdate()
{
	if (g_FeedSpeedStatusCounter == g_StatusCounter)
	{
		return; // no update yet
	}
	g_FeedSpeedStatusCounter = g_StatusCounter;
	if (g_TargetFeedRate == 100)
	{
		socket.emit('serialInject', String.fromCharCode(0x90)); // set to 100
		socket.emit('serialInject', "?");
		g_TargetFeedRate = undefined;
	}
	else if (g_TargetFeedRate != undefined)
	{
		var delta = g_TargetFeedRate - laststatus.machine.overrides.feedOverride;
		if (delta != 0)
		{
			if (delta >= 10)
			{
				socket.emit('serialInject', String.fromCharCode(0x91)); // increase by 10
			}
			else if (delta > 0)
			{
				socket.emit('serialInject', String.fromCharCode(0x93)); // increase by 1
			}
			else if (delta <= -10)
			{
				socket.emit('serialInject', String.fromCharCode(0x92)); // decrease by 10
			}
			else if (delta < 0)
			{
				socket.emit('serialInject', String.fromCharCode(0x94)); // decrease by 1
			}
			socket.emit('serialInject', "?");
			return;
		}
		g_TargetFeedRate = undefined;
	}

	if (g_TargetSpeedRate  == 100)
	{
		socket.emit('serialInject', String.fromCharCode(0x99)); // set to 100
		socket.emit('serialInject', "?");
		g_TargetSpeedRate  = undefined;
	}
	else if (g_TargetSpeedRate != undefined)
	{
		var delta = g_TargetSpeedRate - laststatus.machine.overrides.spindleOverride;
		if (delta != 0)
		{
			if (delta >= 10)
			{
				socket.emit('serialInject', String.fromCharCode(0x9A)); // increase by 10
			}
			else if (delta > 0)
			{
				socket.emit('serialInject', String.fromCharCode(0x9C)); // increase by 1
			}
			else if (delta <= -10)
			{
				socket.emit('serialInject', String.fromCharCode(0x9B)); // decrease by 10
			}
			else if (delta < 0)
			{
				socket.emit('serialInject', String.fromCharCode(0x9D)); // decrease by 1
			}
			socket.emit('serialInject', "?");
			return;
		}
		g_TargetSpeedRate = undefined;
	}

	clearInterval(g_FeedSpeedRateTimer);
	g_FeedSpeedRateTimer = undefined;
	g_FeedSpeedStatusCounter = undefined;
}

// Handles the communication from the pendant
function PendantComHandler(data)
{
	data = (data + "").trim();
	if (data == CHAR_ACK)
	{
		if (COM_LOG_LEVEL >= 3) { console.log("COM: ", "<ACK>"); }
		SendPortMsg();
		return;
	}

	if (COM_LOG_LEVEL >= 2 || (COM_LOG_LEVEL == 1 && data != "PING" && !data.startsWith("RAWJOY:") && !data.startsWith("JOG:W") && !data.startsWith("JOG:JXY")))
	{
		console.log("COM: ", data);
	}

	// connection
	if (data.startsWith("DANT:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#DANT#"); }
		HandleHandshake();
		return;
	}
	if (data.startsWith("NAME:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#NAME#"); }
		g_PendantRomSettings.pendantName = data.substring(5);
		return;
	}
	if (data.startsWith("CALIBRATION:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#CALIBRATION#"); }
		var cal = data.substring(12).split(',').map(Number);

		g_PendantRomSettings.calibrationX[0] = cal[0];
		g_PendantRomSettings.calibrationX[2] = cal[1];
		g_PendantRomSettings.calibrationX[3] = cal[2];
		g_PendantRomSettings.calibrationX[1] = cal[3];

		g_PendantRomSettings.calibrationY[0] = cal[4];
		g_PendantRomSettings.calibrationY[2] = cal[5];
		g_PendantRomSettings.calibrationY[3] = cal[6];
		g_PendantRomSettings.calibrationY[1] = cal[7];

		return;
	}

	// heartbeat
	if (data == "PING")
	{
		WritePort("PONG");
		return;
	}

	// dismiss current alarm
	if (data == "DISMISS")
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#DISMISS#"); }
		socket.emit('clearAlarm', 2);
		$('.closeAlarmBtn').click();
		$('.closeErrorBtn').click();
		return;
	}

	if (data.startsWith("DIALOG:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#DIALOG#"); }
		var dlg = data.substring(7).split(',').map(Number);
		if (dlg[0] == g_DialogId)
		{
			g_DialogId = 0;
			if (dlg[1] == 1 && g_DialogLCallback)
			{
				g_DialogLCallback();
			}
			else if (dlg[1] == 2 && g_DialogRCallback)
			{
				g_DialogRCallback();
			}
		}
		return;
	}

	// joystick position for calibration
	if (data.startsWith("RAWJOY:"))
	{
		var xy = data.substring(7).split(',').map(Number);
		g_RawJoyX = xy[0];
		g_RawJoyY = xy[1];
		UpdateCalibration();
		return;
	}

	// calibration input from the pendant
	if (data.startsWith("CAL:"))
	{
		data = data.substring(4);
		if (data == "CANCEL")
		{
			CancelCalibration(true);
		}
		else
		{
			SetCalibrationStage(Number(data) + 1);
		}
		return;
	}

	////////////////////// ACTIONS //////////////////////

	if (data == "STOP")
	{
		SmartStop();
		return;
	}

	if (data == "ABORT")
	{
		socket.emit('stop', {stop: false, jog: false, abort: true});
		return;
	}

	if (data.startsWith("JOG:"))
	{
		HandleJogCommand(data.substring(4));
		return;
	}

	if (data.startsWith("SET0:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#SET0#"); }
		if (data.length >= 6 && data[5] >= 'X' && data[5] <= 'Z')
		{
			sendGcode("G10 P0 L20 " + data[5] + "0");
		}
		return;
	}

	// initiate homing
	if (data == "HOME")
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#HOME#"); }
		if (laststatus.machine.modals.homedRecently)
		{
			ShowPendantDialog({
				title: "Home XYZ",
				text: [
					"Machine is already",
					"homed.",
					"Home anyway?"],
				lButton: ["No"],
				rButton: ["Yes", home],
			});
			return;
		}
		home();
		return;
	}

	if (data == "JOBMENU")
	{
		HandleJobMenu();
		return;
	}

	if (data.startsWith("JOB:"))
	{
		HandleJobCommand(data.substring(4));
		return;
	}

	if (data.startsWith("FEED:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#FEED#"); }
		var val = Number(data.substring(5)) * WHEEL_OVERRIDE_STEP;
		if (val == 0)
		{
			g_TargetFeedRate = 100;
		}
		else
		{
			var current = g_TargetFeedRate != undefined ? g_TargetFeedRate : laststatus.machine.overrides.feedOverride;
			if (val > 0)
			{
				val = Math.ceil((val + current) / WHEEL_OVERRIDE_STEP) * WHEEL_OVERRIDE_STEP;
			}
			else
			{
				val = Math.floor((val + current) / WHEEL_OVERRIDE_STEP) * WHEEL_OVERRIDE_STEP;
			}
			val = Math.min(Math.max(val, 10), 200);
			g_TargetFeedRate = val;
		}
		if (g_FeedSpeedRateTimer == undefined)
		{
			FeedSpeedRateUpdate();
			g_FeedSpeedRateTimer = setInterval(FeedSpeedRateUpdate, 50);
		}
		return;
	}

	if (data.startsWith("SPEED:"))
	{
		if (COM_LOG_LEVEL >= 1) { console.log("#SPEED#"); }
		var val = Number(data.substring(6)) * WHEEL_OVERRIDE_STEP;
		if (val == 0)
		{
			g_TargetSpeedRate = 100;
		}
		else
		{
			var current = g_TargetSpeedRate != undefined ? g_TargetSpeedRate : laststatus.machine.overrides.spindleOverride;
			if (val > 0)
			{
				val = Math.ceil((val + current) / WHEEL_OVERRIDE_STEP) * WHEEL_OVERRIDE_STEP;
			}
			else
			{
				val = Math.floor((val + current) / WHEEL_OVERRIDE_STEP) * WHEEL_OVERRIDE_STEP;
			}
			val = Math.min(Math.max(val, 10), 200);
			g_TargetSpeedRate = val;
		}
		if (g_FeedSpeedRateTimer == undefined)
		{
			FeedSpeedRateUpdate();
			g_FeedSpeedRateTimer = setInterval(FeedSpeedRateUpdate, 50);
		}
		return;
	}

	if (data.startsWith("PROBE:"))
	{
		HandleProbeCommand(data.substring(6));
		return;
	}

	if (data.startsWith("RUNMACRO:"))
	{
		var idx = Number(data.substring(9));
		if (COM_LOG_LEVEL >= 1) { console.log("#RUNMACRO#", idx); }
		if (idx >= 1 && idx <= 7)
		{
			ExecuteMacro(g_PendantSettings["macro" + idx].name);
		}
		return;
	}
}

var g_SerialQueue = []; // queued up messages to send to the port
var g_bSerialPending = false;

// Sends the next message in the queue to the port
function SendPortMsg()
{
	if (g_SerialQueue.length == 0)
	{
		g_bSerialPending = false;
		return;
	}

	var text = g_SerialQueue[0];
	g_SerialQueue.splice(0, 1);
	g_PendantPort.write(text);
	g_bSerialPending = true;
	if (text != "PONG\n")
	{
		if (COM_LOG_LEVEL >= 2) { console.log("SEND: ", text.trim()); }
	}
}

// Sends a string to the pendant
function WritePort(text)
{
	if (g_PendantPort)
	{
		if (g_SerialQueue.length >= 10)
		{
			return; // only queue up to 10 messages. the rest are dropped
		}
		while (true)
		{
			if (text.length < MAX_MESSAGE_LENGTH)
			{
				g_SerialQueue.push(text + "\n");
				break;
			}
			g_SerialQueue.push(text.substring(0, MAX_MESSAGE_LENGTH - 1) + CHAR_ACK);
			text = text.substring(MAX_MESSAGE_LENGTH - 1);
		}
		if (!g_bSerialPending)
		{
			SendPortMsg();
		}
	}
}

// Executes a CONTROL macro by name (either G-code or Javascript)
function ExecuteMacro(macroName)
{
	for (var i = 0; i < buttonsarray.length; i++)
	{
		var button = buttonsarray[i];
		if (button.title.replaceAll('{', '_').replaceAll('}', '_') == macroName)
		{
			if (button.codetype == "gcode")
			{
				sendGcode(button.gcode);
			}
			else if (button.codetype == "javascript" && !button.jsrunonstartup)
			{
				runJsMacro(i);
			}
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Connect/Disconnect

var g_ComPorts;
var g_CurrentTryPort;
var g_CurrentTryParser;
var g_CurrentTryTimer;

// Handles the initial handshake response from the pendant
function TryComHandler(data)
{
	if (data.startsWith("DANT:"))
	{
		var html = EscapeHtml(g_CurrentTryPort.path);
		var ver = data.substring(5);
		if (ver != PENDANT_VERSION)
		{
			printLog("<span class='fg-darkRed'>[ pendant ] </span><span class='fg-blue'>Found pendant on port " + html + ", but it is version " + ver + " instead of " + PENDANT_VERSION + "</span>")
		}
		else
		{
			// handshake received, complete the connection
			clearTimeout(g_CurrentTryTimer);
			g_PendantPort = g_CurrentTryPort;
			printLog("<span class='fg-darkRed'>[ pendant ] </span><span class='fg-blue'>Connected on port " + html + "</span>")
			$('#pendant > span.icon > span > svg > path').attr("fill", "currentColor");
			$('#DisconnectPendant').removeClass("disabled");

			g_CurrentTryParser.off('data', TryComHandler);
			g_CurrentTryParser.on('data', PendantComHandler);
			g_PendantPort.on('close', DisconnectPendant);
			socket.on('status', HandleStatus);
			socket.on('queueCount', HandleQueueStatus);
			socket._callbacks["$jobComplete"].splice(0,0, HandleJobComplete);
			socket.on('ok', HandleOk);

			g_CurrentTryPort = undefined;
			g_CurrentTryParser = undefined;
			g_CurrentTryTimer = undefined;
			g_ComPorts = undefined;
			g_StatusCounter = 0;

			localStorage.setItem("PendantPort", g_PendantPort.path);
			HandleHandshake();
		}
	}
}

// Cleans up the current connection attempt
function CloseCurrentPort()
{
	if (g_CurrentTryPort)
	{
		if (COM_LOG_LEVEL >= 1) { console.log("No response from port ", g_CurrentTryPort.path); }
		g_CurrentTryPort.drain(g_CurrentTryPort.close());
		g_CurrentTryPort = undefined;
		if (g_CurrentTryParser)
		{
			g_CurrentTryParser.off('data', TryComHandler);
		}
		g_CurrentTryParser = undefined;
		g_CurrentTryTimer = undefined;
	}
}

// Tries to communicate with the pendant using the current port
function TryConnectPort()
{
	g_CurrentTryPort.write("PEN\n");
	g_CurrentTryTimer = setTimeout(function()
	{
		CloseCurrentPort();
		TryNextPort();
	}, 1000);
}

// Attempts to connect to the next port in the list
function TryNextPort()
{
	if (g_ComPorts.length == 0)
	{
		g_ComPorts = undefined;
		printLog("<span class='fg-darkRed'>[ pendant ] </span><span class='fg-blue'>Failed to find a pendant</span>")
		$('#ConnectPendant').removeClass("disabled");
	}
	else
	{
		var path = g_ComPorts[0];
		g_ComPorts.splice(0, 1);

		g_CurrentTryPort = new SerialPort({path: path, baudRate: PENDANT_BAUD_RATE});
		g_CurrentTryPort.on('error', function(err) { console.log("Port ", path, " Error: ", err.message)} )
		g_CurrentTryParser = g_CurrentTryPort.pipe(new ReadlineParser({delimiter: '\r\n'}));
		g_CurrentTryParser.on('data', TryComHandler);
		if (ARDUINO_RESETS_ON_CONNECT)
		{
			// normally the Arduino resets when the serial port is connected. wait 2 seconds for it to finish restarting
			g_CurrentTryTimer = setTimeout(function()
			{
				if (g_CurrentTryPort)
				{
					TryConnectPort();
				}
			}, 2000);
		}
		else
		{
			// if the Arduino is hacked to not reset, simply try to connect right away
			TryConnectPort();
		}
	}
}

// Initiates the connection to the pendant
window.ConnectPendant = function()
{
	if (!g_PendantPort)
	{
		$('#ConnectPendant').addClass("disabled");
		$('#DisconnectPendant').addClass("disabled");
		printLog("<span class='fg-darkRed'>[ pendant ] </span><span class='fg-blue'>Looking for pendant...</span>")
		SerialPort.list().then(function(ports)
			{
				g_ComPorts = ports.map(x => x.path);
				var lastPort = localStorage.getItem("PendantPort");
				if (lastPort)
				{
					// sort so the lastPort is first in the list
					g_ComPorts.sort((a, b) => { if (a == lastPort) { return -1; } if (b == lastPort) { return 1; } return 0; });
				}
				TryNextPort();
			});
	}
}

// Disconnects the pendant and cloes the port
window.DisconnectPendant = function()
{
	if (g_PendantPort)
	{
		WritePort("BYE");
		g_PendantPort.drain(g_PendantPort.close());
		g_PendantPort = undefined;
		g_SerialQueue = [];
		g_bSerialPending = false;
		$('#ConnectPendant').removeClass("disabled");
	}
	$('#pendant > span.icon > span > svg > path').attr("fill", "silver");
	$('#DisconnectPendant').addClass("disabled");
	socket.off('status', HandleStatus);
	socket.off('queueCount', HandleQueueStatus);
	socket.off('jobComplete', HandleJobComplete);
	socket.off('ok', HandleOk);
	socket.off('prbResult');
}

///////////////////////////////////////////////////////////////////////////////
// Calibration

// timer to refresh the calibration graphics
var g_CalibrationTimer;

// last raw joystick values [0..1023]
var g_RawJoyX;
var g_RawJoyY;

// current calibration settings in the dialog input fields
var g_JoySetX;
var g_JoySetY;

// current live calibration settings
var g_JoyCalX;
var g_JoyCalY;

// current calibration stage: 0 - detect range, 1+ - detect center
var g_CalibrationStage;

// Returns the calibration settings from the dialog for a specified axis (either 'X' or 'Y')
function ParseJoystickSettings(axis)
{
	var min = $('#PendantJoyMin' + axis).val();
	min = Math.min(Math.max(min, 0), 1023);

	var max = $('#PendantJoyMax' + axis).val();
	max = Math.min(Math.max(max, min), 1023);

	var centerMin = $('#PendantJoyCenterMin' + axis).val();
	var centerMax = $('#PendantJoyCenterMax' + axis).val();
	centerMin = Math.min(Math.max(centerMin, min), max);
	centerMax = Math.min(Math.max(centerMax, centerMin), max);

	return [min, max, centerMin, centerMax];
}

// Reads settings from the dialog input fields
window.RefreshJoystickSettings = function()
{
	g_JoySetX = ParseJoystickSettings('X');
	g_JoySetY = ParseJoystickSettings('Y');
}

// Starts reading the joystick from the pendant and update the graphics
function StartJoystick()
{
	RefreshJoystickSettings();
	WritePort("CAL:STARTJ");

	const centerX = 110;
	const centerY = 110;
	const Scale = 100;
	const MarkerSize = 5;

	var calibrateCtx = document.getElementById("PendantCalCanvas").getContext("2d");
	calibrateCtx.lineWidth = 2;
	calibrateCtx.beginPath();
	calibrateCtx.rect(centerX - Scale - MarkerSize, centerY - Scale - MarkerSize, (Scale+MarkerSize)*2 + 2, (Scale+MarkerSize)*2 + 2);
	calibrateCtx.stroke();

	var calibrateCoords = document.getElementById("PendantCalCoords");
	calibrateCoords.innerHTML = ""

	g_CalibrationTimer = setInterval(function()
	{
		calibrateCtx.clearRect(centerX - Scale - MarkerSize + 1, centerY - Scale - MarkerSize + 1, (Scale+MarkerSize)*2, (Scale+MarkerSize)*2);

		var setX = g_CalibrationStage != undefined ? g_JoyCalX : g_JoySetX;
		var setY = g_CalibrationStage != undefined ? g_JoyCalY : g_JoySetY;

		calibrateCtx.lineWidth = 1;
		if (setX[0] <= setX[1])
		{
			var x1 = centerX + ((setX[0]-512)*Scale)/512 + 1;
			var x2 = centerX + ((setX[1]-512)*Scale)/512 + 1;
			var y1 = centerY - ((setY[0]-512)*Scale)/512 + 1;
			var y2 = centerY - ((setY[1]-512)*Scale)/512 + 1;
			calibrateCtx.beginPath();
			calibrateCtx.strokeStyle = "#00BF00";
			calibrateCtx.rect(x1, y1, x2-x1, y2-y1);
			calibrateCtx.stroke();
		}

		if (setX[2] <= setX[3])
		{
			var x1 = centerX + ((setX[2]-512)*Scale)/512 + 1;
			var x2 = centerX + ((setX[3]-512)*Scale)/512 + 1;
			var y1 = centerY - ((setY[2]-512)*Scale)/512 + 1;
			var y2 = centerY - ((setY[3]-512)*Scale)/512 + 1;

			calibrateCtx.beginPath();
			calibrateCtx.strokeStyle = "#BF0000";
			calibrateCtx.rect(x1, y1, x2-x1, y2-y1);
			calibrateCtx.stroke();
		}

		if (g_RawJoyX != undefined && g_RawJoyY != undefined)
		{
			calibrateCoords.innerHTML = "X: " + g_RawJoyX + ", Y: " + g_RawJoyY;
			var x = centerX + ((g_RawJoyX-512)*Scale)/512 + 1;
			var y = centerY - ((g_RawJoyY-512)*Scale)/512 + 1;
			calibrateCtx.lineWidth = 2;
			calibrateCtx.beginPath();
			calibrateCtx.strokeStyle = "#000000";
			calibrateCtx.moveTo(x - MarkerSize, y);
			calibrateCtx.lineTo(x + MarkerSize, y);
			calibrateCtx.moveTo(x, y - MarkerSize);
			calibrateCtx.lineTo(x, y + MarkerSize);
			calibrateCtx.stroke();
		}

	}, 30);
}

// Stops updating the calibration graphics
function StopJoystick()
{
	if (g_CalibrationTimer)
	{
		clearInterval(g_CalibrationTimer);
		g_CalibrationTimer = undefined;
		g_RawJoyX = undefined;
		g_RawJoyY = undefined;
	}
	document.getElementById("PendantCalText").innerHTML = "";
	WritePort("CAL:STOPJ");
	g_CalibrationStage = undefined;
}

// Begins the calibration process
window.StartCalibration = function()
{
	ShowElement($('#PendantCalibrate'), false);
	ShowElement($('#PendantCalibrateCancel'), true);
	WritePort("CAL:STARTC");
	g_JoyCalX = [1024, -1, 1024, -1];
	g_JoyCalY = [1024, -1, 1024, -1];
	SetCalibrationStage(0);
}

// Cancels the calibration process. external is true if the request comes from the pendant
window.CancelCalibration = function(external)
{
	if (g_CalibrationStage != undefined)
	{
		ShowElement($('#PendantCalibrate'), true);
		ShowElement($('#PendantCalibrateCancel'), false);
		document.getElementById("PendantCalText").innerHTML = "";
		g_CalibrationStage = undefined;
		if (!external)
		{
			WritePort("CAL:STOPC"); // no need to stop the pendant if the request came from it
		}
	}
}

// Sets the current calibration stage: 0 - detect range, 1..8 - detect cener, 9 - completes the calibration
function SetCalibrationStage(stage)
{
	g_CalibrationStage = stage;

	if (g_CalibrationStage == 0)
	{
		document.getElementById("PendantCalText").innerHTML = "<b>Calibrating Range</b><br>Move the stick to all corners multiple times, then click OK on the pendant.";
		UpdateCalibration();
	}
	else if (g_CalibrationStage <= 8)
	{
		document.getElementById("PendantCalText").innerHTML = "<b>Calibrating Center [" + g_CalibrationStage + " of 8]</b><br>Pull the stick away from the center, then release it, then press OK on the pendant.<br>Use different direction each time.";
		g_JoyCalX[2] = Math.min(g_JoyCalX[2], g_RawJoyX);
		g_JoyCalX[3] = Math.max(g_JoyCalX[3], g_RawJoyX);
		g_JoyCalY[2] = Math.min(g_JoyCalY[2], g_RawJoyY);
		g_JoyCalY[3] = Math.max(g_JoyCalY[3], g_RawJoyY);
	}
	else
	{
		WritePort("CAL:STOPC");
		ShowElement($('#PendantCalibrate'), true);
		ShowElement($('#PendantCalibrateCancel'), false);
		document.getElementById("PendantCalText").innerHTML = "<b>Done!</b>";
		g_CalibrationStage = undefined;

		g_JoySetX = g_JoyCalX;
		$('#PendantJoyMinX').val(g_JoyCalX[0]);
		$('#PendantJoyMaxX').val(g_JoyCalX[1]);
		var centerMin = Math.floor((g_JoyCalX[0]*1 + g_JoyCalX[2]*9) / 10);
		var centerMax = Math.ceil((g_JoyCalX[1]*1 + g_JoyCalX[3]*9) / 10);
		g_JoySetX[2] = centerMin;
		g_JoySetX[3] = centerMax;
		$('#PendantJoyCenterMinX').val(centerMin);
		$('#PendantJoyCenterMaxX').val(centerMax);

		g_JoySetY = g_JoyCalY;
		$('#PendantJoyMinY').val(g_JoyCalY[0]);
		$('#PendantJoyMaxY').val(g_JoyCalY[1]);
		var centerMin = Math.floor((g_JoyCalY[0]*1 + g_JoyCalY[2]*9) / 10);
		var centerMax = Math.ceil((g_JoyCalY[1]*1 + g_JoyCalY[3]*9) / 10);
		g_JoySetY[2] = centerMin;
		g_JoySetY[3] = centerMax;
		$('#PendantJoyCenterMinY').val(centerMin);
		$('#PendantJoyCenterMaxY').val(centerMax);
	}
}

// Updates the calibration based on the current raw joystick position
function UpdateCalibration()
{
	if (g_CalibrationStage == 0)
	{
		g_JoyCalX[0] = Math.min(g_JoyCalX[0], g_RawJoyX);
		g_JoyCalX[1] = Math.max(g_JoyCalX[1], g_RawJoyX);
		g_JoyCalY[0] = Math.min(g_JoyCalY[0], g_RawJoyY);
		g_JoyCalY[1] = Math.max(g_JoyCalY[1], g_RawJoyY);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Settings

window.g_SettingsTabIndex = 0; // current settings tab
window.g_bZThicknessValid = false; // if the probe thickness value was modified

// HTML for the body of the settings dialog
// Note: intially this was wrapped in a form, however a button in the form causes the app to reset. Seems to work without a form anyway
const g_SettingsContent = `
<ul data-role="tabs" data-expand="true">
  <li onclick="SelectSettingsTab(0);"><a id="PendantTab1" href="#">Main Settings</a></li>
  <li onclick="SelectSettingsTab(1);"><a id="PendantTab2" href="#">Z Probe</a></li>
  <li onclick="SelectSettingsTab(2);"><a id="PendantTab3" href="#">Tool Probe</a></li>
  <li onclick="SelectSettingsTab(3);"><a id="PendantTab4" href="#">Macros</a></li>
  <li onclick="SelectSettingsTab(4);"><a id="PendantTab5" href="#">Calibrate Joystick</a></li>
</ul>
<div id="PendantTab11" class="row mt-3">
  <label class="cell-sm-4 pt-1" title="Check if you want the pendant to be connected on startup">Connect Automatically</label>
  <div class="cell-sm-6">
    <input id="PendantAutoConnect" type="checkbox" data-role="checkbox" data-style="2" checked />
  </div>
</div>

<div id="PendantTab12" class="row mb-2">
  <label class="cell-sm-4 pt-1" title="Enter text to display on the pendant boot screen.
This setting can only be edited while the pendant is connected.">Pendant Name (up to 18 chars)</label>
  <div class="cell-sm-6">
    <input id="PendantName" data-role="input" data-clear-button="false" data-editable="true" />
  </div>
</div>
<div id="PendantTab13" class="row mb-2">
  <label class="cell-sm-4 pt-1" title="Select the units for the coordinates on the pendant display">Display Units</label>
  <div class="cell-sm-6">
    <select id="PendantDisplayUnits" data-role="select" data-clear-button="false" data-filter="false" onchange="if (g_SettingsTabIndex == 0) { SelectSettingsTab(0); }">
      <option value="mm">mm</option>
      <option value="inches">inches</option>
    </select>
  </div>
</div>

<div id="PendantTab14" class="row mb-2">
  <label class="cell-sm-4 pt-1" title="Type up to 5 values for the jog distance for each click of the wheel">Wheel Click Steps</label>
  <div class="cell-sm-6">
    <input id="PendantWheelStepsMm" data-role="input" data-clear-button="false" data-append="mm/click" data-editable="true" />
  </div>
</div>

<div id="PendantTab15" class="row mb-2">
  <label class="cell-sm-4 pt-1" title="Type up to 5 values for the jog distance for each click of the wheel">Wheel Click Steps</label>
  <div class="cell-sm-6">
    <input id="PendantWheelStepsIn" data-role="input" data-clear-button="false" data-append="inches/click" data-editable="true" />
  </div>
</div>

<div id="PendantTab16" class="row">
  <label class="cell-sm-4" title="Check if you want the spindle to automatically stop when a job is paused.
This is done by triggering the door event. Resuming the job will automatically start the spindle, wait 4 seconds, then resume the motion.
If unchecked, you can still stop the spindle from the pendant using the RPM0 button.">Stop Spindle on Pause</label>
  <div class="cell-sm-6">
    <input id="PendantPauseSpindle" type="checkbox" data-role="checkbox" data-style="2" checked />
  </div>
</div>

<div id="PendantTab17" class="row mb-2">
  <label class="cell-sm-4 pt-1" title="Add up to 3 safety checks that you need to confirm before running a job.
The names are separated by |. Each name is up to 16 characters">Job Checklist</label>
  <div class="cell-sm-6">
    <input id="PendantJobChecklist" data-role="input" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab18" class="row mb-2 pt-1 border-top bd-gray">
  <div class="cell-sm-4">
    <input id="PendantSafeLimits" type="checkbox" data-role="checkbox" data-style="2" data-caption="Enable Safe Jogging Area" checked onchange="if (g_SettingsTabIndex == 0) { SelectSettingsTab(0); }"/>
  </div>
</div>

<div id="PendantTab19" class="row mb-2">
  <div class="cell-sm-2">
    <button id="PendantResetSafeLimits" class="button" onclick="ResetSafeLimits();" title="Reset to the default size based on the Grbl settings.
The machine needs to be connected">Reset</button>
  </div>
  <div class="cell-sm-4">
    <input id="PendantMinX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min X" data-append="mm" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantMaxX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max X" data-append="mm" data-editable="true" />
  </div>
</div>
<div id="PendantTab110" class="row mb-2">
  <label class="cell-sm-2"></label>
  <div class="cell-sm-4">
    <input id="PendantMinY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min Y" data-append="mm" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantMaxY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max Y" data-append="mm" data-editable="true" />
  </div>
</div>
<div id="PendantTab111" class="row mb-2">
  <label class="cell-sm-2"></label>
  <div class="cell-sm-4">
    <input id="PendantMinZ" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min Z" data-append="mm" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantMaxZ" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max Z" data-append="mm" data-editable="true" />
  </div>
</div>

<div id="PendantTab21" class="row  mt-3 mb-2">
  <label class="cell-sm-3 pt-1">Z Probe Sequence</label>
  <div class="cell-sm-4">
  <select id="PendantZStyle" data-role="select" data-clear-button="false" data-filter="false" onchange="if (g_SettingsTabIndex == 1) { SelectSettingsTab(1); }">
    <option value="single">Single Pass</option>
    <option value="dual">Dual Pass</option>
    <option value="default">Use default dialog</option>
  </select>
  </div>
</div>

<div id="PendantTab22" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Percentage of the rapid Z speed to use when Z Down button is pressed.
Z Up always moves at full speed">Down Jog Speed</label>
  <div class="cell-sm-4">
    <input id="PendantZDown" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-append="%" data-editable="true" />
  </div>
</div>

<div id="PendantTab23" class="row mb-2">
  <label class="cell-sm-3 pt-1">Plate Thickness</label>
  <div class="cell-sm-4">
    <input id="PendantZThickness" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-append="mm" data-editable="true" onchange="g_bZThicknessValid = true; $('#PendantResetZThickness').prop('disabled', false);" />
  </div>
  <div class="cell-sm-4">
    <button id="PendantResetZThickness" class="button" onclick="g_bZThicknessValid = false; $('#PendantZThickness').val(localStorage.getItem('z0platethickness')); $('#PendantResetZThickness').prop('disabled', true);" title="Reset to the default value from OpenBuilds">Reset</button>
  </div>
</div>

<div id="PendantTab24" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Check this to use the Z value from the probe contact instead of the position after the probing completes.
They can differ by 0.1-0.2mm as the machine doesn't stop moving immediately on contact." >Use Exact Probe Result</label>
  <div class="cell-sm-4">
    <input id="PendantUseProbe" type="checkbox" data-role="checkbox" data-style="2" checked />
  </div>
</div>

<div id="PendantTab25" class="row mb-2 pt-1 border-top bd-gray">
  <label class="cell-sm-6">First Pass</label>
</div>

<div id="PendantTab26" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Initial movement to locate the Z probe">Seek</label>
  <div class="cell-sm-4">
    <input id="PendantZSeek1T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantZSeek1F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab27" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Retraction after the Z probe was touched.
Use short distance and slower speed if there is a second pass">Retract</label>
  <div class="cell-sm-4">
    <input id="PendantZRetract1T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantZRetract1F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab28" class="row mb-2 pt-1 border-top bd-gray">
  <label class="cell-sm-6">Second Pass</label>
</div>

<div id="PendantTab29" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Second movement to locate the Z probe. Needs to be larger than the Retract from the first pass.
Use shorter distance and slower speed than the first pass for better accuracy">Seek</label>
  <div class="cell-sm-4">
    <input id="PendantZSeek2T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantZSeek2F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab210" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Retraction after the Z probe was touched">Retract</label>
  <div class="cell-sm-4">
    <input id="PendantZRetract2T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantZRetract2F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab31" class="row mt-3 mb-2">
  <label class="cell-sm-3 pt-1">Tool Probe Sequence</label>
  <div class="cell-sm-4">
  <select id="PendantToolStyle" data-role="select" data-clear-button="false" data-filter="false" onchange="if (g_SettingsTabIndex == 2) { SelectSettingsTab(2); }">
    <option value="disable">Disabled</option>
    <option value="single">Single Pass</option>
    <option value="dual">Dual Pass</option>
  </select>
  </div>
</div>

<div id="PendantTab32" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Percentage of the rapid Z speed to use when Z Down button is pressed.
Z Up always moves at full speed">Down Jog Speed</label>
  <div class="cell-sm-4">
   <input id="PendantToolDown" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-append="%" data-editable="true" />
  </div>
</div>

<div id="PendantTab33" class="row mb-2 mt-2">
  <label class="cell-sm-3 pt-1" title="XY location of the tool probe in machine coordinates, and a safe Z above it">Probe Location</label>
  <div class="cell-sm-3">
    <input id="PendantToolX" type="number" style="text-align:right;" data-role="input" data-prepend="X" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-3">
    <input id="PendantToolY" type="number" style="text-align:right;" data-role="input" data-prepend="Y" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-3">
    <input id="PendantToolZ" type="number" style="text-align:right;" data-role="input" data-prepend="Z" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab34" class="row mb-2 pt-1 border-top bd-gray">
  <label class="cell-sm-6">First Pass</label>
</div>

<div id="PendantTab35" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Initial movement to measure the tool">Seek</label>
  <div class="cell-sm-4">
    <input id="PendantToolSeek1T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantToolSeek1F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab36" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Retraction after the tool probe was touched.
Use short distance and slower speed if there is a second pass">Retract</label>
  <div class="cell-sm-4">
    <input id="PendantToolRetract1T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantToolRetract1F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab37" class="row mb-2 pt-1 border-top bd-gray">
  <label class="cell-sm-6">Second Pass</label>
</div>

<div id="PendantTab38" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Second movement to measure the tool. Needs to be larger than the Retract from the first pass.
Use shorter distance and slower speed than the first pass for better accuracy">Seek</label>
  <div class="cell-sm-4">
    <input id="PendantToolSeek2T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantToolSeek2F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<div id="PendantTab39" class="row mb-2">
  <label class="cell-sm-3 pt-1" title="Retraction after the tool probe was touched">Retract</label>
  <div class="cell-sm-4">
    <input id="PendantToolRetract2T" type="number" style="text-align:right;" data-role="input" data-prepend="Travel" data-append="mm" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-4">
    <input id="PendantToolRetract2F" type="number" style="text-align:right;" data-role="input" data-prepend="Feed" data-append="mm/min" data-clear-button="false" data-editable="true" />
  </div>
</div>

<p id="PendantTab41" class="small mb-4">Select up to 7 of the existing OpenBuilds CONTROL macros to activate from the pendant.
Enter the macro name in the first column. In the second column you can enter a display name to show on the pendant screen. It can be up to 8 characters long.<br>
Check the Hold box to require the button to be pressed for 1 second (prevents accidental activation).</p>

<div id="PendantTab42" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText1" data-role="input" data-prepend="#1" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel1" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold1" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab43" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText2" data-role="input" data-prepend="#2" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel2" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold2" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab44" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText3" data-role="input" data-prepend="#3" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel3" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold3" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab45" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText4" data-role="input" data-prepend="#4" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel4" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold4" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab46" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText5" data-role="input" data-prepend="#5" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel5" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold5" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab47" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText6" data-role="input" data-prepend="#6" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel6" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold6" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<div id="PendantTab48" class="row mb-2">
  <div class="cell-sm-5">
    <input id="PendantMacroText7" data-role="input" data-prepend="#7" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-5">
    <input id="PendantMacroLabel7" data-role="input" data-prepend="Label" data-clear-button="false" data-editable="true" />
  </div>
  <div class="cell-sm-1">
    <input id="PendantMacroHold7" type="checkbox" data-role="checkbox" data-style="2" data-caption="Hold" />
  </div>
</div>

<p id="PendantTab51" class="small mb-4">Move the joystick on the pendant to verify it is connected. Press the Start Calibration
button to begin the calibration process, which will determine the range of motion and the center zone for each axis.
You can also enter the values manually.</p>

<div id="PendantTab52" class="row mb-3 mt-5">
  <div class="cell-sm-4">
    <canvas id="PendantCalCanvas" width="220" height="220" />
    <br/>
    <p id="PendantCalCoords">X:1023, Y:1023</p>
  </div>
  <div class="cell-sm-4">
    <button id="PendantCalibrate" class="button success" onclick="StartCalibration();">Start Calibration</button>
    <p id="PendantCalText"></p>
    <br>
    <button id="PendantCalibrateCancel" class="button" onclick="CancelCalibration(false);">Cancel Calibration</button>
  </div>
</div>

<div id="PendantTab53" class="row pt-1 border-top bd-gray">
  <div class="cell-sm-6 border-right bd-gray">
    <div class="row mb-2">
      <label class="cell-sm-6" title="The min and max value that the joystick can reach">Joystick Range</label>
    </div>

    <div class="row mb-2">
      <div class="cell-sm-6">
        <input id="PendantJoyMinX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min X" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
      <div class="cell-sm-6">
        <input id="PendantJoyMaxX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max X" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
    </div>

    <div class="row mb-2">
      <div class="cell-sm-6">
        <input id="PendantJoyMinY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min Y" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
      <div class="cell-sm-6">
        <input id="PendantJoyMaxY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max Y" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
    </div>
  </div>

  <div class="cell-sm-6">
    <div class="row mb-2">
      <label class="cell-sm-6" title="The size of the center zone. When the curor is inside the zone, the joystick is considered to be released.
The calibration process uses a conservative estimate. If your joystick is more accurate, you can manually reduce the size of the center zone">Center Zone</label>
    </div>

    <div class="row mb-2">
      <div class="cell-sm-6">
        <input id="PendantJoyCenterMinX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min X" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
      <div class="cell-sm-6">
        <input id="PendantJoyCenterMaxX" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max X" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
    </div>

    <div class="row mb-2">
      <div class="cell-sm-6">
        <input id="PendantJoyCenterMinY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Min Y" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
      <div class="cell-sm-6">
        <input id="PendantJoyCenterMaxY" type="number" style="text-align:right;" data-role="input" data-clear-button="false" data-prepend="Max Y" data-editable="true" onchange="RefreshJoystickSettings();"/>
      </div>
    </div>
  </div>
</div>`

// Shows or hides a dialog element
function ShowElement(el, show)
{
	if (show)
	{
		el.show();
	}
	else
	{
		el.hide();
	}
}

// Selects a tab in the settings. Shows/hides the necessary elements
window.SelectSettingsTab = function(index)
{
	if (index != g_SettingsTabIndex)
	{
		if (index == 4 && g_PendantPort == undefined)
		{
			return;
		}
		g_SettingsTabIndex = index;
	}
	var tab1 = (index == 0);
	var tab2 = (index == 1);
	var tab3 = (index == 2);
	var tab4 = (index == 3);
	var tab5 = (index == 4);
	ShowElement($('#PendantTab11,#PendantTab12,#PendantTab13,#PendantTab16,#PendantTab17,#PendantTab18,#PendantTab19,#PendantTab110,#PendantTab111'), tab1);

	var safeLimitsDisabled = !$('#PendantSafeLimits').prop('checked');
	$('#PendantResetSafeLimits,#PendantMinX,#PendantMaxX,#PendantMinY,#PendantMaxY,#PendantMinZ,#PendantMaxZ').prop('disabled', safeLimitsDisabled);

	var displayUnits = $('#PendantDisplayUnits').val();
	ShowElement($('#PendantTab14'), tab1 && displayUnits != "inches");
	ShowElement($('#PendantTab15'), tab1 && displayUnits == "inches");

	var zStyle = $('#PendantZStyle').val();
	ShowElement($('#PendantTab21,#PendantTab22'), tab2);
	ShowElement($('#PendantTab23,#PendantTab24,#PendantTab26,#PendantTab27'), tab2 && zStyle != "default");
	ShowElement($('#PendantTab25,#PendantTab28,#PendantTab29,#PendantTab210'), tab2 && zStyle == "dual");

	ShowElement($('#PendantTab31'), tab3);

	var toolStyle = $('#PendantToolStyle').val();
	ShowElement($('#PendantTab32,#PendantTab33,#PendantTab35,#PendantTab36'), tab3 && toolStyle != "disable");
	ShowElement($('#PendantTab34,#PendantTab37,#PendantTab38,#PendantTab39'), tab3 && toolStyle == "dual");

	ShowElement($('#PendantTab41,#PendantTab42,#PendantTab43,#PendantTab44,#PendantTab45,#PendantTab46,#PendantTab47,#PendantTab48'), tab4);

	ShowElement($('#PendantTab51,#PendantTab52,#PendantCalibrate,#PendantTab53'), tab5);
	ShowElement($('#PendantCalibrateCancel'), false);

	if (tab5)
	{
		StartJoystick();
	}
	else
	{
		StopJoystick();
	}
}

function GetDefaultSafeLimits(axis)
{
	var param;
	switch (axis)
	{
		case "X": param = grblParams["$130"]; break;
		case "Y": param = grblParams["$131"]; break;
		case "Z": param = grblParams["$132"]; break;
	}

	if (laststatus.connectionStatus != 0 && param != undefined)
	{
		return {min: 3 - param, max: -3};
	}
	return {min: 0, max: 0};
}

function GetSettingsSafeLimits(settings, axis)
{
	var limits = settings["safeLimits" + axis];
	return (limits.min == 0 && limits.max == 0) ? GetDefaultSafeLimits(axis) : limits;
}

function GetSafeLimits(axis)
{
	return GetSettingsSafeLimits(g_PendantSettings, axis);
}

window.ResetSafeLimits = function()
{
	if (laststatus.connectionStatus != 0)
	{
		var limits = GetDefaultSafeLimits('X');
		$('#PendantMinX').val(limits.min);
		$('#PendantMaxX').val(limits.max);
		limits = GetDefaultSafeLimits('Y');
		$('#PendantMinY').val(limits.min);
		$('#PendantMaxY').val(limits.max);
		limits = GetDefaultSafeLimits('Z');
		$('#PendantMinZ').val(limits.min);
		$('#PendantMaxZ').val(limits.max);
	}
}

// Reads the settings from the dialog and saves them
function ReadSettingsFromDialog(updateRomSettings)
{
	// main settings
	g_PendantSettings.autoConnect = $('#PendantAutoConnect').prop('checked');
	g_PendantSettings.displayUnits = $('#PendantDisplayUnits').val();

	g_PendantSettings.wheelStepsMm = $('#PendantWheelStepsMm').val().split(',').map(Number);
	g_PendantSettings.wheelStepsIn = $('#PendantWheelStepsIn').val().split(',').map(Number);

	g_PendantSettings.safeLimitsEnabled = $('#PendantSafeLimits').prop('checked');
	g_PendantSettings.safeLimitsX = {min: Number($('#PendantMinX').val()), max: Number($('#PendantMaxX').val())};
	g_PendantSettings.safeLimitsY = {min: Number($('#PendantMinY').val()), max: Number($('#PendantMaxY').val())};
	g_PendantSettings.safeLimitsZ = {min: Number($('#PendantMinZ').val()), max: Number($('#PendantMaxZ').val())};

	// advanced settings
	g_PendantSettings.pauseSpindle = $('#PendantPauseSpindle').prop('checked');
	g_PendantSettings.jobChecklist = $('#PendantJobChecklist').val();
	g_PendantSettings.zProbe.style = $('#PendantZStyle').val();
	g_PendantSettings.zProbe.downSpeed = Math.min(Math.max($('#PendantZDown').val(), 10), 100);
	g_PendantSettings.zProbe.thickness = g_bZThicknessValid ? Number($('#PendantZThickness').val()) : undefined;
	g_PendantSettings.zProbe.useProbeResult = $('#PendantUseProbe').prop('checked');
	g_PendantSettings.zProbe.seek1 = {travel: Number($('#PendantZSeek1T').val()), feed: Number($('#PendantZSeek1F').val())};
	g_PendantSettings.zProbe.retract1 = {travel: Number($('#PendantZRetract1T').val()), feed: Number($('#PendantZRetract1F').val())};
	g_PendantSettings.zProbe.seek2 = {travel: Number($('#PendantZSeek2T').val()), feed: Number($('#PendantZSeek2F').val())};
	g_PendantSettings.zProbe.retract2 = {travel: Number($('#PendantZRetract2T').val()), feed: Number($('#PendantZRetract2F').val())};

	g_PendantSettings.toolProbe.style = $('#PendantToolStyle').val();
	g_PendantSettings.toolProbe.downSpeed = Math.min(Math.max($('#PendantToolDown').val(), 10), 100);
	g_PendantSettings.toolProbe.location = {X: Number($('#PendantToolX').val()), Y: Number($('#PendantToolY').val()), Z: Number($('#PendantToolZ').val())};
	g_PendantSettings.toolProbe.seek1 = {travel: Number($('#PendantToolSeek1T').val()), feed: Number($('#PendantToolSeek1F').val())};
	g_PendantSettings.toolProbe.retract1 = {travel: Number($('#PendantToolRetract1T').val()), feed: Number($('#PendantToolRetract1F').val())};
	g_PendantSettings.toolProbe.seek2 = {travel: Number($('#PendantToolSeek2T').val()), feed: Number($('#PendantToolSeek2F').val())};
	g_PendantSettings.toolProbe.retract2 = {travel: Number($('#PendantToolRetract2T').val()), feed: Number($('#PendantToolRetract2F').val())};

	// macros
	for (var idx = 1; idx <= 7; idx++)
	{
		var name = $('#PendantMacroText' + idx).val();
		var label = $('#PendantMacroLabel' + idx).val();
		var hold = $('#PendantMacroHold' + idx).prop('checked') == true;
		g_PendantSettings["macro" + idx] = {name: name, label: label, hold: hold};
	}

	// ROM settings
	if (updateRomSettings)
	{
		g_PendantRomSettings.pendantName = $('#PendantName').val();
		g_PendantRomSettings.calibrationX = ParseJoystickSettings('X');
		g_PendantRomSettings.calibrationY = ParseJoystickSettings('Y');
	}

	// save to persistent storage and push to the pendant
	localStorage.setItem("PendantSettings", JSON.stringify(g_PendantSettings));
	PushSettings(updateRomSettings);
}

// Updates the settings controls based on the specified collection
function UpdateSettingsControls(settings, romSettings)
{
	// main settings
	$('#PendantAutoConnect').prop('checked', settings.autoConnect);
	var select = $('#PendantDisplayUnits').data("select");
	if (select)
	{
		select.val(settings.displayUnits);
	}
	else
	{
		$('#PendantDisplayUnits').val(settings.displayUnits);
	}

	var steps = undefined;
	settings.wheelStepsMm.forEach(s => { steps = steps ? (steps + ", " + s.toFixed(2)) : s.toFixed(2); });
	$('#PendantWheelStepsMm').val(steps);

	steps = undefined;
	settings.wheelStepsIn.forEach(s => { steps = steps ? (steps + ", " + s.toFixed(3)) : s.toFixed(3); });
	$('#PendantWheelStepsIn').val(steps);

	$('#PendantSafeLimits').prop('checked', settings.safeLimitsEnabled);

	var limits = GetSettingsSafeLimits(settings, 'X');
	$('#PendantMinX').val(limits.min);
	$('#PendantMaxX').val(limits.max);

	limits = GetSettingsSafeLimits(settings, 'Y');
	$('#PendantMinY').val(limits.min);
	$('#PendantMaxY').val(limits.max);

	limits = GetSettingsSafeLimits(settings, 'Z');
	$('#PendantMinZ').val(limits.min);
	$('#PendantMaxZ').val(limits.max);

	// advanced settings
	$('#PendantPauseSpindle').prop('checked', settings.pauseSpindle);
	$('#PendantJobChecklist').val(settings.jobChecklist);

	var select = $('#PendantZStyle').data("select");
	if (select)
	{
		select.val(settings.zProbe.style);
	}
	else
	{
		$('#PendantZStyle').val(settings.zProbe.style);
	}

 	if (settings.zProbe.thickness == undefined)
	{
		g_bZThicknessValid = false;
		$('#PendantZThickness').val(localStorage.getItem('z0platethickness'));
	}
	else
	{
		g_bZThicknessValid = true;
		$('#PendantZThickness').val(settings.zProbe.thickness);
	}
	$('#PendantResetZThickness').prop('disabled', !g_bZThicknessValid);

	$('#PendantUseProbe').prop('checked', g_PendantSettings.zProbe.useProbeResult);
	$('#PendantZDown').val(settings.zProbe.downSpeed);
	$('#PendantZSeek1T').val(settings.zProbe.seek1.travel);
	$('#PendantZSeek1F').val(settings.zProbe.seek1.feed);
	$('#PendantZRetract1T').val(settings.zProbe.retract1.travel);
	$('#PendantZRetract1F').val(settings.zProbe.retract1.feed);

	$('#PendantZSeek2T').val(settings.zProbe.seek2.travel);
	$('#PendantZSeek2F').val(settings.zProbe.seek2.feed);
	$('#PendantZRetract2T').val(settings.zProbe.retract2.travel);
	$('#PendantZRetract2F').val(settings.zProbe.retract2.feed);


	select = $('#PendantToolStyle').data("select");
	if (select)
	{
		select.val(settings.toolProbe.style);
	}
	else
	{
		$('#PendantToolStyle').val(settings.toolProbe.style);
	}

	$('#PendantToolX').val(settings.toolProbe.location.X);
	$('#PendantToolY').val(settings.toolProbe.location.Y);
	$('#PendantToolZ').val(settings.toolProbe.location.Z);
	$('#PendantToolDown').val(settings.toolProbe.downSpeed);
	$('#PendantToolSeek1T').val(settings.toolProbe.seek1.travel);
	$('#PendantToolSeek1F').val(settings.toolProbe.seek1.feed);
	$('#PendantToolRetract1T').val(settings.toolProbe.retract1.travel);
	$('#PendantToolRetract1F').val(settings.toolProbe.retract1.feed);

	$('#PendantToolSeek2T').val(settings.toolProbe.seek2.travel);
	$('#PendantToolSeek2F').val(settings.toolProbe.seek2.feed);
	$('#PendantToolRetract2T').val(settings.toolProbe.retract2.travel);
	$('#PendantToolRetract2F').val(settings.toolProbe.retract2.feed);

	var autoComplete = [];
	for (var i = 0; i < buttonsarray.length; i++)
	{
		var button = buttonsarray[i];
		if (button.codetype == "gcode" || !button.jsrunonstartup)
		{
			autoComplete.push(button.title);
		}
	}

	// macros
	for (var idx = 1; idx <= 7; idx++)
	{
		var macro = settings["macro" + idx];
		$('#PendantMacroText' + idx).val(macro.name);
		$('#PendantMacroLabel' + idx).val(macro.label);
		$('#PendantMacroHold' + idx).prop('checked', macro.hold);
		if (autoComplete.length > 0)
		{
			$('#PendantMacroText' + idx).attr('data-autocomplete', autoComplete);
		}
	}

	// ROM settings
	if (romSettings)
	{
		$('#PendantName').val(romSettings.pendantName);
		$('#PendantJoyMinX').val(romSettings.calibrationX[0]);
		$('#PendantJoyMaxX').val(romSettings.calibrationX[1]);
		$('#PendantJoyCenterMinX').val(romSettings.calibrationX[2]);
		$('#PendantJoyCenterMaxX').val(romSettings.calibrationX[3]);
		$('#PendantJoyMinY').val(romSettings.calibrationY[0]);
		$('#PendantJoyMaxY').val(romSettings.calibrationY[1]);
		$('#PendantJoyCenterMinY').val(romSettings.calibrationY[2]);
		$('#PendantJoyCenterMaxY').val(romSettings.calibrationY[3]);
	}
}

// Opens a dialog to edit the pendant settings
window.EditPendantSettings = function()
{
	Metro.dialog.create({
		title: "<i class='fas fa-mobile-alt'></i> Pendant Settings",
		content: g_SettingsContent,
		width: '800',
		clsDialog: 'dark',
		actions: [
			{
				caption: "Reset To Defaults",
				onclick: function()
				{
					UpdateSettingsControls(GetDefaultSettings(), g_PendantPort != undefined ? GetDefaultRomSettings() : undefined);
					SelectSettingsTab(g_SettingsTabIndex);
				}
			},
			{
				caption: "Save",
				cls: "js-dialog-close success",
				onclick: function()
				{
					ReadSettingsFromDialog(g_PendantPort != undefined);
				}
			},
			{
				caption: "Cancel",
				cls: "js-dialog-close",
				onclick: function() {}
			}
		],
		onClose: function()
		{
			StopJoystick();
		}
	});

	if (g_PendantPort == undefined)
	{
		$('#PendantName').prop('disabled', true);
		$('#PendantTab5').prop('disabled', true);
		$('#PendantTab5').prop('title', "Only available when the pendant is connected");
	}

	UpdateSettingsControls(g_PendantSettings, g_PendantRomSettings);

	SelectSettingsTab(0);
}

///////////////////////////////////////////////////////////////////////////////

var pendantButton = `
<div class="group">
  <div>
    <button id="pendant" class="ribbon-button dropdown-toggle">
      <span class="icon">
        <span class="fa-layers fa-fw">
          <i class="fas fa-mobile-alt fg-blue"></i>
        </span>
      </span>
      <span class="caption grblmode">Pendant</span>
    </button>
    <ul class="ribbon-dropdown" data-role="dropdown" data-duration="100">
      <li class="" id="ConnectPendant" onclick="ConnectPendant();"><a href="#">Connect</a></li>
      <li class="disabled" id="DisconnectPendant"><a href="#">Disconnect</a></li>
      <li class="" id="PendantSettings" onclick="EditPendantSettings();"><a href="#">Settings</a></li>

    </ul>
  </div>
  <span class="title">Pendant</span>
</div>`

// Cleans up old instance of the plugin. useful when iterating on the code
function CleanupOldVersion()
{
	if ($('#pendant'))
	{
		$('#DisconnectPendant').click();
		$('#pendant').parent().parent().remove();
	}
}

// Recursively merge stored settings into current if the name and type match
function MergeSettingsRec(current, stored)
{
	for (var prop in stored)
	{
		if (prop in current && typeof(stored[prop]) == typeof(current[prop]) && Array.isArray(stored[prop]) == Array.isArray(current[prop]))
		{
			if (typeof(current[prop]) == 'object' && !Array.isArray(current[prop]))
			{
				MergeSettingsRec(current[prop], stored[prop]);
			}
			else
			{
				current[prop] = stored[prop];
			}
		}
	}
}

// Initializes the plugin. tries to auto-connect if enabled
function InitializePendantPlugin()
{
	// create new button
	$('#controlBtnGrp').before(pendantButton);

	// this exports the ShowPendantDialog function to be used by other macros and allows them to create UI on the pendant screen.
	// put this at the top of the new macro:
	// var ShowPendantDialog = $('#pendant').prop('ShowDialog');
	// and then you can use ShowPendantDialog as normal
	$('#pendant').prop('ShowDialog', () => { return ShowPendantDialog; });

	// read settings
	if (localStorage.getItem("PendantSettings"))
	{
		var settings = JSON.parse(localStorage.getItem("PendantSettings"));
		if (settings)
		{
			MergeSettingsRec(g_PendantSettings, settings);
		}

		$('#DisconnectPendant').on('click', DisconnectPendant);
	}

	// delay until the next frame to allow the UI to update
	setTimeout(function()
	{
		DisconnectPendant();
		// auto-connect if enabled
		if (g_PendantSettings.autoConnect == true)
		{
			ConnectPendant();
		}
	}, 0);
}

CleanupOldVersion()
InitializePendantPlugin()
