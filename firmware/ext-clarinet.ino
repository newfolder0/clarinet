//**********************************************************************************|
#include "application.h"
#include "MMA8452-Accelerometer-Library-Spark-Core/MMA8452-Accelerometer-Library-Spark-Core.h"
#include "MAG3110-magnetometer-library.h"
#include "neopixel/neopixel.h"

/*
 * Extended Clarinet Project
 * Spark Core Device Code
 *
 * Author: Peter Bell
 *
 * Spark:
 * ------
 * D0 - SDA (both accelerometer and magnetometer)
 * D1 - SCL (both accelerometer and magnetometer)
 * D2 - touch button 0
 * D3 - touch button 1
 * D4 - touch button 2
 * D5 - touch button 3
 * D6 - touch button 4
 * A0 - 'calibrate' button
 * A7 - neopixel data
 * 3.3 - touch button VCC
 * 3.3* - (cleaner supply) VCC, both accelerometer and magnetometer
 * VIN - neopixel+ (Vin)
 * GND - everything
 *
 * User defined variables:
 *  ACCEL_SCALE - accelerometer scaling
 *  latch[] - choose whether buttons latch on/off
 *  brightness - set LED (neopixel strip) brightness in percent
 *  rainbowSpeed - set rainbow animation speed(0 - 255)
 *  loopDelay - use for debugging, if unstable
 *  targetIP - default IP to send to
 *  outPort - default port out
 *
 *
 * Other notes:
 * ------------
 * User can modify both outPort and targetIP with HTTP requests, with relevant parameters;
 *      setTargetIP
 *      setOutPort
 *
 * For Example: curl https://api.spark.io/v1/devices/0123456789abcdef01234567/setTargetIP -d access_token=1234123412341234123412341234123412341234 -d "args=10.42.0.64"
 *
 * User can request the final byte of localIP and targetIP or outPort with HTTP requests;
 *      localIPTail
 *      targetIPTail
 *      port
 *
 * Only one touch button can be pressed at a time due to touch
 * breakout board limitation.
 *
 * Spark UDP implementation is buggy and requires a lot of work arounds.
 * Here is some information: https://community.spark.io/t/udp-issues-and-workarounds/4975
 */

// options - defined as global variables or constants
#define LED_PIN     D7
#define BUTTON_0    D2
#define BUTTON_1    D3
#define BUTTON_2    D4
#define BUTTON_3    D5
#define BUTTON_4    D6
#define BUTTON_5    A0

// Neopixels
// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN   A7
#define PIXEL_COUNT 28
#define PIXEL_TYPE  WS2812B
#define COLOUR_CYCLE_BUTTON 0

// Accelerometer:
// scale: SCALE_2G, SCALE_4G, or SCALE_8G
// data rate: ODR_800, ODR_400, ODR_200, ODR_100, ODR_50, ODR_12,
// ODR_6, or ODR_1
#define ACCEL_SCALE     SCALE_2G
#define ACCEL_DATA_RATE ODR_800

// note: sendUDP method should empty received buffers before sending but just in
//      case, keep incoming and outgoing ports different. If the spark receives its own
//      outgoing packets it will fill up its incoming buffers and crash.
// These could be const but for come reason couldn't be exposed to the web API
int inPort = 7001;
int outPort = 7000;

//Likely IPs when using 'spare' network.
// Robert desktop: 101
// Robert laptop: 100
// Pete laptop: 103 or 81 on mintymac
// Robert Pite performance: 52
int targetIPTail = 100; // Robert desktop: 101, Robert performance: 52, Pete laptop: 81 ###########################################################################################
int localIPTail;
IPAddress targetIP;

// loop delay in milliseconds, crashes if too short. 10 is good.
const int loopDelay = 10;
const bool delayOn = true;

