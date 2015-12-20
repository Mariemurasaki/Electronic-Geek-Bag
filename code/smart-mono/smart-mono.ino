/*  ----------------------------------------------------------------------------------------
   
   Electronic GeekBag Code - Monochrome version
  
   Code for the Electronic Sewing workshop held a the 32c3
   https://events.ccc.de/congress/2015/wiki/Assembly:Electronic_GeekBag

   It is meant for an Adafruit Gemma and can be programmed using the Adruino IDE.
   The Project home with further descriptions can be found here:
   https://github.com/Mariemurasaki/Electronic-Geek-Bag
                             
   This is the initial code that can be put on a GeekBag with the following main feautres: 
    - Eyes turn on/off depending on light level
    - Eyes are doing random idle animations
    - Battery consumtion optimisation, as far as this is possible on a Gemma               
    - Some stuff is easily configurable (see "User Settings" below)

   ----------------------------------------------------------------------------------------
   
    The MIT License (MIT)
    
    Copyright (c) 2015 Marcus Prinz
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

               
 ---------------------------------------------------------------------------------------- */

#include <avr/power.h>
#include <avr/sleep.h>


// *****************************************************
// ********** Hardware specific settings ***************
// *****************************************************

const int LED_PIN_DIGITAL = 1;                    // 1 = D1
const int LIGHT_SENSOR_PIN_NUMBER_DIGITAL = 2;    // 2 = D2
const int LIGHT_SENSOR_PIN_NUMBER_ANALOG = 1;     // 1 = A1  (WTF? who came up with this numbering ?!?+)



// ***********************************
// ********** User settings **********
// ***********************************
                                                             
const byte GLOBAL_BRIGHTNESS = 255;                           // additional brightness value, to dimm the eyes if they are too bright

const unsigned long RANDOM_ACTION_INTERVAL_MIN = 8000;       // minimum time between animations like blink
const unsigned long RANDOM_ACTION_INTERVAL_MAX = 15000;      // maximum time between animations like blink

// The light value where we switch on/off the eyes.
// We add some hysteresis here to prevent the eyes from flickering at a single on/off point
const int UPPER_LIGHT_VALUE = 220;                           // if OFF, turn ON at light level 200
const int LOWER_LIGHT_VALUE = 180;                           // if ON, turn OFF at light level 180


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// *************************************
// ********** State variables **********
// *************************************

boolean areEyesOn = false;
unsigned long timeForNextAction = millis() + RANDOM_ACTION_INTERVAL_MAX; // just set some initial value here



// ********************************************************
// ********** Sleep-functions for saving Battery **********
// ********************************************************
// This topic is a bit advanced, you probably only want to run delayWithPowersave() and deepSleepWhileBright()
// You could replace both methods by a simple "delay(100)" and everything sill still work, but consumes more battery


// helper method... use the macros below
void sleep_delay_util(const int lowLightLevel, const int interval, const clock_div_t sleepClockSpeed, const boolean omitSensorReading)
{
    int currLightLevel = 0;
    unsigned long wakeTime;

    // enable sleeping capabilities
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    do
    {
        clock_prescale_set(clock_div_2);                            // speed up CPU to normal speed
        currLightLevel = omitSensorReading ? 0 : getLightValue();   // read light value
        wakeTime = millis() + interval;                             // calclate when we have to wake up next 
        clock_prescale_set(sleepClockSpeed);                        // speed down the CPU to the sleep-speed 
        while (millis() < wakeTime)                                 // sleep in a loop as we haven't reached wakeup time
            sleep_mode();                                           // go to sleep (we will be woken a lot by arduino's timer0 (every type it overflows))
    }
    while (currLightLevel > lowLightLevel);                         // we break the loop if the light level goes onder its minimum
                                                                    // if we only want to wait without checking light, we set defaults that will allways break the loop
                                                                    
    clock_prescale_set(clock_div_2);                                // speed up CPU to normal speed
    sleep_disable();
}


// replacement for "delay( milliseconds )" that will save about 3mA 
#define delayWithPowersave(delayTime)                    (sleep_delay_util( 32000, (delayTime), clock_div_2, true))


// waits for the brightness to drop below a certain level, and checks it in intervalls, saves about 4mA
#define deepSleepWhileBright(lowLightLevel,interval)     (sleep_delay_util( (lowLightLevel), (interval)/32, clock_div_64, false))



// ******************************************************
// ********** Methods to read the light sensor **********
// ******************************************************


// Measures the light-level on the sensor
// returns the average value of 10 measurements (value is betwen 0 an 1024)
int getLightValue()
{
    int lightValue = 0;
    power_adc_enable();

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

    power_adc_disable();
    return lightValue / 10;
}



// *****************************************************************************************
// ********** LED focused methods that are working with normal LEDs and Neopixels **********
// *****************************************************************************************


