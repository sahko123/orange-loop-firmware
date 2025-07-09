/* -----------------------------------------------------------------

   Example - Overlay test script

   HCMS39xx Library
   https://github.com/Andy4495/HCMS39xx

   Modified for overlay functionality test

*/
#include "HCMS39xx.h"
#include "font5x7.h"
#include "time.h"
#include "sys/time.h"

// HCMS39xx(uint8_t num_chars, uint8_t data_pin, uint8_t rs_pin, uint8_t clk_pin, uint8_t ce_pin, uint8_t blank_pin)
HCMS39xx myDisplay1(16, 6, 7, 8, 9, 10); // osc_select_pin tied high
HCMS39xx myDisplay2(20, 12, 7, 8, 11, 10); // osc_select_pin tied high
HCMS39xx myDisplay3(20, 14, 7, 8, 13, 10); // osc_select_pin tied high

#define MAXLEN 4
#define UP_BTN 18
#define CENTER_BTN 17
#define DOWN_BTN 16

uint8_t testGrid[280];
uint8_t daylight_savings = 1;
uint8_t animation_buffer[280];
uint8_t colon_layer_buffer[280];
uint32_t loop_start = 0;
uint32_t loop_stop = 0;
uint32_t delta_time = 0;
uint32_t delta_accumulator = 0;
uint32_t animation_frame = 0;
uint32_t number= 1700;

const char* day_strings[] = {"D SN", "D MN", "D TU", "D WE", "D TH", "D FR", "D SA"};


enum Menu {show_mini_time, show_home, show_time, set_brightness, set_current, set_time, NUM_STATES};
Menu current_menu = show_home;


HCMS39xx::DISPLAY_CURRENT current_options[] = {
  HCMS39xx::CURRENT_12_8_mA,
  HCMS39xx::CURRENT_4_0_mA,
  HCMS39xx::CURRENT_6_4_mA,
  HCMS39xx::CURRENT_9_3_mA,
};

HCMS39xx::DISPLAY_CURRENT current_current = HCMS39xx::DEFAULT_CURRENT;

const int num_current_options = sizeof(current_options)/sizeof(current_options[0]);
int current_index = 0;

void increaseCurrent() {
  current_index = (current_index + 1) % num_current_options;
  current_current = current_options[current_index];
}

void decreaseCurrent() {
  current_index = (current_index + num_current_options - 1) % num_current_options;
  current_current = current_options[current_index];
}

bool show_text = false;
uint8_t current_brightness= 15;

bool lastUPButtonState = true; // Default high to prevent startup animation
bool lastDOWNButtonState = true;
bool lastCENTERButtonState = true;
bool UPButtonState = false;
bool DOWNButtonState = false;
bool CENTERButtonState = false;

bool menu_engaged = false;

void setup() {
  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(CENTER_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);

  for (uint8_t i = 0; i < 20; i++) {
    if (i % 2 == 0) {
        testGrid[i] = 0b01010101; // pattern: alternating pixels
    } else {
        testGrid[i] = 0b10101010; // inverse pattern
    }
  }
  
  colon_layer_buffer[150] = 0b00100100;
  
  Serial.begin(115200);

  myDisplay1.begin();
  myDisplay1.displayUnblank();
  myDisplay1.setSerialMode();
  myDisplay1.setBrightness(current_brightness);
  myDisplay1.setCurrent(current_current);

  myDisplay2.begin();
  myDisplay2.displayUnblank();
  myDisplay2.setSerialMode();
  myDisplay2.setBrightness(current_brightness);
  myDisplay2.setCurrent(current_current);

  myDisplay3.begin();
  myDisplay3.displayUnblank();
  myDisplay3.setSerialMode();
  myDisplay3.setBrightness(current_brightness);
  myDisplay3.setCurrent(current_current);

  //initialise time
  struct timeval tv;

  tv.tv_sec = 1611198855; // Jan 21, 2021  3:14:15AM ...RPi Pico Release;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  

  // Initialize overlay pattern to draw a horizontal line on row 3 (bit 2)

  delay(500);
}

