#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_DotStarMatrix.h>
#include <Adafruit_DotStar.h>
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif

/*
#include <string.h>
#include <Arduino.h>
*/
/*
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
#include <SoftwareSerial.h>
#endif
*/


#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BLEGatt.h"
#include "BluefruitConfig.h"

#ifdef __AVR__
  #include <avr/power.h>
#endif


#define FACTORYRESET_ENABLE        1
#define MINIMUM_FIRMWARE_VERSION   "0.7.0"

#define BLUEFRUIT_HWSERIAL_NAME      Serial1


// Setup Matrix
////////////////////////

#define DATAPIN  5
#define CLOCKPIN 6

// MATRIX DECLARATION:
// Parameter 1 = width of DotStar matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   DS_MATRIX_TOP, DS_MATRIX_BOTTOM, DS_MATRIX_LEFT, DS_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     DS_MATRIX_TOP + DS_MATRIX_LEFT for the top-left corner.
//   DS_MATRIX_ROWS, DS_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   DS_MATRIX_PROGRESSIVE, DS_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type:
//   DOTSTAR_BRG  Pixels are wired for BRG bitstream (most DotStar items)
//   DOTSTAR_GBR  Pixels are wired for GBR bitstream (some older DotStars)

Adafruit_DotStarMatrix matrix = Adafruit_DotStarMatrix(
  32, 8, DATAPIN, CLOCKPIN,
  DS_MATRIX_BOTTOM     + DS_MATRIX_RIGHT +
  DS_MATRIX_COLUMNS + DS_MATRIX_ZIGZAG,
  DOTSTAR_BRG);


// FEATHER

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

Adafruit_BLEGatt gatt =   Adafruit_BLEGatt(ble);



int32_t charid_number;

#define FUNC_SCROLL_TEXT 2
#define FUNC_RAINBOW 3
#define FUNC_HEARTS 4
#define FUNC_POP_TEXT 5
#define FUNC_CHANGE_COLOR 10
#define FUNC_SET_BRIGHTNESS 11
#define FUNC_SET_PULSE 12

#define FUNC_DARK 20

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void connected(void)
{
  Serial.println( F("Connected") );
//  light.setPixelColor(0, colors[1]); // Green
//  light.show();
}

void disconnected(void)
{
  Serial.println( F("Disconnected") );
//  light.setPixelColor(0, colors[0]); // Red
//  light.show();
}


// Start with HEARTS.
byte lightVal=FUNC_HEARTS;
byte oldLightVal=0;

#define PULSE_RATE 5
bool pulseOn = false;
int pulseOffset = PULSE_RATE;


char text[20]; // Text that came from client.
unsigned int textLength=0;

// Bluetooth callback.
void BleGattRX(int32_t chars_id, uint8_t data[], uint16_t len)
{
  Serial.print( F("[BLE GATT RX] (" ) );
  Serial.print(chars_id);
  Serial.print(") ");
  oldLightVal = lightVal;
  lightVal = data[0];
  Serial.println(lightVal);

  Serial.print("data length: ");
  Serial.println(len);
  for (int i=0; i < len; i++) {
    if (i>0) Serial.print(",");
    Serial.print(data[i]);
  }
  Serial.println();

  // Setup text scrolling
  if (lightVal == FUNC_SCROLL_TEXT) {
    setupScrollingText(data, len);
  }
  // Text popping
  else if (lightVal == FUNC_POP_TEXT) {
    setupPopText(data, len);
  }
  // Color change
  else if (lightVal == FUNC_CHANGE_COLOR) {
    changeColor(data, len);
    lightVal = oldLightVal; // We don't change function, just send message.
  }
  // Brightness
  else if (lightVal == FUNC_SET_BRIGHTNESS) {
    pulseOn = false; // Turn off pulsing
    changeBrightness(data, len);
    lightVal = oldLightVal; // We don't change function, just send message.
  }
  else if (lightVal == FUNC_SET_PULSE) {
    pulseOn = true;
    lightVal = oldLightVal;
  }
  // Testing: see what's on the data packet.
  

  //Serial.println("BleGattRX");
}

uint32_t color = matrix.Color(255,0,0);
int bright = 10;
void changeColor(uint8_t data[], uint16_t len) {
  // Data comes as R, G, B
  // DOTSTAR_BRG but matrix.Color acts like G, R, B
  matrix.setTextColor(c16(data[1], data[2], data[3]));  
}

