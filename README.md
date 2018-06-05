# Touch2Push
Drive the Ableton Push 2 Display with Touchdesigner

Please see http://futurewife.tv/touch2push for more info

I cannot claim that this software will not damage your Push 2 since I haven't done enough testing to ensure that it is safe, so use at your own risk! It is technically possible to brick your Push 2 by sending the wrong frame header (accidentally sending the "flash firmware" header). This code *should* never do that, but if you modify the frame header in the source this becomes a possibility.

I used sample code from Ableton's Push 2 Github to get started: https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc and had to reference their example with JUCE quite a bit to figure out how to do the nitty gritty stuff: https://github.com/Ableton/push2-display-with-juce

Also note that the Visual Studio project and code for the CPlusPlus CHOP were modified from Derivative's Touchdeisgner CPlusPlus CHOP examples. The original example project comes with the installation of Touchdesigner if you're curious.

Installation:

1. Clone this repository https://github.com/beauburrows/Touch2Push

1. Download Zadig https://zadig.akeo.ie/ and use it to replace the driver for the "Ableton Push 2 Display" with libusb0 (the Push 2 must be plugged into your computer for this step). Note that replacing USB drivers can render hardware useless to that computer if you install incorrect drivers for other devices, so be careful with this step!

2. Download latest libusb windows binaries (https://libusb.info/), copy the three files in MS64/dll and paste them in the CHOP/include folder of this cloned repository (at the time of writing this the libusb version was 1.0.22)

4. Download latest libusb source files (https://libusb.info/), copy all the files in /libusb and paste them in the CHOP/include folder of this cloned repository (at the time of writing this the libusb version was 1.0.22)

4. Open "CHOP/Push2CHOP.sln" in Visual Studio 2015 and build the dll

5. Install Touchdesigner, then open touchdesigner/touch2push.toe (my version of Touchdesigner was 23120 at the time of writing this). Select the cpluspluschop operator, go the the custom paramters, and click "open". This should connect to the Push 2 plugged into your computer and send to the display.

6. Make sure you click "close" on the cpluspluschop before your close Touchdesigner (if anyone has an alternative to needed to click close before closing touch, please let me know!)
