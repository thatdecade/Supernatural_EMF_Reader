//testing functions one by one on the new board,
// Nov 6, 2022
// Dustin Westaby
// can select board as arduino nano, but do not set fuses with that board (disables internal oscillator)

#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <arduinoFFT.h>
#include "buttons.h"

/* ******************************************************************************
   Options
 ****************************************************************************** */

//meter range display is 1V of this number
// example: percentage will show 3.8V = 0% and >4.8V = 100%
#define BATTERY_LOW  3.8

#define DEFAULT_METER_ANIMATION_DELAY_MS 50

#define HOLD_DETECTION_DELAY_MS 750

#define DISPLAY_MODE_LONGEST_TIME_BETWEEN_SELF_ACTIVATION 15 //seconds

#define IDLE_BLINK_TIMEOUT 30000 //30 seconds

/* ******************************************************************************
   Pin Definitions
 ****************************************************************************** */
#define METER_PIN 9

#define AUDIO_INPUT_PIN     A0  // Wiring: A0 --- 10k ohm --- Audio Source
#define THRESHOLD_INPUT_PIN A6  // POT
#define BATTERY_LEVEL_PIN   A7  // Battery % = (ADC*2*3.3/1024-LOW)*100

typedef enum
{
  LED_BUTTON,
  LED_1,
  LED_2,
  LED_3,
  LED_4,
  LED_5,
  NUM_LEDS
} led_name_e;

byte LED_PIN_MAP[NUM_LEDS] =
{
  A5,   // LED_BUTTON
  10,   // LED_5
  A1,   // LED_4
  A2,   // LED_3
  A3,   // LED_2
  A4,   // LED_1
};

/* ******************************************************************************
   Globals
 ****************************************************************************** */
SoftwareSerial mySoftwareSerial(0, 1); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

#define MAX_METER_OUTPUT 100

unsigned long last_user_interaction_timestamp = 0;

/* ******************************************************************************
   Menu System
 ****************************************************************************** */

enum
{
  PROCESSED,
  NOT_PROCESSED
} mode_processed_e;

typedef enum
{
  IDLE_MODE = 0, //Not used
  FX_MODE,
  DISPLAY_MODE,
  SET_AUDIO_SELECT_MODE,
  SET_VOLUME_MODE,
  BATTERY_CHECK_MODE,
  NUMBER_OF_MODES,
} modes_e;

byte mode_selected = FX_MODE;

bool hidden_button_click_flag = false;
bool hidden_button_held_flag = false;

/* ******************************************************************************
   EEPROM Storage
 ****************************************************************************** */

#define MAX_AUDIO_CLIPS 5
byte audio_clip_selected = 1;

// ******************************

typedef struct
{
  byte address;
  byte default_value;
  byte valid_low;
  byte valid_high;
  byte *global_pointer;
} eeprom_storage_t;

typedef enum
{ // order of storage arrays
  AUDIO_SELECTION = 0,
  MODE_SELECTION,
  NUMBER_OF_STORED_VALUES,
} value_storage_e;

eeprom_storage_t data_storage[NUMBER_OF_STORED_VALUES] =
{
  { // AUDIO_SELECTION
    /* address    */ 0,
    /* default    */ 3,
    /* valid_low  */ 1,
    /* valid_high */ MAX_AUDIO_CLIPS,
    /* global_ptr */ &audio_clip_selected,
  },
  { // MODE_SELECTION
    /* address    */ 1,
    /* default    */ FX_MODE,
    /* valid_low  */ FX_MODE,
    /* valid_high */ (NUMBER_OF_MODES - 1),
    /* global_ptr */ &mode_selected,
  },
};

/* ******************************************************************************
   FFT Stuff
 ****************************************************************************** */
#define FFT_ANIMATION_DELAY   1
#define PROP_ANIMATION_DELAY 50

#define SAMPLES 64  // must be a power of 2

// defines how much brigthness the LEDs will loose each loop
// 5 is the maximum --> the organ will be very responsive
// lower will smootly the the leds if no new peak is detected
#define decayPerLoop 1
#define growthPerLoop 5

arduinoFFT FFT = arduinoFFT();

typedef enum FREQ_RANGE
{
  BASS = 0,
  MID,
  HIGHS,
} freq_range_e;

/* ******************************************************************************
    Source Code
 ****************************************************************************** */