// Converts component color to 16-bit color in GRB
uint16_t c16(uint8_t red, uint8_t green, uint8_t blue) {
  uint16_t GRBColor = blue >> 3;
  GRBColor |= (red & 0xFC) << 3;
  GRBColor |= (green  & 0xF8) << 8;
  return GRBColor;
}
#define MAX_BRIGHTNESS 40

void changeBrightness(uint8_t data[], uint16_t len) {
  setBrightness(data[1]);
}

void setBrightness(int newBright) {
  bright = newBright;
  if (bright > MAX_BRIGHTNESS) bright = MAX_BRIGHTNESS; // Limit to maximum brightness
  else if (bright < 0) bright = 0;
  matrix.setBrightness(bright);
}

void pulseLoop() {
  if (pulseOn) {
    int newBright = bright;
    newBright += pulseOffset;
    // Don't min out, change direction
    if (newBright <= PULSE_RATE) {
      newBright = PULSE_RATE;
      pulseOffset = PULSE_RATE;
    }
    else if (newBright >= MAX_BRIGHTNESS) {
      newBright = MAX_BRIGHTNESS;
      pulseOffset = -PULSE_RATE;
    }
    setBrightness(newBright);
  }
}

uint8_t service_id;
unsigned long ts=0;

void setupBLE() {  
  // This doesn't work when the serial port is not connected
  // so in order to get it run on it's own, comment this line out


  //  Initialise the module
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
//    Perform a factory reset to make sure everything is in a known state
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      error(F("Couldn't factory reset"));
    }
  }

  if ( !ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    error( F("Callback requires at least 0.7.0") );
  }
  // Disable command echo from Bluefruit 
  ble.echo(false);

  // Needed for Flora
  //ble.setInterCharWriteDelay(5); // 5 ms

  if (!ble.sendCommandCheckOK("AT+GAPDEVNAME=FloraDress")) {
    Serial.println("Couldn't change device name");
  }

  // Loop a few times trying to connect.
  int loopCount=20;
  service_id = 0;
  while (service_id == 0 && (loopCount-- > 0)) {
    service_id = gatt.addService(0x1888);
    Serial.print("service_id: "); Serial.println(service_id);
    delay(200);
  }

  // Loop a few times trying to connect.
  charid_number = 0;
  loopCount=20;
  while (charid_number == 0 && (loopCount-- > 0)) {
    // Was BLE_DATATYPE_INTEGER
    // Maximum length for configuration is 32
    // Maximum length we seem to be able to send is 20?
    charid_number = gatt.addCharacteristic(0x1777, GATT_CHARS_PROPERTIES_READ | GATT_CHARS_PROPERTIES_NOTIFY | GATT_CHARS_PROPERTIES_BROADCAST | GATT_CHARS_PROPERTIES_WRITE, 1, 20, BLE_DATATYPE_BYTEARRAY);
    Serial.print("characteristic_id: "); Serial.println(charid_number);
    delay(500);
  }

  // Advertise the service 0x88, 0x18
  uint8_t advdata[] { 0x02, 0x01, 0x06, 0x03, 0x02, 0x88, 0x18 };
  loopCount=20;
  while (!ble.setAdvData( advdata, sizeof(advdata)) && (loopCount-- > 0)) {
    delay(500);
  }

  Serial.println("reset()");
  ble.reset();
  delay(200);

  // Set callbacks 
  Serial.println("Setting callbacks");
  ble.setConnectCallback(connected);
  ble.setDisconnectCallback(disconnected);


  // We do it multiple times because sometimes the first time gives an error.
  Serial.println("Setting GATT callback ");
  delay(200);
  ble.setBleGattRxCallback(charid_number, BleGattRX);
  delay(200);
  ble.setBleGattRxCallback(charid_number, BleGattRX);
    
  randomSeed(10);

  ts = millis();
}


void setupMatrix() {
  Serial.println("setup matrix");
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(bright); 
  matrix.setTextColor(matrix.Color(255, 0, 0));  
}

bool enableBLE = true;
void setup() {
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);

  if (enableBLE) setupBLE();
  setupMatrix();
}




// RAINBOW
////////////////

