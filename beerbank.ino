/*
 * Copyright (c) 2015 FirstBuild
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "chillhub.h"

#define BEER_PIN                     A4
#define BEER_SENSOR_HAS_BEER         0
#define BEER_ALARM_SET               1
#define BEER_ALARM_FALLBACK_DURATION 15000
#define BEER_ALARM_DEBOUNCE_DURATION 1000
#define BEER_BANK_UUID               "072b3293-d637-4f53-a273-edf8c7711d17"
#define BEER_SENSOR_ID               0xB0
#define BEER_ALARM_ID                0xB1
#define BEER_TIME_REMAINING_ID       0xB2
#define dprint(x)
#define dprintln(x)

unsigned char beer_sensor;
unsigned char beer_alarm;
unsigned long beer_sensor_changed;
unsigned long beer_alarm_changed;
unsigned long beer_time_remaining;
chInterface ChillHub;

void beer_tone_bad(void) {
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
  delay(150);
  tone(13, 800, 125);
}

void beer_tone_good(void) {
  tone(13, 2200, 125);
  delay(175);
  tone(13, 4400, 250);
}

void beer_tone_wait(void) {
  tone(13, 1000, 50);
  delay(100);
  tone(13, 1000, 50);
}

void beer_bank_announce(void) {
  ChillHub.setup("beerbanks", BEER_BANK_UUID);
  ChillHub.subscribe(deviceIdRequestType, (chillhubCallbackFunction) beer_bank_announce);
  ChillHub.createCloudResourceU16("beer_sensor", BEER_SENSOR_ID, 0, beer_sensor);
  ChillHub.createCloudResourceU16("time_remaining", BEER_TIME_REMAINING_ID, 0, beer_time_remaining);
  ChillHub.createCloudResourceU16("beer_alarm", BEER_ALARM_ID, 1, beer_alarm);
  ChillHub.addCloudListener(BEER_ALARM_ID, (chillhubCallbackFunction) beer_bank_set_alarm);
}

void beer_bank_set_alarm(unsigned int set) {
  unsigned long now = millis();
  unsigned long wait = now - beer_alarm_changed;
  
  if (wait < BEER_ALARM_DEBOUNCE_DURATION) {
    /* ignore changes we generated */
    ChillHub.updateCloudResourceU16(BEER_ALARM_ID, beer_alarm);
    ChillHub.updateCloudResourceU16(BEER_TIME_REMAINING_ID, beer_time_remaining);
  }
  else if (set) {
    beer_alarm = BEER_ALARM_SET;
    beer_time_remaining = 0;
    beer_alarm_changed = now;
    ChillHub.updateCloudResourceU16(BEER_TIME_REMAINING_ID, beer_time_remaining);
  }
  else {
    beer_alarm = !BEER_ALARM_SET;
    beer_alarm_changed = now;
  }
}

void beer_bank_setup(void) {
  pinMode(BEER_PIN, INPUT_PULLUP);
  
  beer_sensor = digitalRead(BEER_PIN); /* read the initial state of the beer sensor */
  beer_alarm = BEER_ALARM_SET; /* assume the alarm is set until firebase connection is established */
  beer_sensor_changed = millis(); /* assume the value just changed even though it probably happened a while ago */
  beer_alarm_changed = millis(); /* assume the beer alarm was just changed */
  beer_time_remaining = 0; /* there is no time remaining because the alarm is set */
  beer_bank_announce(); /* announce this beerbank to the chillhub */
}

void beer_bank_loop(void) {
  unsigned long now = millis();
  unsigned char current = digitalRead(BEER_PIN);
  
  if (beer_sensor == current) {
    /* the value has not changed so do nothing */
  }
  else {
    beer_sensor = current;
    beer_sensor_changed = now;
    
    dprint("beer sensor changed to ");
    dprint(beer_sensor);
    dprint(" at ");
    dprint(beer_sensor_changed);
    dprintln(" milliseconds");
    
    if (beer_sensor == BEER_SENSOR_HAS_BEER) {
      /* a beer just fell down and is blocking the sensor */
    }
    else if (beer_alarm == BEER_ALARM_SET) {
      dprintln("A beer has been taken and the alarm was set!");
      beer_tone_bad();
    }
    else {
      dprintln("A beer has been taken and the alarm was disabled.");
      beer_alarm = BEER_ALARM_SET;
      beer_alarm_changed = now;
      beer_time_remaining = 0;
      ChillHub.updateCloudResourceU16(BEER_ALARM_ID, beer_alarm);
      ChillHub.updateCloudResourceU16(BEER_TIME_REMAINING_ID, beer_time_remaining);
      beer_tone_good();
    }
    
    ChillHub.updateCloudResourceU16(BEER_SENSOR_ID, beer_sensor);
  }
  
  if (beer_alarm == BEER_ALARM_SET) {
    /* the alarm is set so the beer is secure */
  }
  else {
    unsigned long duration = now - beer_alarm_changed;
    unsigned long seconds_remaining = (BEER_ALARM_FALLBACK_DURATION - duration) / 1000;
    
    if (duration > BEER_ALARM_FALLBACK_DURATION) {
      dprintln("A beer was not taken and the alarm has fallen back to set!");
      beer_alarm = BEER_ALARM_SET;
      beer_alarm_changed = now;
      beer_time_remaining = 0;
      ChillHub.updateCloudResourceU16(BEER_ALARM_ID, beer_alarm);
      ChillHub.updateCloudResourceU16(BEER_TIME_REMAINING_ID, beer_time_remaining);
    }
    else if (beer_time_remaining != seconds_remaining) {
      beer_time_remaining = seconds_remaining;
      ChillHub.updateCloudResourceU16(BEER_TIME_REMAINING_ID, beer_time_remaining);
      beer_tone_wait();
    }
  }

  ChillHub.loop();
}

void setup(void) {
  Serial.begin(9600);
  beer_bank_setup();
}

void loop(void) {
  beer_bank_loop();
}
