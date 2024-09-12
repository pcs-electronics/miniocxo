# Mini OCXO generator with independently programmable 3x outputs, 330 kHz to 330 MHz, plus a direct 10 MHz output

[PCS Electronics shop](https://www.pcs-electronics.com/shop/rigexpert-products/other-reu-rigexpert-products/ocxo-3-channel-signal-generator-330khz-to-330mhz-2/)

![Image](/IMAGES/miniocxo-front.png)
![Image](/IMAGES/miniocxo-rear.png)

## Areas of potential use:
* Stable reference frequency for radio communications: transmitters, receivers
* Amateur radio: QO-100, VHF, SHF, FT8
* Laboratory-grade, stable RF signal source
* High end audio for synchronizing Hi-Fi ADCs and DACs
* Analog and digital radio and TV broadcasting
* Laboratory work, with three independent channels precisely in phase and thermally stabilized

## Arduino environment

Install the board as described here: https://github.com/DeqingSun/ch55xduino

( See also: https://sourceforge.net/p/sdcc/bugs/3569/ )

>Tools - Board - CH5xx boards - CH552 
>Tools - USB settings - Default CDC 
>Tools - Upload method - USB 
>Tools - Clock source - 24 MHz internal 

## Updating the firmware:

Install and run the WCHISPTool from http://www.wch-ic.com/downloads/WCHISPTool_Setup_exe.html

>Tab: CH55x Series 
>Chip model: CH552, download type: USB 
>User file: select the HEX file with a firmware. 

Connect the PROG jumper, cycle the power (not the USB port!), remove the PROG jumper

Check the Device Manager: under "Interface" branch, there should be a "USB module" device.

If there is an "Unknown device", go to the WCHISPTool folder and install the driver.

In the WCHISPTool, click Search and make sure the "MCU model: CH552" appeared in the list.

Click Download

Cycle the power

Now the Device manager will list a new "USB Serial Port" under "Ports (COM & LPT").

Alternatively, instead of WCHISPTool, use vnproch55x :

>vnproch55x firmware.hex -r2