void setup() {
  // put your setup code here, to run once:

  analogReference(EXTERNAL);

  //setup up output pins, start with LEDs off
  for ( byte i = 0; i < sizeof(LED_PIN_MAP); i++)
  {
    pinMode(LED_PIN_MAP[i], OUTPUT);
    digitalWrite(LED_PIN_MAP[i], LOW);
  }
  digitalWrite(LED_PIN_MAP[LED_BUTTON], HIGH);
  
  pinMode(METER_PIN, OUTPUT);

  enable_input_switches();

  eeprom_read_all_data_to_globals();

  delay(1000); //wait for dfplayer to boot
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial))
  {
    //TBD, report error state to user
    //Press button to continue
    while (get_button_state(HIDDEN_BUTTON) == IS_NOT_PRESSED)
    {
      poll_input_switches();
      for ( int i = LED_1; i < NUM_LEDS; i++)
      {
        digitalWrite(LED_PIN_MAP[i], HIGH);
      }
      delay(100);
      for ( int i = LED_1; i < NUM_LEDS; i++)
      {
        digitalWrite(LED_PIN_MAP[i], LOW);
      }
      delay(100);
    }
  }

  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms
  myDFPlayer.volume(15);  //Set volume value (0~30).
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  randomSeed(analogRead(A0));
  digitalWrite(LED_PIN_MAP[LED_BUTTON], LOW);
}

void loop() {
  static long loop_timer = 0;
  static long random_activate_timer = 0;
  static long blink_timer = 0;
  static byte track = 0;
  static byte sensorValue = 0;

  // wait one millisecond for the next process
  if (loop_timer < millis())
  {
    loop_timer = millis() + 1;

    poll_input_switches();
    process_hidden_button_state();

    //hold button to send to next mode
    if (hidden_button_held_flag)
    {
      //reset globals
      hidden_button_held_flag = false;
      hidden_button_click_flag = false;
      digitalWrite(LED_PIN_MAP[LED_BUTTON], LOW);
      myDFPlayer.stop();
      
      increment_data_store(MODE_SELECTION); //mode_selected++ with wrap around

      //tell user which mode is selected
      while ((loop_timer + 1000) > millis())
      {
        bargraph_and_meter(mode_selected % 5, PROP_ANIMATION_DELAY);
      }
    }

    //handle button presses in each mode
    switch (mode_selected)
    {
      case FX_MODE:
        if (hidden_button_click_flag)
        {
          hidden_button_click_flag = false;
          
          playTrackX(audio_clip_selected);
        }
        sensorValue = analyze_audio_update_bargraph(); //this takes about 100ms
        bargraph_and_meter(sensorValue, FFT_ANIMATION_DELAY);
        break;
        
      case DISPLAY_MODE:
        //press button or wait for random timer
        if ((hidden_button_click_flag) || (random_activate_timer < millis()))
        {
          hidden_button_click_flag = false;
          random_activate_timer = millis() + random(1, DISPLAY_MODE_LONGEST_TIME_BETWEEN_SELF_ACTIVATION) * 1000;
          last_user_interaction_timestamp = millis();
          
          playTrackX(audio_clip_selected);
        }
        sensorValue = analyze_audio_update_bargraph(); //this takes about 100ms
        bargraph_and_meter(sensorValue, FFT_ANIMATION_DELAY);
        break;
        
      case BATTERY_CHECK_MODE:
        //no button press needed, display battery reading
        bargraph_and_meter(get_battery_meter(), PROP_ANIMATION_DELAY);
        break;
        
      case SET_AUDIO_SELECT_MODE:
        if(blink_timer + 50 < millis())
        {
          blink_timer = millis();
          digitalWrite(LED_PIN_MAP[LED_BUTTON], !digitalRead(LED_PIN_MAP[LED_BUTTON])); //blink hidden LED
        }
        if (hidden_button_click_flag)
        {
          hidden_button_click_flag = false;
          
          increment_data_store(AUDIO_SELECTION); //audio_clip_selected++ with wrap around
          playTrackX(audio_clip_selected);
        }
        bargraph_and_meter(audio_clip_selected, PROP_ANIMATION_DELAY);
        break;
        
      case SET_VOLUME_MODE:
        break;
        
      default:
        break;
    }
  }

  //TBD, toggle switch sets silent or audio

  // Chirp first LED every few seconds
  if((last_user_interaction_timestamp + IDLE_BLINK_TIMEOUT) < millis())
  {
    last_user_interaction_timestamp = millis();
    while ((loop_timer + 500) > millis())
    {
      bargraph_and_meter(1, PROP_ANIMATION_DELAY);
    }
    bargraph_only(0, PROP_ANIMATION_DELAY);
  }
}

