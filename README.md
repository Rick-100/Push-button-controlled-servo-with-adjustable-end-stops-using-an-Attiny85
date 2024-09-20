# Push-button-controlled-servo-with-adjustable-end-stops-using-an-Attiny85

This repository is for this video:

https://youtu.be/g3QAqGpAoNo

This program is written for the Attiny85. It moves a servo between 2 end positions using two push buttons. The end positions are adjustable by using a program mode selected by a third button. The positions are stored in eeprom so they are not lost on power down. If the circuit is powered on with the program button held down, the default values are restored. The project was started after watching this video by pileofstuff:

https://youtu.be/vMrGfrk_M0I

The video is about controlling the track switches on a model train layout with either push buttons or IR sensors. The push buttons are for manual control. The IR sensors detect a train coming and set the switch in the direction that won't derail the train. The end positions of the servos need to be adjustable so the servo doesn't bind. In the video he used pots on an Arduino Nano to set the end positions. The push buttons and IR sensors were wired in parallel. While reading the video comments, people suggested using this method so I thought I would give it a try on the Attiny85 to get some more experience with the chip. I'm also designing an experimenters board for the Attiny85 and needed a project to test it out.

The program uses the Arduino IDE and this core:

https://github.com/SpenceKonde/ATTinyCore

While the program uses the Arduino IDE, I started development with Atmel Studio 7. That's why the code looks a little different than most Arduino code. I did not use the Arduino methods of manipulating the pins. When compiling and burning the code use the following settings in the IDE:

 *  Attiny is running at 8 MHz internal
 *  Timer 1 clock: CPU frequency
 *  millis() micos(): disabled
 *  Save eeprom: eeprom not detained
 *  B.O.D. disabled
 *  

The interrupt driven servo routines are a modified version of routines from this program by Andreas Ennemoser:

https://github.com/chiefenne/ATTINY85-Servo-Control/blob/master/main.c

I modified them to get more servo travel at the price of some resolution:

 *defaults servo positions servo operates between 2 and 187
 *pulse lengths between approximately 750uS and 2250 uS

Most of the program is taken up by the user interface. Since the servo refresh rate is approximately 50 Hz, the main loop is synced to the refresh. The program uses 4 modes to keep track of what it's doing:

 * the user interface is 3 pushbuttons PLUS   PROG    MINUS and an led
 * 
 * mode 0 = solid led / normal running mode 
 *  pressing PLUS moves the servo end PLUS end position
 *  pressing MINUS moves the servo end MINUS end position
 * 

 * mode 1 = slowest flashing led  
 *  the prog button was pressed and it's waiting for a press on PLUS or MINUS to select which end to adjust
 * the servo will move to the initial position stored in variables plus_limit or minus_limit and enter mode 2
 * 

 * mode 2 = faster flashing led / the servo positions are adjusted until PROG is pressed and released  
 *  then there stored in eeprom and mode 0 is entered
 *

 *mode 3 = even faster flashing led on power up / the eeprom was read and the data was out of range or bad checksum 
 *  the limits were restored to defaults but not written to eeprom
 *  when the PROG button is pressed and released the data is written to eeprom and mode is set to 0
 *  back to solid led
 *

 *mode 4 = fastest flashing led / happens after aprox 10 seconds of no buttons pressed 
 *  servos pulses have been disabled so the servo is no longer actively driven
 *  pressing PLUS or MINUS enables the servo pulses and puts mode back to 0 with solid led
 *  pressing and releasing PROG enables servo pulses and puts mode back to 0 with solid led
 * 
 * 
 *to reset to values stored in constants MAXPLUS and MAXMINUS power up with PROG button held down
 * 

In his video pileofstuff showed the push buttons and IR sensors wired in parallel. When I was testing the circuit with the sensors , if a sensor was blocked it interferes with the push button. This could probably be fixed by adding a spdt toggle switch on each Attiny85 pin that the buttons are on. The toggle switch could select either the button or the sensor to connect to the pin. 

This has been a fun project even though I'm not into model trains. I think it might be useful for other non train projects. The code is a starting point for another project I have in mind. I've been using my 3018 CNC to make boards with the isolation routing method, so I designed some circuit boards for the project. I'll include the gerber files in case anyone wants them. There are two boards. The one with the Attiny85 and the one that has the push buttons and led. The attiny board has headers for connecting the IR sensors. The sensors have a non standard pinout but the board uses the hobby servo standard of 1:GND 2:V+ 3:SIG so you have to swap the wire in the hook up wires you use.

Link to open main board in EasyEda editor:
https://easyeda.com/editor#id=|053991f04dec438685bc99f91181d4e4|dd444679e9d94d9bacffa765860c2ff6

Link to open user input board in EasyEda editor:

https://easyeda.com/editor#id=|87594c4b0b304a26913ac3bafe0e990d|861154c907374d65bb66870aeddd5e62

I added the servo_limits_speed.ino to give the option of controlling the speed the servo moves.

Thanks




