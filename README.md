# Magic Visor Controller
Arduino controller built for Adafruit Feather BLE. 

This contoller works with the [Magic Visor Android App](https://github.com/keithfry/visor-android).

## Setup
### Install Adafruit boards
_You only need to do this once for the Arduino IDE_
1. In Arduino IE, open Settings / Preferences
2. Add the Adafruit board library URL in "Additional boards manager URL:"         
   https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
3. Go to Tools > Board > Boards Manager.
4. Search for "feather" and find "Adafruit AVR Boards", click Install

### Install Adafruit Bluetooth Libraries
_You only need to do this once per project_
1. Open your IDE and open your Sketch
2. Access the Library Manager: Go to Sketch > Include Library > Manage Libraries....
3. Search for the Adafruit BluefruitLE nRF51 library and Adafruit Dotstar library, then install each

More on this library is here: https://learn.adafruit.com/adafruit-feather-32u4-bluefruit-le/installing-ble-library


## Runtime
### Initialization
setup() is called when the board is initialized, we do two steps:
1. setupBLE() - configure BLE
   1. create a device "FloraDress" (this is an old name when we were building it with a Flora)
   2. create a service with a registered callback for events we receive from a client
2. setupMatrix() - configure the visor panel with the dimensions, pin connections, and other configuration needed
   1. Matrix uses DATAPIN = 5 and CLOCKPIN = 6
> [!Important]
> If you've connected to different pins you'll need to change these values

We also set the FUNC_HEARTS as the default mode.

### Client Requests
The client can request to change modes or display characteristics.

#### Modes
The following modes execute based on request from the client:
1. **FUNC_HEART** : displays scrolling red hearts on a purple background.
2. **FUNC_RAINBOW** : displays a scrolling continuous rainbow
3. **FUNC_SCROLLING_TEXT** : displays scrolling text in the selected color provided by client.
4. **FUNC_POP_TEXT** : splits the text by spaces and if possible displays each word in sequence in selected color.
5. **FUNC_DARK** : display nothing, can be used to conserve battery since LED illumination takes the most power

#### Display Characteristics
1. **FUNC_COLOR_CHANNGE**: For text display, the client can change the display color through a message with RGB values
2. **FUNC_SET_BRIGHTNESS**: the client can change the brightness through a message with intensity value, can be used for different conditions or to save battery 
3. **FUNC_SET_PULSE**: the client can change the pulse speed from no pulse to up to 8 pulses per second (which can be a bit nauseating) for a different effect.

   
