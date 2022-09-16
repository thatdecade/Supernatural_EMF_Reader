/*
   Prop EMF Reader, based on tv show Supernatural
   Author: Dustin Westaby
   Date: July 2015

   Uses audio fx board from adafruit.
   https://learn.adafruit.com/adafruit-audio-fx-sound-board/

   Build Pictures:
   https://www.flickr.com/photos/dwest2/albums/72157640062702294

*/


#include <EEPROM.h>

/* ----------------------------------
    Pin Defines
   ---------------------------------- */
// Analog meter output, use potentiometer in series to trim down the max needle position
#define METER_PIN 11
#define MAX_METER_OUTPUT 100

// Two or three position toggle switch for mode selection
#define TOGGLE_LEFT_PIN 9
#define TOGGLE_RIGHT_PIN 8

// Button triggers effects
#define HIDDEN_BUTTON_PIN 12
#define HIDDEN_LED_PIN 13

// LEDs swing with meter position
byte LED_PIN_MAP[6] =
{
  2,   // LED 1
  3,   // LED 2
  4,   // LED 3
  5,   // LED 4
  6,   // LED 5
  HIDDEN_LED_PIN
};

/* ----------------------------------
    Globals
   ---------------------------------- */
//audio clip selection is stored in eeprom
byte audio_clip_selected = 1;
#define MAX_AUDIO_CLIPS 5

//initial data for globals
byte left_toggle_state = LOW;
byte right_toggle_state = LOW;
byte hidden_button_powerup_state = LOW;
unsigned long lastEvent = 0;
byte secret_waiting = false;

/* ----------------------------------
    Source Code
   ---------------------------------- */
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);

  /* Start with LEDs off (outputs) */
  for ( byte i=0; i < sizeof(LED_PIN_MAP); i++)
  {
    pinMode(LED_PIN_MAP[i], OUTPUT);
    digitalWrite(LED_PIN_MAP[i], LOW);
  }

  pinMode(METER_PIN, OUTPUT);
  pinMode(TOGGLE_LEFT_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_RIGHT_PIN, INPUT_PULLUP);
  pinMode(HIDDEN_BUTTON_PIN, INPUT_PULLUP);

  //retrieve and validate data from eeprom
  audio_clip_selected = EEPROM.read(0);
  if( ( audio_clip_selected == 0 ) ||
      ( audio_clip_selected > MAX_AUDIO_CLIPS ) )
  {
     audio_clip_selected = 1;

     //fix eeprom
     EEPROM.write(0, audio_clip_selected);

    //indicate EEPROM was reset (LED stays on until button is pressed)
    digitalWrite(HIDDEN_LED_PIN, HIGH);
  }

  //Hidden button is read during a power cycle to determine alternate mode request
  hidden_button_powerup_state = digitalRead(HIDDEN_BUTTON_PIN);

  //chirp LED to confirm button pressed during startup
  if(hidden_button_powerup_state == LOW)
  {
    digitalWrite(HIDDEN_LED_PIN, HIGH);
    delay(200);
    digitalWrite(HIDDEN_LED_PIN, LOW);
  }
}

void loop() {

  //Check toggle position every loop
  left_toggle_state  = digitalRead(TOGGLE_LEFT_PIN);
  right_toggle_state = digitalRead(TOGGLE_RIGHT_PIN);

  if(hidden_button_powerup_state == HIGH)
  {
    if(left_toggle_state == LOW)
    {
      //Primary Left Mode
      forward_quiet();
      power_on_indicator();
      secret_waiting = false;
    }
    else if(right_toggle_state == LOW)
    {
      //Primary Right Mode
      prop_mode_single(1); //runs on trigger only
      power_on_indicator();
      secret_waiting = false;
    }
    else
    {
      //Primary Center Mode (secret)
      if(secret_ready())
      {
        select_sound();
      }
    }
  }
  else
  {
    if(left_toggle_state == LOW)
    {
      //Secondary Left Mode
      reverse_quiet();
      power_on_indicator();
      secret_waiting = false;
    }
    else if(right_toggle_state == LOW)
    {
      //Secondary Right Mode
      prop_mode_single(0); //no trigger, always runs
      power_on_indicator();
      secret_waiting = false;
    }
    else
    {
      //Secondary Center Mode (secret)
      if(secret_ready())
      {
        lamp_test();
      }
    }
  }


}

/* ******************************************************************************
 * Setup Functions
 ****************************************************************************** */

void power_on_indicator()
{
  //Power ON indicator
  //blink the bottom LED for every 10 seconds without button press
  if(millis() - lastEvent > 10000)
  {
    bargraph_only(1); //50ms chirp
    bargraph_only(0);

    //reset timeout
    lastEvent = millis();
  }
}

byte secret_ready()
{
  //Do not run secret mode when toggle is flipped and momentarily hits center.
  //  Only run if it is held in the center for a while
  if (secret_waiting == false)
  {
    //toggle switch hit center, reset the timer
    lastEvent = millis();
    secret_waiting = true;
  }

  if(millis() - lastEvent > 1000)
  {
    //allow the secret mode, but do not reset the timer
    return 1;
  }
  else
  {
    return 0;
  }
}