/* ******************************************************************************
   Sub-Functions for Audio Analysis
 ****************************************************************************** */

byte analyze_audio_update_bargraph()
{
  double vReal[SAMPLES], vImag[SAMPLES];
  static double max_reading = 5; //assume at least 5
  static double temp_sums;
  int user_threshold;
  char temp[20];

  // collect Samples
  for (int i = 0; i < SAMPLES; i++)
  {
    int sample = analogRead(AUDIO_INPUT_PIN); // read the voltage at the analog pin
    vReal[i] = sample / 4 - 128;    // compress the data and getting rid of DC-Voltage
    vImag[i] = 0;                   // reset old imaginary part of the FFT
  }

  // let the fourier transform do his magic
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

  // calculate average loudness of the different frequency bands
  //temp_sums  = calculateBars(vReal, vImag,  1,  3, temp_sums); //Bass
  temp_sums   = calculateBars(vReal, vImag,  4, 40, temp_sums); //Mids
  //temp_sums = calculateBars(vReal, vImag, 40, 63, temp_sums); //High

  //sprintf(temp,"Sum= %i",(int)temp_sums);
  //Serial.println(temp);

  //use max observed for mapping (TBD, allo max reset when audio clips change)
  if (temp_sums > max_reading)
  {
    max_reading = temp_sums;
  }

  //get user threshold and convert to percent
  user_threshold = map(analogRead(THRESHOLD_INPUT_PIN), 0, 1023, 0, 100);

  // light a number of bars matching the average, scaled with max * user percentage
  return map(temp_sums, 0, max_reading * user_threshold / 100, 0, 5);
}

double calculateBars(double vReal[], double vImag[], int first, int last, double oldSum) {
  double newSum = 0;

  // sum up all absolute value of the complex numbers (it's just the pythagorean theorem)
  for (int i = first; i < last; i++)
  {
    newSum += sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }

  newSum = newSum / (last - first - 1);   // divide to get the the average loudness
  newSum = constrain(newSum, 0, 5000);    // crop the number above 5000
  newSum = map(newSum, 0, 5000, 0, 1024); // map it between 0 and 1024

  oldSum -= decayPerLoop;                 // substract the decay from the old brightness
  if (newSum > oldSum)
  {
    // if the old brightness is now lower then the new, the new brightness will be outputted
    oldSum += growthPerLoop;
    if (oldSum >= newSum)
    {
      oldSum = newSum;
    }
  }

  return oldSum;
}

/* ******************************************************************************
   Sub-Functions for Audio Playback
 ****************************************************************************** */

// routine to play a specific song (This is needed because we are using the MP3-TF-16 boards)
void playTrackX (byte x) {

  mySoftwareSerial.write((byte)0x7E);
  mySoftwareSerial.write((byte)0xFF);
  mySoftwareSerial.write((byte)0x06);
  mySoftwareSerial.write((byte)0x03);
  mySoftwareSerial.write((byte)0x00);
  mySoftwareSerial.write((byte)0x00);
  mySoftwareSerial.write((byte)x);
  mySoftwareSerial.write((byte)0xEF);
}

/* ******************************************************************************
   Sub-Functions for Battery Life
 ****************************************************************************** */

uint8_t get_battery_meter()
{
  return map(get_battery_percent(), 0, 100, 1, 5);
}

uint8_t get_battery_percent()
{
  int nBattPct;
  float fBattV = analogRead(BATTERY_LEVEL_PIN);
  fBattV = fBattV * 2;           // voltage divider resistors
  fBattV = fBattV * 3.3;         // reference voltage
  fBattV = fBattV / 1024;        // convert to voltage
  fBattV = fBattV - BATTERY_LOW; // subtract low voltage range
  nBattPct = fBattV * 100;         // convert voltage to a percentage
  nBattPct = constrain(nBattPct, 0, 100);
  return (uint8_t)nBattPct;
}



