/*
 * buttons.h
 *
 *  Created on: Jul 15, 2022
 *      Author: Dustin Westaby
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_

#include <Arduino.h>

#define BUTTON_SCAN_RATE_MILLIS 2

typedef enum
{
  IS_NOT_PRESSED,
  IS_PRESSED,
  WAS_RELEASED,
  IS_HELD,
} buttonState_e;

typedef enum
{ // order of storage arrays
	HIDDEN_BUTTON = 0,
	TOGGLE_LEFT,
	TOGGLE_RIGHT,
	NUMBER_OF_BUTTONS,
} button_name_e;

void poll_input_switches();

void enable_input_switches();
void disable_input_switches();

byte get_button_state(byte button);

#endif /* INC_BUTTONS_H_ */