// initialise button arrays
int buttonPin[] = {BUTTON_4, BUTTON_3, BUTTON_2, BUTTON_1, BUTTON_0, BUTTON_5};
bool button[] = {false, false, false, false, false, false};
bool holdButton[] = {false, false, false, false, false, false};    // previous states
// choose buttons momentary (only on while still pressed) or latching (press once for on, once again for off)
const bool latch[] = {false, true, true, true, true, false};

MMA8452Q accel; // initialise accelerometer
//MAG3110 magnet; // initialise magnetometer
UDP udp;        // initialise UDP

// hardcoded set up for accel data send buffer - should be generated more dynamically, see comment block below
byte sendBuffer[]  = {'l','i','s','t',0,0,0,0,  ',','i', 'i','i','i', 'i','i','i', 0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0};

// if concatenating arrays was easy this could be broken down like below:
/*byte OSCHeader[]  = {'l','i','s','t',0,0,0,0,  ',','i', 'i','i','i', 'i','i','i', 0,0,0,0}; // prefix
byte OSCButtons[] = {0,0,0,0};                      // buttons
byte OSCAccel[]   = {0,0,0,0,  0,0,0,0,  0,0,0,0};  // accelerometer
byte OSCMagnet[]  = {0,0,0,0,  0,0,0,0,  0,0,0,0};  // magnetometer
byte sendBuffer[] = OSCHeader + OSCButtons + OSCAccel + OSCMagnet;  */

// this is the index of the first byte of the arguments (atual data section at the
// end of the packet) in the sendBuffer (Max OSC packet)
uint8_t dataIndex = 20;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);
const uint8_t MAX_BRIGHTNESS = 50;   // maximum brightness allowed
uint8_t brightness = 50;        // set initial neopixel brightness in percent
uint8_t rainbowState = 0;       // initial rainbow state - relevant to some LED methods
uint8_t rainbowSpeed = 1;       // configurable rainbow speed, 0 to 255
bool prevColourButton = false;  // previous state of button to change colour


//**********************************************************************************|
void setup() {
    // set up button pins - LOW is pressed
    for (uint8_t i = 0; i < arraySize(buttonPin); i++) pinMode(buttonPin[i], INPUT_PULLUP);

    pinMode(LED_PIN, OUTPUT);   // configure onboard LED pin
    digitalWrite(LED_PIN, LOW); // ensure it is turned off

    // initialise neopixels
    pixels.begin();
    pixels.setBrightness(brightness);
    pixels.show();   // initialise all pixels to 'off'

    // config wifi/IPs, begin serial and udp communication
    Serial.begin(9600); // baud rate = 9600 - DEBUG
    localIPTail = WiFi.localIP()[3];
    targetIP = WiFi.localIP();
    targetIP[3] = targetIPTail;

    WiFi.ping(WiFi.gatewayIP());    // addressing another UDP bug - can't send
                                    // anything until you've pinged someone (wtf?)
    udp.begin(inPort);

    // initalise accelerometer and magnetometer
    accel.init(ACCEL_SCALE, ACCEL_DATA_RATE);
  //  magnet.init(SYSMOD_ACTIVE_RAW);

    Spark.variable("localIPTail", &localIPTail, INT);
    Spark.variable("targetIPTail", &targetIPTail, INT);
    Spark.variable("port", &outPort, INT);
    Spark.variable("brightness", &brightness, INT);

    Spark.function("setTargetIP", setTargetIP);
    Spark.function("setPort", setOutPort);
    Spark.function("brightness", setBrightness);
    Spark.function("setIPTail", setTargetIPTail);

    // Flash a few times to signal end of setup
    int flashes = 3;    // number of flashes
    int duration = 100; // flash duration and delay in milliseconds
    for (int i = 0; i < flashes; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(duration);
        digitalWrite(LED_PIN, LOW);
        delay(duration);
    }

//    if (!Spark.connected) targetIP = IPAddress(192,168,0,103);
}


