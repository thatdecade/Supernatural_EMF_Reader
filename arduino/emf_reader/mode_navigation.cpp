/*
 * menu_navitation.c
 *
 *  Created on: Jul 15, 2022
 *      Author: Dustin Westaby
 */

#include "buttons.h"
#include "mode_navigation.h"

#define HOLD_DETECTION_DELAY_MS 750

byte software_state = INITILIZATION;
byte software_state_processed = true;

//prototypes
void button_clicked(byte button);
void button_held(byte button);

void process_left_click();
void process_right_click();
void process_hidden_click();

void process_left_hold();
void process_right_hold();
void process_hidden_hold();

enum
{
   PROCESSED,
   NOT_PROCESSED
};

byte software_state_save_for_after_audio_selection = 0;
long last_menu_interaction_timestamp[NUMBER_OF_BUTTONS];
byte last_mode_button_state[NUMBER_OF_BUTTONS];

bool trigger_event = false;

/* ************************************************ */
/* Button states  */
/* ************************************************ */

void init_mode_structs()
{
  for( byte i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
  	last_menu_interaction_timestamp[i] = 0;
  	last_mode_button_state[i] = NOT_PROCESSED;
  }
}

void process_mode_selection()
{
  byte current_button_state;

  for( byte i = 0; i < NUMBER_OF_BUTTONS; i++)
  {
    current_button_state = get_button_state(i);
    
    if(current_button_state == WAS_RELEASED)
    {
        //CLICKED ACTION

        //Note: action taken on WAS_RELEASED instead of IS_PRESSED to prevent extra clicks during button holds

        last_menu_interaction_timestamp[i] = millis();

        if(last_mode_button_state[i] == NOT_PROCESSED)
        {
            //one action per press / hold
            for( byte j = 0; i < NUMBER_OF_BUTTONS; i++)
            {
              last_mode_button_state[i] = PROCESSED;
            }
            
            button_clicked(i);
        }
    }
    else if ( ( current_button_state == IS_HELD ) &&
              ( last_menu_interaction_timestamp[i] + HOLD_DETECTION_DELAY_MS < millis() ) )
    {
        //HELD ACTION

        last_menu_interaction_timestamp[i] = millis();

        if(last_mode_button_state[i] == NOT_PROCESSED)
        {
            //one action per press / hold
            for( byte j = 0; i < NUMBER_OF_BUTTONS; i++)
            {
              last_mode_button_state[i] = PROCESSED;
            }

            button_held(i);
        }
    }
    else if (current_button_state == IS_NOT_PRESSED)
    {
        //reset timers
        last_menu_interaction_timestamp[i] = millis();
        last_mode_button_state[i] = NOT_PROCESSED;
    }

  }// end for loop
}

/* ************************************************ */
/* Button actions  */
/* ************************************************ */

void button_clicked(byte button)
{      
  // call clicked action handler
  switch(button)
  {
    case TOGGLE_LEFT:
      process_left_click();
      break;
    case TOGGLE_RIGHT:
      process_right_click();
      break;
    case HIDDEN_BUTTON:
      process_hidden_click();
      break;
    default:
      break; //do nothing
  }
}

void button_held(byte button)
{
  // call clicked action handler
  switch(button)
  {
    case TOGGLE_LEFT:
      process_left_hold();
      break;
    case TOGGLE_RIGHT:
      process_right_hold();
      break;
    case HIDDEN_BUTTON:
      process_hidden_hold();
      break;
    default:
      break; //do nothing
  }
}

void process_left_click()
{
	//TBD
}

void process_right_click()
{
	//TBD
}

void process_hidden_click()
{
  switch (software_state)
  {   
      case GO_TO_SLEEP:
      case SLEEPING:
      case INITILIZATION:
        //wake up
        set_software_state(INITILIZATION);
        break;
  	  case PROP_AUDIO_MODE:
      case PROP_SILENCE_MODE:
      case SET_AUDIO_SELECTION:
      	set_trigger_event();
      default:
        //do nothing
        break;
  }
}

void process_left_hold()
{
  switch (software_state)
  {
      case PROP_AUDIO_MODE:
      case PROP_SILENCE_MODE:
      case SHOWCASE_MODE:
      case EMF_MODE:
        //mode_navigation_back
        set_software_state(software_state - 1);
        break;
        
      default:
        //do nothing
        break;
  }
}

void process_right_hold()
{
  switch (software_state)
  {
      case PROP_AUDIO_MODE:
      case PROP_SILENCE_MODE:
      case SHOWCASE_MODE:
      case EMF_MODE:
        //mode_navigation_forward
        set_software_state(software_state + 1);
        break;
        
      default:
        //do nothing
        break;
  }
}

void process_hidden_hold()
{
	 
  switch (software_state)
  {
      case PROP_AUDIO_MODE:
      case PROP_SILENCE_MODE:
      case SHOWCASE_MODE:
      case EMF_MODE:
        // user wants to access settings. Enter audio selection
        software_state_save_for_after_audio_selection = software_state;
        set_software_state(SET_AUDIO_SELECTION);
        break;
      case SET_AUDIO_SELECTION:
        // user wants to exit settings. Save the settings
        set_software_state(SAVE_AUDIO_SELECTION);
        break;
      
      default:
        //do nothing
        break;
  }
}

/* ************************************************ */
/* Software states  */
/* ************************************************ */

void set_software_state(uint8_t new_state)
{
  //validate state
  if(new_state < LAST_MENU_ITEM)
  {
    software_state = new_state;
  }
  
  //handle wrap around
  switch (software_state)
  {
    case EXIT_AUDIO_SELECTION:
    	// go back to previous state
      software_state = software_state_save_for_after_audio_selection;
    	break;
    case MENU_SCREEN_WRAP_LOW:
      software_state = MENU_LOW_WRAP_AROUND;
      break;
    case MENU_SCREEN_WRAP_HIGH:
      software_state = MENU_HIGH_WRAP_AROUND;
      break;
    default:
      //do nothing
      break;
  }
}

uint8_t get_software_state()
{
  return software_state;
}

void set_trigger_event()
{
	trigger_event = true;
}

bool get_trigger_event_and_clear()
{
	bool temp = trigger_event;
	trigger_event = false;
	return temp;
}