/*******************************************************************************
   Sub-Functions for Lights and Neter
 *******************************************************************************/

void bargraph_and_meter(int current_number, byte delay_ms)
{
  //this wrapper function allows the bargraph and meter functions to be combined

  /* Update Analog meter */
  meter_only(current_number);

  // Update bargraph
  bargraph_only(current_number, delay_ms);
}

void meter_only(int current_number)
{
  /* Update Analog meter */
  analogWrite(METER_PIN, map(current_number, 0, 5, 0, MAX_METER_OUTPUT));
}

void bargraph_only(int current_number, byte delay_ms)
{
  static int last_number = 0;
  static unsigned long last_update_timestamp = 0;

  if (millis() > last_update_timestamp + delay_ms)
  {
    last_update_timestamp = millis();

    //clamp and set zero index
    if (current_number > 5)  current_number = 5;
    if (last_number < LED_1) last_number = LED_1;
    current_number += LED_1;

    /* Update bargraph display */
    if (current_number > last_number)
    {
      /* Turn on LEDs, up to the requested number */
      digitalWrite(LED_PIN_MAP[last_number], HIGH);
      //delay(delay_ms);
      last_number++;
    }
    else if (current_number <= last_number)
    {
      /* Turn off LEDs, down to the requested number */
      {
        digitalWrite(LED_PIN_MAP[last_number], LOW);
        //delay(delay_ms);
        last_number--;
      }
    }

    //update number
    //last_number = current_number;
  }
}

/* ******************************************************************************
    EEPROM Data Storage
 ****************************************************************************** */

void eeprom_read_all_data_to_globals()
{
  for (byte i = 0; i < NUMBER_OF_STORED_VALUES; i++)
  {
    eeprom_read_data_storage(i);
  }
}

void eeprom_read_data_storage(byte index)
{
  *(data_storage[index].global_pointer) = EEPROM.read(data_storage[index].address);
  if ( ( *(data_storage[index].global_pointer) < data_storage[index].valid_low ) ||
       ( *(data_storage[index].global_pointer) > data_storage[index].valid_high ) )
  {
    *(data_storage[index].global_pointer) = data_storage[index].default_value;
    EEPROM.write(data_storage[index].address, *(data_storage[index].global_pointer));
  }
}

void eeprom_write_data_storage(byte index)
{
  EEPROM.write(data_storage[index].address, *(data_storage[index].global_pointer));
}

void increment_data_store(byte index)
{
  //check if we are at the end of the list of modes
  if ( (*(data_storage[index].global_pointer) + 1) <= data_storage[index].valid_high )
  {
    //increment is ok
    *(data_storage[index].global_pointer) = *(data_storage[index].global_pointer) + 1;
  }
  else
  {
    //set to wrap around value
    *(data_storage[index].global_pointer) = data_storage[index].valid_low;
  }

  //save audio selection to eeprom
  eeprom_write_data_storage(index);
}

/* ******************************************************************************
   Menu System
 ****************************************************************************** */

void process_hidden_button_state()
{
  static unsigned long last_button_press_timestamp = 0;
  static byte last_hidden_button_state = NOT_PROCESSED;
  byte current_button_state = get_button_state(HIDDEN_BUTTON);

  if (current_button_state == WAS_RELEASED)
  {
    //CLICKED ACTION

    //Note: action taken on WAS_RELEASED instead of IS_PRESSED to prevent extra clicks during button holds

    last_button_press_timestamp = millis();
    last_user_interaction_timestamp = millis();

    if (last_hidden_button_state == NOT_PROCESSED)
    {
      //one action per press / hold
      last_hidden_button_state = PROCESSED;

      hidden_button_click_flag = true;
    }
  }
  else if ( ( current_button_state == IS_HELD ) &&
            ( last_button_press_timestamp + HOLD_DETECTION_DELAY_MS < millis() ) )
  {
    //HELD ACTION

    last_button_press_timestamp = millis();

    if (last_hidden_button_state == NOT_PROCESSED)
    {
      //one action per press / hold
      last_hidden_button_state = PROCESSED;

      hidden_button_held_flag = true;
    }
  }
  else if (current_button_state == IS_NOT_PRESSED)
  {
    //reset timers
    last_button_press_timestamp = millis();
    last_hidden_button_state = NOT_PROCESSED;
  }
}