//**********************************************************************************|
void loop() {
    readAccel();    // read accelerometer registers
 //   readMagnet();   // read magnetometer registers
    readButtons();  // read button inputs
    dataToMax();    // convert to Max-but-not-quite-OSC string, store in sendBuffer

    // if wifi is ready, send accelerometer data over UDP
    if (WiFi.ready()) sendUDP();

    // LED stuff
//    rainbow();
//    pixels.setPixelColor(13, 255, 255, 255);
//    brightnessButtons();
    colourButton();
    updatePixels();

    // update cloud variables
    localIPTail = WiFi.localIP()[3];
    targetIPTail = targetIP[3];
//    targetIP[3] = targetIPTail;

    if (delayOn) delay(loopDelay);

//    Serial.println(WiFi.localIP());
}


//**********************************************************************************|

int setTargetIPTail(String args) {
    int newTail = args.toInt();
    targetIP[3] = newTail;
    targetIPTail = newTail;
    return  targetIPTail;
}

// set target IP for UDP packets when called by HTTP request
int setTargetIP(String args) {
    Serial.println(args); //DEBUG

    IPAddress newIP = parseIP(args);
    Serial.println(newIP);
    targetIP = newIP;
    targetIPTail = newIP[3];
    return targetIP[3];
}

// parse IPAddress from string
IPAddress parseIP(String args) {
    uint8_t ipInts[4];

    for(unsigned int i = 0; i < arraySize(ipInts); i++) {
        int delimiter = args.indexOf(',' || '.');   // find first , or . delimiter
        ipInts[i] = args.substring(0, delimiter).toInt();   // get number part
        String newArgs = args.substring(delimiter + 1);   // trim from IP string
        args = newArgs;
    }

    IPAddress newIP = {ipInts[0], ipInts[1], ipInts[2], ipInts[3]};
    return newIP;
}

// set port to send UDP packets to
int setOutPort(String args) {
    outPort = args.toInt();
    return  outPort;
}

// read values from accelerometer
void readAccel() {
    if (accel.available()) accel.read();
}

// read values from magnetometer
void readMagnet() {
//    magnet.read();
}

// read touch button inputs and save to global variables
void readButtons() {
    for (unsigned int i = 0; i < arraySize(button); i++) {  // for each button
        bool pressed = !digitalRead(buttonPin[i]);

        if (!latch[i]) {    // if not latching
            // button state equals if pressed
            button[i] = pressed;
        } else {   // if latching
            // if pressed but not before (rising edge), toggle state
            if (pressed && !holdButton[i]) {
                if (button[i]) button[i] = false;
                else button[i] = true;
            }

            holdButton[i] = pressed;
        }
    }
}

// convert accelerometer data to Max UDP string, store in global sendBuffer
// This is pretty crude/brute-force-y and could be significantly improved
// by making it dynamically adjust packet size and header to suit sendBuffer
void dataToMax() {
    // change axes to match standard convention
    int ax = 4095-accel.y;
    int ay = 4095-accel.x;
    int az = 4095-accel.z;

    int mx = 65536;//-magnet.mx;
    int my = 65536;//-magnet.my;
    int mz = 65536;//-magnet.mz;

    // turn button data into byte
    byte buttonByte = 0;
    for (unsigned int i = 0; i < arraySize(button); i++) {  // for each button
        if (button[i]) buttonByte++;    // if button pressed, flip LSB
        // if not last button, bit shift left
        if (i < arraySize(button) - 1) buttonByte <<= 1;
    }

    // break accelerometer data into bytes
    byte axMSB = ax >> 8;
    byte axLSB = ax;

    byte ayMSB = ay >> 8;
    byte ayLSB = ay;

    byte azMSB = az >> 8;
    byte azLSB = az;

    // break accelerometer data into bytes
    byte mxMSB = mx >> 8;
    byte mxLSB = mx;

    byte myMSB = my >> 8;
    byte myLSB = my;

    byte mzMSB = mz >> 8;
    byte mzLSB = mz;

    // put all data in send buffer
    sendBuffer[dataIndex + 3] = buttonByte;

    sendBuffer[dataIndex + 6] = axMSB;
    sendBuffer[dataIndex + 7] = axLSB;
    sendBuffer[dataIndex + 10] = ayMSB;
    sendBuffer[dataIndex + 11] = ayLSB;
    sendBuffer[dataIndex + 14] = azMSB;
    sendBuffer[dataIndex + 15] = azLSB;

    sendBuffer[dataIndex + 18] = mxMSB;
    sendBuffer[dataIndex + 19] = mxLSB;
    sendBuffer[dataIndex + 22] = myMSB;
    sendBuffer[dataIndex + 23] = myLSB;
    sendBuffer[dataIndex + 26] = mzMSB;
    sendBuffer[dataIndex + 27] = mzLSB;
}