void loop() {

  UPButtonState = digitalRead(UP_BTN);
  DOWNButtonState = digitalRead(DOWN_BTN);
  CENTERButtonState = digitalRead(CENTER_BTN);
  
  

  if (Serial.available()) {
    // Read the incoming timestamp as a String
    String input = Serial.readStringUntil('\n');

    // Convert to a long integer (epoch time)
    time_t newTime = (time_t)input.toInt();

    // Set time
    struct timeval tv = { .tv_sec = newTime, .tv_usec = 0 };
    settimeofday(&tv, nullptr);

    Serial.print("Time set to: ");
    Serial.println(newTime);
  }
  time_t now;
  char buff[80];
  time(&now);
  struct tm* tmstruct = localtime(&now);
  //strftime(buff, sizeof(buff), "%c", tmstruct); // Time Diagnostic
  //Serial.println(buff);

  int hour = tmstruct->tm_hour;
  int minute = tmstruct->tm_min;
  int second = tmstruct->tm_sec;
  int day_of_week = tmstruct->tm_wday;
  int year = tmstruct->tm_year;
  int month = tmstruct->tm_mon;
  int day = tmstruct->tm_mday;

  loop_start = micros();
  myDisplay1.clear();
  myDisplay2.clear();
  myDisplay3.clear();

  if (CENTERButtonState && !lastCENTERButtonState && animation_frame==0 && !menu_engaged) {
    ;
  } else if (UPButtonState && !lastUPButtonState && !menu_engaged) {
    current_menu = static_cast<Menu>((current_menu + 1) % NUM_STATES);
  } else if (DOWNButtonState && !lastDOWNButtonState && !menu_engaged) {
    current_menu = static_cast<Menu>((current_menu + NUM_STATES - 1) % NUM_STATES);
  }

  switch (current_menu) {
    case show_mini_time:
      myDisplay1.print("                ");
      char buffertimeMini[21]; // With extra for null terminator
      snprintf(buffertimeMini, sizeof(buffertimeMini), "            %02d%02d", hour+daylight_savings, minute);
      myDisplay2.print(buffertimeMini);
      myDisplay3.print("                    ");
      if(delta_accumulator < 1000000) { // If more than 33ms has passed or animation is just started
        if (delta_accumulator > 500000) {
          myDisplay2.printDirectBufferXOR(&colon_layer_buffer[80], 100);
        } else {
          
        }
        delta_accumulator += delta_time;
      } else {
        delta_accumulator = 0;
      }
      break;
    case show_home:
      if (CENTERButtonState && !lastCENTERButtonState) {
        startAnimation();
      }
      if (show_text) {
        myDisplay1.print("                ");
        char buffer[21]; // With extra for null terminator
        snprintf(buffer, sizeof(buffer), "        H %02dM %02dS %02d", hour+daylight_savings, minute, second);
        myDisplay2.print(buffer);
        myDisplay3.print("                    ");
      }
      Serial.println("show home");
      break;
    case show_time:
      
      myDisplay1.print("                ");
      char buffertime[21]; // With extra for null terminator
      snprintf(buffertime, sizeof(buffertime), "M %02dD %02dH %02dM %02dS %02d", month, day, hour+daylight_savings, minute, second);
      myDisplay2.print(buffertime);
      myDisplay3.print("                    ");
      Serial.println("show time");
      break;
    case set_brightness:

      if (CENTERButtonState && !lastCENTERButtonState) {
        if (menu_engaged) {
          menu_engaged = false;
        } else{
          menu_engaged = true;
        }
      }
      if (menu_engaged) {
      if (UPButtonState && !lastUPButtonState) {
        if(current_brightness+1!=16){
          current_brightness++;
        }
        myDisplay1.setBrightness(current_brightness);
        myDisplay2.setBrightness(current_brightness);
        myDisplay3.setBrightness(current_brightness);
      } else if (DOWNButtonState && !lastDOWNButtonState) {
        if(current_brightness-1!=0){
          current_brightness--;
        }
        myDisplay1.setBrightness(current_brightness);
        myDisplay2.setBrightness(current_brightness);
        myDisplay3.setBrightness(current_brightness);
      }
      }

      myDisplay1.print("                ");
      char bufferbright[21]; // With extra for null terminator
      snprintf(bufferbright, sizeof(bufferbright), "        SET GLOW- %02d", current_brightness);
      myDisplay2.print(bufferbright);
      myDisplay3.print("                    ");
      Serial.println("set brightness");
      break;
    case set_current:

      if (CENTERButtonState && !lastCENTERButtonState) {
        if (menu_engaged) {
          menu_engaged = false;
        } else{
          menu_engaged = true;
        }
      }
      if (menu_engaged) {
      if (UPButtonState && !lastUPButtonState) {
        if(current_current+1!=16){
          increaseCurrent();
        }
        myDisplay1.setCurrent(current_current);
        myDisplay2.setCurrent(current_current);
        myDisplay3.setCurrent(current_current);
      } else if (DOWNButtonState && !lastDOWNButtonState) {
        if(current_current-1!=0){
          decreaseCurrent();;
        }
        myDisplay1.setCurrent(current_current);
        myDisplay2.setCurrent(current_current);
        myDisplay3.setCurrent(current_current);
      }
      }
      
      myDisplay1.print("                ");
      char bufferCurr[21]; // With extra for null terminator
      const char* current_current_display;
      if (current_current == 48) {
        current_current_display = "12.8";
      } else if (current_current == 32) {
        current_current_display = " 4.0";
      } else if (current_current == 16) {
        current_current_display = " 6.4";
      } else if (current_current == 0) {
        current_current_display = " 9.3";
      } else {
        current_current_display = "UNDF";
      }
      
      snprintf(bufferCurr, sizeof(bufferCurr), "        SET mAMP%s", current_current_display);
      myDisplay2.print(bufferCurr);
      myDisplay3.print("                    ");
      Serial.println("set current");
      break;
    case set_time:
      break;
  }
  Serial.println(current_menu);

  
  
  
  myDisplay1.printDirectBufferOverlay(animation_buffer, 80);
  myDisplay2.printDirectBufferOverlay(&animation_buffer[80], 100);
  myDisplay3.printDirectBufferOverlay(&animation_buffer[180], 100);
  

  
  
  myDisplay1.refreshDisplay();
  myDisplay2.refreshDisplay();
  myDisplay3.refreshDisplay();
  if (animation_frame>0){
  if(delta_accumulator >= 25000 || animation_frame==1) { // If more than 33ms has passed or animation is just started
    delta_accumulator = 0;
    if (animation_frame>0) {
      animation_frame++;
      renderWaterfallFrame2();
      if (animation_frame == 90){
        showText(true);
      }
      finish_at_frame(300);
    }

  } else {
    delta_accumulator += delta_time;
  }
  }

  //delay(33);
  
  

  lastUPButtonState = UPButtonState;
  lastDOWNButtonState = DOWNButtonState;
  lastCENTERButtonState = CENTERButtonState;
  loop_stop = micros();
  delta_time = loop_stop-loop_start;
}

