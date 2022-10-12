
/*
   Prop EMF Reader, based on tv show Supernatural
   Author: Dustin Westaby
   Date: July 2015

   Uses audio fx board from adafruit.
   https://learn.adafruit.com/adafruit-audio-fx-sound-board/

   Build Pictures:
   https://www.flickr.com/photos/dwest2/albums/72157640062702294

*/

/*
 * Audio Jack to Bargraph Test
 * 
 * Arduino Nano, Wiring
 * 3.3V --- 5.1k ohm --- AREF
 * A0 --- 10k ohm --- Audio Source --- 100K ohm --- ground
 * 
 * The jack to A0 resistors above can be modified for easier detection.
 * Adding a DC level to the AC audio signal.
 * 
 *                               VCC
 *                                |
 *                               |R| 10k
 *                                |
 * Audio Jack Input ---- |C| ---- O ---- Audio + VCC/2 to MCU
 *                      100nF     |
 *                               |R| 10k
 *                                |
 *                               GND
 */

#include <EEPROM.h>
#include <arduinoFFT.h>
#include "buttons.h"
#include "mode_navigation.h"

/* ----------------------------------
    Quick Program Switches
   ---------------------------------- */
#define ENABLE_SERIAL_DEBUG

#define ANALOG_REFERENCE EXTERNAL //Define as INTERNAL (5V) or EXTERNAL if you wired 3.3V --- 5.1k ohm --- AREF

/* ----------------------------------
    Animation Information
   ---------------------------------- */
#define FFT_ANIMATION_DELAY   1
#define PROP_ANIMATION_DELAY 50

// after audio is loudness threshold is reached, these defines how fast the lights change
// 1024 is max responsivness, min smoothing
//    1 is min responsivness, max smoothing
#define DECAY_PER_FFT_LOOP  1
#define GROWTH_PER_FFT_LOOP 5

/* ----------------------------------
    Pin Defines
   ---------------------------------- */
#define AUDIO_INPUT_PIN     A0  // Wiring: A0 --- 10k ohm --- Audio Source
#define THRESHOLD_INPUT_PIN A7  // POT

#define METER_PIN 9
#define MAX_METER_OUTPUT 100

// Button triggers effects
#define HIDDEN_LED_PIN A5

byte LED_PIN_MAP[6] =
{
  A4,   // LED 1
  A3,   // LED 2
  A2,   // LED 3
  A1,   // LED 4
  10,   // LED 5
  HIDDEN_LED_PIN,
};

#define BATT_50PERCENT_PIN A6

/* ----------------------------------
    Globals
   ---------------------------------- */
//audio clip selection is stored in eeprom
byte audio_clip_selected = 1;
#define MAX_AUDIO_CLIPS 5
#define EEPROM_ADDRESS_AUDIO_SELECTION 0

//initial data for globals
byte left_toggle_state = LOW;
byte right_toggle_state = LOW;
byte hidden_button_powerup_state = LOW;
unsigned long lastEvent = 0;
byte secret_waiting = false;

#define FFT_SAMPLES 64  // must be a power of 2
arduinoFFT FFT = arduinoFFT();      

/* ******************************************************************************
    Source Code
 ****************************************************************************** */
void setup()
{
  Serial.begin(9600);
  
  analogReference(ANALOG_REFERENCE);
 
  //setup up output pins, start with LEDs off
  for ( byte i=0; i < sizeof(LED_PIN_MAP); i++)
  {
    pinMode(LED_PIN_MAP[i], OUTPUT);
    digitalWrite(LED_PIN_MAP[i], LOW);
  }
  
  pinMode(METER_PIN, OUTPUT);
  
  enable_input_switches();

#ifdef ENABLE_SERIAL_DEBUG
  Serial.write("EMF Started");
#endif

  //retrieve and validate data from eeprom
  audio_clip_selected = EEPROM.read(EEPROM_ADDRESS_AUDIO_SELECTION);
  if( ( audio_clip_selected == 0 ) ||
      ( audio_clip_selected > MAX_AUDIO_CLIPS ) )
  {
     audio_clip_selected = 1;

     //fix eeprom
     EEPROM.write(EEPROM_ADDRESS_AUDIO_SELECTION, audio_clip_selected);
     
#ifdef ENABLE_SERIAL_DEBUG
  Serial.write("EEPROM Reset");
#endif
  }
  
  init_mode_structs();

}
 
    
void loop()
{   
  static long analysis_timer = 0;

  // wait one millisecond for the next analysis
  if(analysis_timer < millis())
  {
    analysis_timer = millis() + 1;
    
    poll_input_switches();
    process_mode_selection();
    
    run_schedule_for_mode();
  }
  
  //TBD: check for sleep
  
}