// function to send a UDP packet.
// note: currently, udp.write() sends immediately but pretend it is buffered to be
// sent at udp.endPacket() because this is expected behaviour in the future.
void sendUDP() {
//    digitalWrite(LED_PIN, HIGH); // DEBUG - onboard LED on for beginning of tranmission

    // read incoming buffer to avoid filling it up - for bug in UDP implementation
    int32_t packetSize = udp.parsePacket();
    unsigned char tempBuffer[packetSize];
    //Dump any packets RX
    while (packetSize > 0) {
        udp.read(tempBuffer, packetSize >= 12 ? 12 : packetSize);
        packetSize = udp.parsePacket();
    }

    udp.beginPacket(targetIP, outPort);
    udp.write(sendBuffer, arraySize(sendBuffer));
    udp.endPacket();

//    digitalWrite(LED_PIN, LOW); // DEBUG - switch onboard LED off at end of transmission
}

// update LED strip
void updatePixels() {
    uint8_t newBrightness = ((float)MAX_BRIGHTNESS/100)*brightness*(255/100);
    pixels.setBrightness(newBrightness);
    pixels.show();
}

int setBrightness(String args) {
    brightness = args.toInt();
    return brightness;
}

// function for choosing a colour on a wheel with a single byte.
// input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    if(WheelPos < 85) {
        return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
        WheelPos -= 85;
        return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}

// set colour using a button ************************************************************
// - bit of a kludge
void colourButton() {
    byte number = 6;
    byte segment = 255 / number;
    uint32_t currentColour;

    if (button[COLOUR_CYCLE_BUTTON] && !prevColourButton) {
        prevColourButton = true;
        rainbowState++;
        if (rainbowState > number) rainbowState = 0;

        // choose colour
        if (rainbowState == 0) currentColour = pixels.Color(0,0,0);
        else {
            currentColour = Wheel((rainbowState-1)*segment & 255);
        }

        // set all pixels
        for(uint16_t i=0; i<pixels.numPixels(); i++) {
            pixels.setPixelColor(i, currentColour);
        }
    } else if (!button[COLOUR_CYCLE_BUTTON] && prevColourButton) prevColourButton = false;
}

// obsolete but possibly useful code for controlling LED brightness with buttons
// and animating a rainbow
/*
// Neopixel rainbow maker
void rainbow() {
    uint16_t i;
    uint8_t segments = 255 / pixels.numPixels();

    for(i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel((segments*i+rainbowState) & 255));
    }

    rainbowState += rainbowSpeed;
}

// set brightness with touch buttons
void brightnessButtons()
{
    if (button[0]) brightness = 1;  // min brightness without off
    if (button[1] && brightness > 1) brightness -= 1;  // decrease brightness
    if (button[2]) {    // if on, switch off - if off, switch to 50
        if (brightness > 0) brightness = 0;
        else brightness = 50;
    }
    if (button[3] && brightness < 100) brightness += 1;  // increase brightness
    if (button[4]) brightness = 100;    // set brightness
}*/

//DEBUG
/*
void printBits(byte myByte){
 for(byte mask = 0x80; mask; mask >>= 1){
   if(mask  & myByte)
       Serial.print('1');
   else
       Serial.print('0');
 }
}
*/
