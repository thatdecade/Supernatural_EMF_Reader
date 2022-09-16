//Source: https://the-techblog.com/en/lightorgan/

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

#include <arduinoFFT.h>

#define AUDIO_INPUT_PIN A0
#define THRESHOLD_INPUT_PIN A1

#define MAX_METER_OUTPUT 100

byte LED_PIN_MAP[6] =
{
  2,   // LED 1
  3,   // LED 2
  4,   // LED 3
  5,   // LED 4
  6,   // LED 5
};

#define FFT_ANIMATION_DELAY   1
#define PROP_ANIMATION_DELAY 50

/* ******************************************************************************
 * FFT Stuff
 ****************************************************************************** */
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
void setup()
{
  //Serial.begin(9600);
  
  // define the analog voltage reference
  analogReference(DEFAULT);
 
  for ( byte i=0; i < sizeof(LED_PIN_MAP); i++)
  {
    pinMode(LED_PIN_MAP[i], OUTPUT);
    digitalWrite(LED_PIN_MAP[i], LOW);
  }
  
  //Serial.write("FFT Ready");
}
 
    
void loop()
{   
  static long analisis_timer = 0;

  // wait one millisecond for the next analysis
  if(analisis_timer < millis())
  {
    analisis_timer = millis() + 1;
    
    analyze_audio_update_bargraph();
  }
}

/* ******************************************************************************
 * Sub-Functions for Audio Analysis
 ****************************************************************************** */

void analyze_audio_update_bargraph()
{
  double vReal[SAMPLES], vImag[SAMPLES];
  static double max_reading = 5; //assume at least 5
  static double temp_sums;
  int user_threshold;
  char temp[20];
  
  // collect Samples
  for(int i=0; i < SAMPLES; i++)
  {
    int sample = analogRead(AUDIO_INPUT_PIN); // read the voltage at the analog pin
    vReal[i] = sample/4-128;        // compress the data and getting rid of DC-Voltage 
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
  if(temp_sums > max_reading)
  {
    max_reading = temp_sums;
  }
  
  //get user threshold and convert to percent
  user_threshold = map(analogRead(THRESHOLD_INPUT_PIN),0,1023,0,100);
  
  // light a number of bars matching the average, scaled with max * user percentage
  bargraph_only(map(temp_sums,0,max_reading*user_threshold/100,0,5), FFT_ANIMATION_DELAY);
}
 
double calculateBars(double vReal[], double vImag[], int first, int last, double oldSum){
  double newSum = 0;
 
  // sum up all absolute value of the complex numbers (it's just the pythagorean theorem)
  for(int i=first; i < last; i++)  
  {
    newSum += sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
  }
  
  newSum = newSum/(last-first-1);         // divide to get the the average loudness
  newSum = constrain(newSum,0,5000);      // crop the number above 5000 
  newSum = map(newSum,0,5000,0,1024);     // map it between 0 and 1024
 
  oldSum -= decayPerLoop;                 // substract the decay from the old brightness
  if(newSum > oldSum)
  {
    // if the old brightness is now lower then the new, the new brightness will be outputted 
    oldSum += growthPerLoop;
    if(oldSum >= newSum)
    {
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