void startAnimation(){
  animation_frame = 1;
}

void finish_at_frame(uint32_t last_frame) {
  if (animation_frame >= last_frame) {
    animation_frame = 0;
    showText(false);
  }
}

void renderWaterfallFrame2() {
  if (animation_frame > 0) {

    for (int display_number=0; display_number<14; display_number++) { // For all displays
      shiftDown(&animation_buffer[(display_number*20)]); // Shift down pixels on all displays
    }
    for (int display_number=0; display_number<14; display_number++) { // For all displays
      carryPixelsOver(&animation_buffer[(display_number*20)], &animation_buffer[(display_number*20)+20]); // Carry pixels over from bottom of one display to the top of another
    }

    // For the first display
    for (int i = 0; i < 3; i++) { // For every character
      for (int ii = 0; ii < 20; ii++) { // For every column in a display
        animation_buffer[ii] |= 0b00000001;
          if (animation_frame < 30 ) { // Frames <30
            if (random(40) > animation_frame ) {  // Chance to remove drop
              animation_buffer[ii] ^= 0b00000001;
            }
          } else {
            if (random(40) < -(30 - animation_frame) ) {  // Chance to remove drop
              animation_buffer[ii] ^= 0b00000001;
            }
          }
      }
    }
  }
}

void showText(bool show){
  if (show) {
    show_text = true;
  } else {
    show_text = false;
  }
}

void carryPixelsOver(uint8_t* source, uint8_t* dest){
  for(int i=0; i<20; i++){
    dest[i] |= (source[i] & 0b10000000) >> 7; // place carry into LSB of byte2
  }
}

void shiftDown(uint8_t* source){
  for(int i=0; i<20; i++){
    source[i] <<= 1; // place carry into LSB of byte2
  }
}
