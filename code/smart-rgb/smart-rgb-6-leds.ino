
#include <FastLED.h>
#include <avr/power.h>
#include <avr/sleep.h>


// *****************************************************
// ********** Hardware specific settings ***************
// *****************************************************

const int LED_PIN_DIGITAL = 1;                    // 1 = D1
const int LIGHT_SENSOR_PIN_NUMBER_DIGITAL = 2;    // 2 = D2
const int LIGHT_SENSOR_PIN_NUMBER_ANALOG = 1;     // 1 = A1  (WTF? who came up with this numbering ?!?+)
const int EYE_NUM_1 = 4;                          //Value ob the position of the EYE_LEDS in the LED-Array
const int EYE_NUM_2 = 5;
const int FOOT_NUM_1 = 0;                          //Value ob the position of the EYE_LEDS in the LED-Array
const int FOOT_NUM_2 = 1;
const int FOOT_NUM_3 = 2;                          //Value ob the position of the FOOT_LEDS in the LED-Array
const int FOOT_NUM_4 = 3;
const int NUM_LEDS = 6;

// ***********************************
// ********** User settings **********
// ***********************************

const byte BASE_EYE_COLOR_H = 200;   // the eye-color of the ghost in angular for the weal (Hue)
const byte BASE_EYE_COLOR_S = 255;   // Saturation
                                                            
const byte BASE_FOOT_COLOR_H = 200;   // the foot-color of the ghost in angular for the weal (Hue)
const byte BASE_FOOT_COLOR_S = 0;    // Saturation
                                                             
const byte GLOBAL_BRIGHTNESS = 60;                           // additional brightness value, to dimm the eyes if they are too bright (value beteen 0 and 255) (Value)
const byte GLOBAL_BRIGHTNESS_FOOT = 100;                      // a brightness 60 with one color usually consumes ~13mA when ON. (the idle-consumption is about 6mA)

const unsigned long RANDOM_ACTION_INTERVAL_MIN = 8000;       // minimum time between animations like blink
const unsigned long RANDOM_ACTION_INTERVAL_MAX = 15000;      // maximum time between animations like blink

// The light value where we switch on/off the eyes.
// We add some hysteresis here to prevent the eyes from flickering at a single on/off point
const int UPPER_LIGHT_VALUE = 220;                           // if OFF, turn ON at light level 200
const int LOWER_LIGHT_VALUE = 180;                           // if ON, turn OFF at light level 180

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////


// *************************************
// ********** State variables **********
// *************************************

boolean areEyesOn = false;
boolean footState = false;
unsigned long timeForNextAction = millis() + RANDOM_ACTION_INTERVAL_MAX; // just set some initial value here

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<NEOPIXEL, LED_PIN_DIGITAL>(leds, NUM_LEDS);

}

void loop()
{
    // we read the light values
    int currentLightValue = getLightValue();

    // and decide between 4 cases:
    //   - turn on
    //   - turn off
    //   - leave on
    //   - leave off
    if (!areEyesOn && currentLightValue < LOWER_LIGHT_VALUE)
    {
        // if lights are currently OFF, but it's dark enough -> play the turn-on animation and set new state
        playAnimOpenEyes();
        areEyesOn = true;

        // we also have to reset the time for the next random action, otherise this will happen immediately
        delay(50);
    }
    else if (areEyesOn && currentLightValue > UPPER_LIGHT_VALUE)
    {
        // if lights are currently ON, but it's too bright -> play the turn-off animation and set new state
        leds[FOOT_NUM_1] = CRGB::Black;
        leds[FOOT_NUM_2] = CRGB::Black;
        leds[FOOT_NUM_3] = CRGB::Black;
        leds[FOOT_NUM_4] = CRGB::Black;
        playAnimCloseEyes();
        areEyesOn = false;
        delay(50);
    }
    else if (areEyesOn)
    {
        // if lights are just ON, we'll check whether we should run some random action (blinkin, winking, etc.)
       playAFootChange();

        // just like normal delay() but we enter IDLE mode to save (a bit of battery)
        // this saves about 3mA
        //delayWithPowersave(100);
        delay(100);
    }
    else
    {
        // LEDs are off and should be.
        // we do some extended power saving, where we reduce the clock speed
        // we could just do a standard "delay" here, but we can save a few mA actually doing something more fancy
        // NOTE: what we do here messes up the system time (runs way slower than it should), so we only use it when the LEDs are off
        // this saves about 4mA
        delay(50);
    }
}