void run_schedule_for_mode()
{
	static uint8_t previous_state = 0;
	uint8_t current_state = get_software_state();
	bool trigger = get_trigger_event_and_clear(); //this is a consume once value
	
	if(current_state != previous_state)
	{
		//TBD, perform do once actions on state change
		
		//blink bargraph to indicate the selected mode
		
		previous_state = current_state;
	}
	
	switch(current_state)
	{
		/* ************ */
    /* SYSTEM MODES */
		/* ************ */
    case GO_TO_SLEEP:
    	set_software_state(SLEEPING);
    	//TBD, turn off sound card and amp
    	//TBD, add sleep code
    	break;
    case SLEEPING:
    	//TBD, when we return from sleep go back to init
    	set_software_state(INITILIZATION);
    	break;
    case INITILIZATION:
    	//TBD, do some init things again
    	set_software_state(PROP_AUDIO_MODE);
    	//break; continue to PROP_AUDIO_MODE
    
		/* ************ */
    /* NORMAL MODES */
		/* ************ */
    case PROP_AUDIO_MODE:
    	//TBD, check for button press
    	if(trigger)
  		{
		    play_audio();
  			uint8_t value = get_audio_analysis_0_to_5();
  			bargraph_only(value, FFT_ANIMATION_DELAY);
  		}
    	break;
    case PROP_SILENCE_MODE:
    	//TBD, check for button press
    	if(trigger)
  		{
  			//TBD, play audio with amp off? or use fake meter RNG routine?
    		//TBD, update bargraph and meter
  		}
    	break;
    case SHOWCASE_MODE:
    	//TBD, RNG or Button trigger
    	if(trigger)
  		{
    		//TBD, update bargraph and meter
  		}
    	break;
    case EMF_MODE:
    	//TBD, read spare analog input
    	//TBD, analyze emf
    	//TBD, play audio
    	//TBD, update bargraph and meter
    	break;
    
		/* ************* */
    /* SETTING MODES */
		/* ************* */
    case SET_AUDIO_SELECTION:
    	// check for button press
    	if(trigger)
  		{
	    	//update audio selection
		    audio_clip_selected++;
		    if( ( audio_clip_selected == 0 ) ||
		        ( audio_clip_selected > MAX_AUDIO_CLIPS ) )
		    {
		       audio_clip_selected = 1;
		    }
		    
    		//save selection
	    	EEPROM.write(EEPROM_ADDRESS_AUDIO_SELECTION, audio_clip_selected);
    	
		    //play audio and display number
		    Serial.println("q"); //stop audio (if playing)
		    delay(25);
		    play_audio();
		    bargraph_only(audio_clip_selected, PROP_ANIMATION_DELAY);
	    }
	    
	    //update bargraph only
  		bargraph_only(audio_clip_selected, PROP_ANIMATION_DELAY);
    	break;
    	
		/* ************ */
    /* HIDDEN MODES */
		/* ************ */
		//we should never be here
    case EXIT_AUDIO_SELECTION:
    case MENU_SCREEN_WRAP_LOW:
    case MENU_SCREEN_WRAP_HIGH:
    case LAST_MENU_ITEM:
    default:
    	set_software_state(INITILIZATION); //reboot
      break;
	}
}

/* ******************************************************************************
 * Sub-Functions for Audio Analysis
 ****************************************************************************** */
// Source: https://the-techblog.com/en/lightorgan/

uint8_t get_audio_analysis_0_to_5()
{
  double vReal[FFT_SAMPLES], vImag[FFT_SAMPLES];
  static double max_reading = 5; //assume at least 5
  static uint16_t temp_sums;
  int user_threshold;
#ifdef ENABLE_SERIAL_DEBUG
  char temp[20];
#endif
  
  // collect FFT_SAMPLES
  for(int i=0; i < FFT_SAMPLES; i++)
  {
    int sample = analogRead(AUDIO_INPUT_PIN); // read the voltage at the analog pin
    vReal[i] = sample/4-128;        // compress the data and getting rid of DC-Voltage 
    vImag[i] = 0;                   // reset old imaginary part of the FFT
  }
  
  // let the fourier transform do his magic
  FFT.Windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, FFT_SAMPLES);
  
  // calculate average loudness of the different frequency bands
  //temp_sums  = calculate1024(vReal, vImag,  1,  3, temp_sums); //Bass
  temp_sums   = calculate1024(vReal, vImag,  4, 40, temp_sums);  //Mids
  //temp_sums = calculate1024(vReal, vImag, 40, 63, temp_sums);  //High
  
#ifdef ENABLE_SERIAL_DEBUG
  sprintf(temp,"Sum= %i",(int)temp_sums);
  Serial.println(temp);
#endif
  
  //use max observed for mapping (TBD, allo max reset when audio clips change)
  if(temp_sums > max_reading)
  {
    max_reading = temp_sums;
  }
  
  //get user threshold and convert to percent
  user_threshold = map(analogRead(THRESHOLD_INPUT_PIN),0,1023,0,100);
  
  // return 0-5 matching the average, scaled with max * user percentage
  return map(temp_sums,0,max_reading*user_threshold/100,0,5);
}

uint16_t calculate1024(double vReal[], double vImag[], int first, int last, double oldSum)
{
	//This function returns a value of 0-1024 for the loudness of the desired frequency band
	
  double newSum = 0;
 
  // sum up all absolute value of the complex numbers (it's just the pythagorean theorem)
  for(int i=first; i < last; i++)  
  {
    newSum += sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }
  
  newSum = newSum/(last-first-1);         // divide to get the the average loudness
  newSum = constrain(newSum,0,5000);      // crop the number above 5000 
  newSum = map(newSum,0,5000,0,1024);     // map it between 0 and 1024
 
  // add some delays to slow down the animation
  oldSum -= DECAY_PER_FFT_LOOP; //delay down
  if(newSum > oldSum)
  {
    oldSum += GROWTH_PER_FFT_LOOP; //delay up
    if(oldSum >= newSum)
    {
    	// delays complete, clamp
      oldSum = newSum;
    }
  }
 
  return oldSum;
}

/* ******************************************************************************
 * Sub-Functions for Lights and Neter
 ****************************************************************************** */

void bargraph_only(int current_number, byte delay_ms)
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
      delay(delay_ms);
    }
  }
  else
  {
    /* Turn off LEDs, down to the requested number */
    for ( int i = last_number; i >= current_number; i--)
    {
      digitalWrite(LED_PIN_MAP[i], LOW);
      delay(delay_ms);
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