// set the current brightness of both eyes
void setStandardEyeBrightness(byte brightness)
{
    unsigned int compensated = brightness;
    compensated = (compensated * GLOBAL_BRIGHTNESS) >> 8;
    analogWrite(LED_PIN_DIGITAL, compensated);
}


// fade both eyes from one brightness to another
void fadeEyes(byte fromBrighness, byte toBrightness, byte steps, int stepDelay)
{
    for (int i = 0; i <= steps; i++)
    {
        setStandardEyeBrightness(((toBrightness * i) + (fromBrighness * (steps - i))) / steps);
        delay(stepDelay);
    }
}


// Animate eyes going Hypno (normal LED version)
void playAnimHypnoCat(int intervalTime, int duration)
{
    int intervals = duration / intervalTime;
    for (int i=0; i<intervals; i++)
    {
        fadeEyes(255, 0, 10, intervalTime / 20);
        fadeEyes(0, 255, 10, intervalTime / 20);
    }
}


// play a single eye-blink
void playAnimBlink(int duration)
{
    fadeEyes(255, 0, 10, duration / 25);
    delay(duration / 5);
    fadeEyes(0, 255, 10, duration / 25);
}


// play an eye-opening animation
// because this is a quite long animation, we check the light level in between so we can end it early if it gets light again
void playAnimOpenEyes()
{
    fadeEyes(0, 255, 30, 50);
    setStandardEyeBrightness(255);
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


// play an eye-closing animation
void playAnimCloseEyes()
{
    fadeEyes(255, 0, 30, 50);
    setStandardEyeBrightness(0);
}


// play a test-animation at bootup
void playAnimBoot()
{
    setStandardEyeBrightness(255);
    delay(500);
    setStandardEyeBrightness(30);
    delay(500);
    setStandardEyeBrightness(255);
    delay(500);
    setStandardEyeBrightness(0);
    delay(500);
}



// ***************************************************
// ********** Methods focused on scheduling **********
// ***************************************************


// choose which animation should be playing based on probability
void doRandomAction()
{
    int percentage = random(0, 100);

    if (percentage < 65)        // probability 65%
    {
        // normal blink
        playAnimBlink(random(120, 150));
    }
    else if (percentage < 90)   // probability 25%
    {
        // long blink
        playAnimBlink(random(1000, 2000));
    }
    else                        // probability 10%
    {
        // fading cycle
        playAnimHypnoCat(random(300, 1500), random(4000, 7000));
    }
}


// check if an animation is due
void checkForRadomAction()
{
    if (millis() > timeForNextAction)
    {
        // if an animation is scheduled to play, play it and schedule the next one
        doRandomAction();
        timeForNextAction = millis() + random(RANDOM_ACTION_INTERVAL_MIN, RANDOM_ACTION_INTERVAL_MAX);
    }
}



// ******************************************
// ********** Main Arduino methods **********
// ******************************************


void setup()
{
    power_all_disable();    // disable ALL the power
    power_timer0_enable();  // ... except timer0 (which we Arduino uses for tracking time)
                            // ... and Analog-Digital-Conversion (which we need for measuring the sensor), 
                            //     but we only enable it where we actually measure stuff (see: getLightValue())

    // We have to set the analog-pin as an input (and use the digita-number of the same pin for it... don't ask... -_- )
    pinMode(LIGHT_SENSOR_PIN_NUMBER_DIGITAL, INPUT);

    // We initialize the random-generator using the light sensor
    // you usually use an unconnected pin for something like that, but we only have one analog pin
    randomSeed(analogRead(LIGHT_SENSOR_PIN_NUMBER_ANALOG));

    // initialize LED-Pin
    pinMode(LED_PIN_DIGITAL, OUTPUT);

    // and play a boot-animation to just see that we turned the thing on.
    playAnimBoot();
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
        timeForNextAction = millis() + random(RANDOM_ACTION_INTERVAL_MIN, RANDOM_ACTION_INTERVAL_MAX);
        delay(50);
    }
    else if (areEyesOn && currentLightValue > UPPER_LIGHT_VALUE)
    {
        // if lights are currently ON, but it's too bright -> play the turn-off animation and set new state
        playAnimCloseEyes();
        areEyesOn = false;
        delay(50);
    }
    else if (areEyesOn)
    {
        // if lights are just ON, we'll check whether we should run some random action (blinkin, winking, etc.)
        checkForRadomAction();

        // just like normal delay() but we enter IDLE mode to save (a bit of battery)
        // this saves about 3mA
        delayWithPowersave(100);
    }
    else
    {
        // LEDs are off and should be.
        // we do some extended power saving, where we reduce the clock speed
        // we could just do a standard "delay" here, but we can save a few mA actually doing something more fancy
        // NOTE: what we do here messes up the system time (runs way slower than it should), so we only use it when the LEDs are off
        // this saves about 4mA
        deepSleepWhileBright(LOWER_LIGHT_VALUE, 200);
        delay(50);
    }
}