// ******************************************************
// ********** Methods to read the light sensor **********
// ******************************************************


// Measures the light-level on the sensor
// returns the average value of 10 measurements (value is betwen 0 an 1024)
int getLightValue()
{
    int lightValue = 0;

    // averaging 10 analog measurements
    // returns between 0 and 10240 (so it still fits a 16-bit integer)
    for (int i = 0; i < 10; i++)
    {
        lightValue += analogRead(LIGHT_SENSOR_PIN_NUMBER_ANALOG);
        
        // Note we'll try read this very fast.
        // We will be woken up to perform this task all the time and want to go back to sleep as soon as possible.
        // going below 300 causes issues though, so i just took 500 to be on the safe side
        delayMicroseconds(500);    
    }
    return lightValue / 10;
}


// *****************************************************************************************
// ********** LED focused methods that are working with normal LEDs and Neopixels **********
// *****************************************************************************************
 
// fade both eyes from one brightness to another
void fadeEyes(byte fromBrighness, byte toBrightness, byte steps, int stepDelay)
{
    for (int i = 0; i <= steps; i++)
    {
        setEyeBrightness(((toBrightness * i) + (fromBrighness * (steps - i))) / steps);
        delay(stepDelay);
    }
}

// sets the eye brightness with the standart color
void setEyeBrightness(byte newBrightness)
{
    updateEyeColor(CHSV(BASE_EYE_COLOR_H,BASE_EYE_COLOR_S,newBrightness), CHSV(BASE_EYE_COLOR_H,BASE_EYE_COLOR_S,newBrightness));
}

// updates the color for the 2 eyes.
void updateEyeColor(CHSV newColorRight, CHSV newColorLeft)
{
    leds[EYE_NUM_1] = newColorRight;
    leds[EYE_NUM_2] = newColorLeft;
    FastLED.show();
}

// play an eye-opening animation
// because this is a quite long animation, we check the light level in between so we can end it early if it gets light again
void playAnimOpenEyes()
{
    fadeEyes(0, 255, 30, 50);
    delay(200);
    if (getLightValue() > UPPER_LIGHT_VALUE)
      return;

    playAnimBlink(300);
    delay(400);
    if (getLightValue() > UPPER_LIGHT_VALUE)
      return;
      
    playAnimBlink(100);
    delay(150);
    if (getLightValue() > UPPER_LIGHT_VALUE)
      return;
      
    playAnimBlink(100);
}

// play a single eye-blink
void playAnimBlink(int duration)
{
    fadeEyes(255, 0, 10, duration / 25);
    delay(duration / 5);
    fadeEyes(0, 255, 10, duration / 25);
}

// play an eye-closing animation
void playAnimCloseEyes()
{
    fadeEyes(255, 0, 30, 50);
}

//play a foot change
void playAFootChange()
{
    if(footState)
    {
        footState = false;
        leds[FOOT_NUM_1] = CRGB::Black;
        leds[FOOT_NUM_3] = CRGB::Black;
        leds[FOOT_NUM_2] = CHSV(BASE_FOOT_COLOR_H,BASE_FOOT_COLOR_S,GLOBAL_BRIGHTNESS_FOOT);
        leds[FOOT_NUM_4] = CHSV(BASE_FOOT_COLOR_H,BASE_FOOT_COLOR_S,GLOBAL_BRIGHTNESS_FOOT);
    }
    else
    {
        footState = true;
        leds[FOOT_NUM_1] = CHSV(BASE_FOOT_COLOR_H,BASE_FOOT_COLOR_S,GLOBAL_BRIGHTNESS_FOOT);
        leds[FOOT_NUM_3] = CHSV(BASE_FOOT_COLOR_H,BASE_FOOT_COLOR_S,GLOBAL_BRIGHTNESS_FOOT);
        leds[FOOT_NUM_2] = CRGB::Black;
        leds[FOOT_NUM_4] = CRGB::Black;
    }
    FastLED.show();
}