const uint16_t RAINBOW_COLORS[] = {
  // RGB
  matrix.Color(0x00, 0xff, 0x00), // red
  matrix.Color(0x66, 0xff, 0x00), // orange 
  matrix.Color(0xda, 0xff, 0x21), // yellow
  matrix.Color(0xff, 0x00, 0x00), // green 
  matrix.Color(0x00, 0x00, 0xff), // blue
  matrix.Color(0x00, 0x22, 0x66), // indigo
  matrix.Color(0x00, 0xff, 0xff) // violet
};
const int RAINBOW_COLORS_CNT = sizeof(RAINBOW_COLORS)/sizeof(uint16_t);

int rainbowX = 0;

void loopRainbow() {
  matrix.fillScreen(0);
  for (int i=0; i < RAINBOW_COLORS_CNT*2; i++) {
    matrix.drawFastVLine(rainbowX+i, 0, 8, RAINBOW_COLORS[i%RAINBOW_COLORS_CNT]);
  }
  if (++rainbowX > matrix.width()) rainbowX=0;
  matrix.show();
  //delay(50);
}
int matrixWidth = matrix.width();
int matrixHeight = matrix.width();

unsigned int shiftRainbow_offset=0;

const uint8_t RBW_WIDE_WIDTH = 4;
// Tracey Likes
void loopShiftingRainbowWide() {
  // Don't need to fill because we are controlling every pixel.
  for (int x=0; x < matrixWidth; x++) {
    matrix.drawFastVLine(x, 0, 8, RAINBOW_COLORS[((x+shiftRainbow_offset)/RBW_WIDE_WIDTH)%RAINBOW_COLORS_CNT]);
  }
  matrix.show();
  ++shiftRainbow_offset;
}

void loopShiftingRainbowAngle_v1() {
  int off=matrixWidth-1;
  int xmax = matrixWidth+matrixHeight;
  for (int x=0; x < xmax; x++) {
    matrix.drawLine(x, 0, x-off, x+off, RAINBOW_COLORS[((x+shiftRainbow_offset)/RBW_WIDE_WIDTH)%RAINBOW_COLORS_CNT]);
  }
  matrix.show();
  ++shiftRainbow_offset;
}

void loopShiftingRainbowAngle_v2() {
  uint8_t off=matrixWidth-1;
  uint8_t xmax = matrixWidth+matrixHeight;
  uint8_t px;
  for (uint8_t x=0; x < xmax; x++) {
    uint16_t color = RAINBOW_COLORS[((x+shiftRainbow_offset)/RBW_WIDE_WIDTH)%RAINBOW_COLORS_CNT];
    for (uint8_t y=0; y < matrixHeight; y++) {
      px = x-y;
      if (px >= 0 && px < matrixWidth) matrix.writePixel(px, y, color);
    }
  }
  matrix.show();
  ++shiftRainbow_offset;
}



// HEART
/////////////////////////

/// http://greekgeeks.net/#maker-tools_convertColor
static const uint16_t P = 0x04FF;
static const uint16_t R = 0x0700;
// BRG
/* Original design, not used.
static const uint16_t HEART1[]   PROGMEM = {
  P, R, R, P, P, R, R, P,
  R, R, R, R, R, R, R, R,
  R, R, R, R, R, R, R, R,
  R, R, R, R, R, R, R, R,
  R, R, R, R, R, R, R, R,
  P, R, R, R, R, R, R, P,
  P, P, R, R, R, R, P, P,
  P, P, P, R, R, P, P, P  
};
*/

static const uint16_t HEART2[]   PROGMEM = {
  P, P, P, P, P, P, P, P,
  P, P, R, P, P, P, R, P,
  P, R, R, R, P, R, R, R,
  P, R, R, R, R, R, R, R,
  P, R, R, R, R, R, R, R,
  P, P, R, R, R, R, R, P,
  P, P, P, R, R, R, P, P,
  P, P, P, P, P, P, P, P  
};
uint8_t heartOffset=7;
void loopShiftingHeart() {
  //matrix.fillScreen(0);
  
  matrix.drawRGBBitmap(0-heartOffset, 0,HEART2, 8, 8);
  matrix.drawRGBBitmap(8-heartOffset, 0,HEART2, 8, 8);
  matrix.drawRGBBitmap(16-heartOffset, 0,HEART2, 8, 8);
  matrix.drawRGBBitmap(24-heartOffset, 0,HEART2, 8, 8);
  matrix.drawRGBBitmap(32-heartOffset, 0,HEART2, 8, 8);
  if (--heartOffset <= 0) heartOffset=7;
  matrix.show();
}


