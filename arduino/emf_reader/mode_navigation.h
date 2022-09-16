/*
 * mode_navigation.h
 *
 *  Created on: May 30, 2022
 *      Author: Dustin Westaby
 */

#ifndef INC_MODE_NAVIGATION_H_
#define INC_MODE_NAVIGATION_H_

#include <Arduino.h>

/* --------------------- */
/*   SW Modes            */
/* --------------------- */
enum menu_page_order
{
	 //system items
   GO_TO_SLEEP = 0,
   SLEEPING,
   INITILIZATION,
   
   MENU_SCREEN_WRAP_LOW,
   
   //normal accessable items
   PROP_AUDIO_MODE,
   PROP_SILENCE_MODE,
   SHOWCASE_MODE,
   EMF_MODE,
   
   MENU_SCREEN_WRAP_HIGH,
   
   //settings items
   SET_AUDIO_SELECTION,
   EXIT_AUDIO_SELECTION,
   
   LAST_MENU_ITEM,
};

#define MENU_HIGH_WRAP_AROUND PROP_AUDIO_MODE
#define MENU_LOW_WRAP_AROUND  EMF_MODE

void init_mode_structs();

void process_mode_selection();
void set_software_state(uint8_t new_state);
uint8_t get_software_state();

void set_trigger_event();
bool get_trigger_event_and_clear();

#endif