void select_sound()
{
  static byte blink_counter = 0;

  bargraph_only(audio_clip_selected);

  if (digitalRead(HIDDEN_BUTTON_PIN) == HIGH)
  {
    //button not pressed

    //blink LED
    blink_counter++;
    digitalWrite(HIDDEN_LED_PIN, 1 & blink_counter);
  }
  else //button pressed
  {
    //next audio clip
    audio_clip_selected++;
    if( ( audio_clip_selected == 0 ) ||
        ( audio_clip_selected > MAX_AUDIO_CLIPS ) )
    {
       audio_clip_selected = 1;
    }

    //save selection
    EEPROM.write(0, audio_clip_selected);

    //play audio and display number
    Serial.println("q"); //stop audio (if playing)
    delay(25);
    play_audio();
    bargraph_only(audio_clip_selected);

    //wait for button release
    delay(100);
    while(!digitalRead(HIDDEN_BUTTON_PIN));
  }
}

void lamp_test()
{
  //Turns on each LED in sequence to ensure everything is working
  //  Also tests the meter (stays at max for 3 seconds to allow adjustment)

  for ( byte i=0; i < 6; i++)
  {
    bargraph_and_meter(i);
    delay(1000);
  }
  digitalWrite(LED_PIN_MAP[5], HIGH);
  delay(1000);
  digitalWrite(LED_PIN_MAP[5], LOW);
  delay(1000);
  Serial.println("#0"); //play sound bank 0
}

/* ******************************************************************************
 * Prop Functions
 ****************************************************************************** */

void prop_mode_single(byte trigger_enable)
{
  byte button_read;

  //pull seed from right side potentiometer
  randomSeed(analogRead(1));

  if(trigger_enable == true)
  {
    button_read = digitalRead(HIDDEN_BUTTON_PIN);
  }
  else
  {
    button_read = LOW;
  }

  if(button_read == LOW)
  {
    play_audio();
    bargraph_and_meter(5);
    delay(100);

    for( byte i=0; i<7; i++)
    {
      bargraph_and_meter(2 + random(0,4));
      delay(100);
    }
    bargraph_and_meter(0);

    //reset timeout
    lastEvent = millis();
  }
}

void forward_quiet()
{
  while( digitalRead(HIDDEN_BUTTON_PIN) == LOW)
  {
    bargraph_and_meter(5);

    //reset timeout
    lastEvent = millis();
  }
  bargraph_and_meter(0);
}

void reverse_quiet()
{
  while( digitalRead(HIDDEN_BUTTON_PIN) == LOW)
  {
    bargraph_and_meter(0);
  }
  bargraph_and_meter(5);

  //Disable the Power ON indicator
  lastEvent = millis();
}

/* ******************************************************************************
 * Sub-Functions for Lights and Neter
 ****************************************************************************** */

void bargraph_and_meter(int current_number)
{
  //this wrapper function allows the bargraph and meter functions to be combined

  /* Update Analog meter */
  meter_only(current_number);

  // Update bargraph
  bargraph_only(current_number);
}

void meter_only(int current_number)
{
  /* Update Analog meter */
  analogWrite(METER_PIN, map(current_number, 0, 5, 0, MAX_METER_OUTPUT));
}

void bargraph_only(int current_number)
{
  static int last_number = 0;

  //clamp max input
  if(current_number > 5)
  {
    current_number = 5;
  }

  /* Update bargraph display */
  if (current_number > last_number)
  {
    /* Turn on LEDs, up to the requested number */
    for ( int i = last_number; i < current_number; i++)
    {
      digitalWrite(LED_PIN_MAP[i], HIGH);
      delay(50);
    }
  }
  else
  {
    /* Turn off LEDs, down to the requested number */
    for ( int i = last_number; i >= current_number; i--)
    {
      digitalWrite(LED_PIN_MAP[i], LOW);
      delay(50);
    }
  }

  //update number
  last_number = current_number;
}

/* ******************************************************************************
 * Sub-Functions for Audio
 ****************************************************************************** */

void play_audio()
{
    /*  Finding the least expensive method of sending strings with a variable:

      Method 1: Do not combine Strings
        Serial.print("#");
        Serial.println(audio_clip_selected);
      Conclusion 1: 434 bytes for println val and print const

      Method 2: Combine Strings
        Serial.println(String("#" + audio_clip_selected));
      Conclusion 2: 958 bytes for string concatinate

      Method 3: Use constants and branch statements
        (see below)
      Conclusion 3: 158 bytes for case switch and println
  */

  switch (audio_clip_selected)
  {
    case 1:
      Serial.println("#0");
      break;
    case 2:
      Serial.println("#1");
      break;
    case 3:
      Serial.println("#2");
      break;
    case 4:
      Serial.println("#3");
      break;
    case 5:
    default:  //max selection is clamped to 5
      Serial.println("#4");
      break;
  }
}