// SCROLL TEXT
////////////////////////
void grabText(uint8_t data[], uint16_t len) {
  // Grab the remaining chars as text.
  textLength=len-1;
  int i;
  // null it out
  for (i=0; i < 20; i++) text[i] = 0x00;
  // Copy in the text.
  for (i=1; i < len; i++) {
    text[i-1]=data[i];
  }
  Serial.print(text);
}

int scrollTextX = matrix.width();
void setupScrollingText(uint8_t data[], uint16_t len) {
  grabText(data, len);

  // Reset scroll matrix.
  scrollTextX = matrix.width();
}

void loopScrollingText() {
  matrix.fillScreen(0);
  // getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  //Serial.print("Text w,h: "); Serial.print(w); Serial.print(","); Serial.println(h);
  matrix.setCursor(scrollTextX, 0);
  matrix.print(text);
  // make it go faster
  --scrollTextX;
  --scrollTextX; 
  if(--scrollTextX < -w) {
    scrollTextX = matrix.width();
//    if(++pass >= 3) pass = 0;
//    matrix.setTextColor(colors[pass]);
  }
  //Serial.println("loop");
  matrix.show();
}




// POP TEXT
////////////////////////

int popTextOffset=0; // Count into segments of text
int popTextRows=0; // Number of rows found after scanning
char popTextLines[5][7] = { // Max 5 lines of 6 characters plus null term
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};
unsigned long lastShiftMillis=0;
#define SHIFT_MILLIS 800

void setupPopText(uint8_t data[], uint16_t len) {
  grabText(data, len);
  // start below so that we can switch to row 0 on the first pass  
  popTextOffset=-1; 
  lastShiftMillis = millis();

  // Go through text length and split/copy into rows
  int row=0;
  int col=0;
  for (int i=0; i < textLength; i++) {
    char c = text[i];
    if (c == ' ') {
      popTextLines[row][col] = 0x00;
      row++;
      col=0;
    }
    else {
      if (col < 6) {
        popTextLines[row][col] = c;
        col++;
      }
    }
  }
  popTextRows=row+1;
//  Serial.print("popTextRows: "); Serial.print(popTextRows);
//  Serial.print("r[0]: "); Serial.println(popTextLines[0]);
//  Serial.print("r[1]: "); Serial.println(popTextLines[1]);
//  Serial.print("r[2]: "); Serial.println(popTextLines[2]);
//  Serial.print("r[3]: "); Serial.println(popTextLines[3]);
//  Serial.print("r[4]: "); Serial.println(popTextLines[4]);
}

/*
 * int popTextOffset=0; // Count into segments of text
int popTextRows=0; // Number of rows found after scanning
char popTextLines[5][7] = { // Max 5 lines of 6 characters plus null term

 */
void loopPopText() {
  unsigned long currMillis = millis();
  if (currMillis - lastShiftMillis > SHIFT_MILLIS) {
    lastShiftMillis = currMillis;
    if (++popTextOffset >= popTextRows) popTextOffset=0;

    // Draw segment.
    matrix.fillScreen(0);
    char *segText = popTextLines[popTextOffset];
    int16_t x1, y1;
    uint16_t w, h;
    matrix.getTextBounds(segText, 0, 0, &x1, &y1, &w, &h);
    x1 = matrixWidth/2 - w/2;
//    Serial.print("popText: "); Serial.print(segText); Serial.print(" x:"); Serial.println(x1);
//    Serial.print("  w:"); Serial.print(w); Serial.print(" h:"); Serial.println(h);
    matrix.setCursor(x1, 0);
    matrix.print(segText);
    matrix.show();
  
  }

}


// DARK
////////////////////
void dark() {
  matrix.fillScreen(0);
  matrix.show();
}





void loop() {
  // put your main code here, to run repeatedly:
  if (enableBLE) ble.update(20);

  // Pulse if necessary
  pulseLoop();
  
  switch (lightVal) {
  case FUNC_RAINBOW:
    loopShiftingRainbowAngle_v2();
  break;
  case FUNC_HEARTS:
    loopShiftingHeart();
  break;
  case FUNC_SCROLL_TEXT:
    loopScrollingText();
  break;
  case FUNC_POP_TEXT:
    loopPopText();
  break;
  case FUNC_DARK:
  default:
    dark();
  break;
    
  }
}
