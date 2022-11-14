/*
 * buttons.c
 *
 *  Created on: Jul 15, 2022
 *      Author: Dustin Westaby
 *      
 *  Usage: process button states after polling.
 *   IS_NOT_PRESSED = Idle state
 *   IS_PRESSED     = Button was "just" pressed
 *   WAS_RELEASED   = Button was "just" released
 *   IS_HELD        = Button was held for more than 
 *                    one cycle.  Combine with a timer.
 *   
 *   IS_PRESSED should not be used if you need to take actions 
 *     for both press and hold.  Use WAS_RELEASED and IS_HELD.
 */

#include "buttons.h"

byte pinMap_buttons[NUMBER_OF_BUTTONS] =
{
  2, // HIDDEN_BUTTON
  3, // TOGGLE_LEFT
  8, // TOGGLE_RIGHT
};

byte button_state[NUMBER_OF_BUTTONS] = 
{
  IS_NOT_PRESSED,
  IS_NOT_PRESSED,
  IS_NOT_PRESSED,
};

bool last_button_read[NUMBER_OF_BUTTONS];
bool current_button_read[NUMBER_OF_BUTTONS];

/* ************************************************ */
/* ************************************************ */
/* ************************************************ */

byte get_button_state(byte button)
{
  return button_state[button];
}

void poll_input_switches()
{

  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
    /* read digital active low button signal */
    current_button_read[i] = !digitalRead(pinMap_buttons[i]);

    if (!last_button_read[i] && current_button_read[i])
    {
      //button was just pressed
      button_state[i] = IS_PRESSED;
    }
    else if  (last_button_read[i] && current_button_read[i])
    {
      //button is being held
      button_state[i] = IS_HELD;
    }
    else if (last_button_read[i] && !current_button_read[i])
    {
      //button was just released
      button_state[i] = WAS_RELEASED;
    }
    else
    {
      button_state[i] = IS_NOT_PRESSED;
    }

    last_button_read[i] = current_button_read[i];
  }
}

void enable_input_switches()
{
  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
		pinMode(pinMap_buttons[i], INPUT);
  	last_button_read[i]    = false;
  	current_button_read[i] = false;
	}
}

void disable_input_switches()
{
  for(uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
		pinMode(pinMap_buttons[i], OUTPUT);
		digitalWrite(pinMap_buttons[i], HIGH);
	}
}
