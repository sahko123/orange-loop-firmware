#include "HCMS39xx.h"
#include "font5x7.h"
#include <STM32RTC.h>

STM32RTC& rtc = STM32RTC::getInstance();

// HCMS39xx(uint8_t num_chars, uint8_t data_pin, uint8_t rs_pin, uint8_t clk_pin, uint8_t ce_pin, uint8_t blank_pin)
HCMS39xx myDisplay1(16, PA7, PA5, PA4, PA6, PA8); // osc_select_pin tied high
HCMS39xx myDisplay2(20, PA3, PA5, PA4, PA2, PA8); // osc_select_pin tied high
HCMS39xx myDisplay3(20, PA1, PA5, PA4, PA0, PA8); // osc_select_pin tied high

#define MAXLEN 4
#define UP_BTN PA9
#define CENTER_BTN PA10
#define DOWN_BTN PB5
#define CHRG_DETECT PB4
#define CLASP_DETECT PB0

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
int8_t current_timezone_offset = 0;
uint8_t selected_animation = 0;

bool HAS_BEEN = 0;

int hour;
int minute;
int second;
int day_of_week;
int year;
int month;
int day;
int year_day;

const char* day_strings[] = {"D SN", "D MN", "D TU", "D WE", "D TH", "D FR", "D SA"};


enum Menu {show_mini_time, show_home, show_time, set_brightness, set_current, set_time_zone_offset, wave_mode, NUM_STATES};
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
uint8_t current_brightness = 15;

bool lastUPButtonState = true; // Default high to prevent startup animation
bool lastDOWNButtonState = true;
bool lastCENTERButtonState = true;
bool UPButtonState = false;
bool DOWNButtonState = false;
bool CENTERButtonState = false;

bool menu_engaged = false;

void setup() {
  pinMode(UP_BTN, INPUT_PULLDOWN);
  pinMode(CENTER_BTN, INPUT_PULLDOWN);
  pinMode(DOWN_BTN, INPUT_PULLDOWN);
  pinMode(CHRG_DETECT, INPUT_PULLUP);
  pinMode(CLASP_DETECT, INPUT_PULLUP);

  for (uint32_t i = 0; i < 280; i++) {
    testGrid[i] = 0b11111111;
  }
  
  colon_layer_buffer[110] = 0b00100100;
  
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
  rtc.begin(0);
  if (!rtc.isTimeSet()) {
    uint32_t unixTime = 1752700800UL; // 17 Jul 2025 00:00:00 UTC
    rtc.setEpoch(unixTime);
  }

  delay(500);
}

void loop() {
  loop_start = micros();
  
  //myDisplay1.begin();
  //myDisplay1.displayUnblank();
  //myDisplay1.setSerialMode();
  //myDisplay1.setBrightness(current_brightness);
  //myDisplay1.setCurrent(current_current);

  //myDisplay2.begin();
  //myDisplay2.displayUnblank();
  //myDisplay2.setSerialMode();
  //myDisplay2.setBrightness(current_brightness);
  //myDisplay2.setCurrent(current_current);

  //myDisplay3.begin();
  //myDisplay3.displayUnblank();
  //myDisplay3.setSerialMode();
  //myDisplay3.setBrightness(current_brightness);
  //myDisplay3.setCurrent(current_current);
  

  UPButtonState = digitalRead(UP_BTN);
  DOWNButtonState = digitalRead(DOWN_BTN);
  CENTERButtonState = digitalRead(CENTER_BTN);

  //Serial.println(digitalRead(CHRG_DETECT));
  //Serial.println(digitalRead(CLASP_DETECT));

  if (Serial.available()) {
    // Read the incoming string until newline
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove any trailing \r or whitespace

    if (input == "bootloader") {
      enterBootloader();  // jump to DFU
    } else if (input == "reset") {
      NVIC_SystemReset(); // core reset
    } else {
      // Otherwise, treat it as an epoch time
      time_t newTime = (time_t)input.toInt() + (current_timezone_offset * 3600);
      rtc.setEpoch(newTime);

      Serial.print("Time set to: ");
      Serial.println(newTime);
    }
  }
  uint32_t now = rtc.getEpoch();
  time_t t = now;
  struct tm* tmstruct = localtime(&t);

  hour = tmstruct->tm_hour;
  minute = tmstruct->tm_min;
  second = tmstruct->tm_sec;
  day_of_week = tmstruct->tm_wday;
  year = tmstruct->tm_year + 1900; // <- Fix: convert to full year
  month = tmstruct->tm_mon + 1;    // <- Fix: human-friendly month
  day = tmstruct->tm_mday;
  year_day = tmstruct->tm_yday;

  

  myDisplay1.clear();
  myDisplay2.clear();
  myDisplay3.clear();

  if (CENTERButtonState && !lastCENTERButtonState && animation_frame==0 && !menu_engaged) {
    ;
  } else if (UPButtonState && !lastUPButtonState && !menu_engaged) {
    current_menu = static_cast<Menu>((current_menu + 1) % NUM_STATES);
    clear_animation_buffer();
    animation_frame = 0;
  } else if (DOWNButtonState && !lastDOWNButtonState && !menu_engaged) {
    current_menu = static_cast<Menu>((current_menu + NUM_STATES - 1) % NUM_STATES);
    clear_animation_buffer();
    animation_frame = 0;
  }

  switch (current_menu) {
    case show_mini_time:
      display_show_minitime();
      break;
    case show_home:
      display_show_home();
      break;
    case show_time:
      display_show_time();
      break;
    case set_brightness:
      display_set_brightness();
      break;
    case set_current:
      display_set_current();
      break;
    case set_time_zone_offset:
      display_set_time_zone_offset();
      break;
    case wave_mode:
      display_wave_mode();
      break;
  }

  if (!digitalRead(CHRG_DETECT)) {
    myDisplay1.print("            CHRG");
  }
  //Serial.println(HAS_BEEN);
  
  
  myDisplay1.printDirectBufferOverlay(&animation_buffer[200], 80);
  myDisplay2.printDirectBufferOverlay(&animation_buffer[100], 100);
  myDisplay3.printDirectBufferOverlay(&animation_buffer[0], 100);
  //myDisplay1.printDirectBufferOverlay(&testGrid[200], 80);
  //myDisplay2.printDirectBufferOverlay(&testGrid[100], 100);
  //myDisplay3.printDirectBufferOverlay(&testGrid[0], 100);

  myDisplay1.refreshDisplay();
  myDisplay2.refreshDisplay();
  myDisplay3.refreshDisplay();
  
  
  if (animation_frame>0){
  if(delta_accumulator >= 25000 || animation_frame==1) { // If more than 33ms has passed or animation is just started
    delta_accumulator = 0;
    if (animation_frame>0) {
      animation_frame++;
      switch (selected_animation) {
        case 0:
          renderWaterfallFrame2();
          if (animation_frame == 90){
            showText(true);
          }
          finish_at_frame(300);
          break;
        case 1:
          renderWaterfallFrame3();
          finish_at_frame(300);
          break;
        break;
      }
      
      

    }

  } else {
    delta_accumulator += delta_time;
  }
  }

  
  
  

  lastUPButtonState = UPButtonState;
  lastDOWNButtonState = DOWNButtonState;
  lastCENTERButtonState = CENTERButtonState;
  loop_stop = micros();
  delta_time = loop_stop-loop_start;
  //delay(500);
}

void startAnimation(uint8_t animation){
  animation_frame = 1;
  selected_animation = animation;
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

void renderWaterfallFrame3() {
  if (animation_frame > 0) {

    for (int display_number=0; display_number<14; display_number++) { // For all displays
      shiftDown(&animation_buffer[(display_number*20)]); // Shift down pixels on all displays
    }
    for (int display_number=0; display_number<14; display_number++) { // For all displays
      if (display_number != 4){
        carryPixelsOver(&animation_buffer[(display_number*20)], &animation_buffer[(display_number*20)+20]); // Carry pixels over from bottom of one display to the top of another
      } else {
        carryPixelsOver(&animation_buffer[(display_number*20)], &animation_buffer[((display_number+3)*20)+20]); // Carry pixels over from bottom of one display to the top of another
      }
    }

    // For the first display
    for (int i = 0; i < 3; i++) { // For every character
      for (int ii = 0; ii < 20; ii++) { // For every column in a display
        animation_buffer[ii] |= 0b00000001;
        if (random(100) < 75 ) {  // Chance to remove drop
          animation_buffer[ii] ^= 0b00000001;
        }
            
      }
    }
    memset(&animation_buffer[100], 0, 60);
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


void enterBootloader() {
  void (*SysMemBootJump)(void);
  uint32_t bootloaderAddr = 0x1FFF0000;  // STM32U083 system memory start

  // De-initialize peripherals (especially USB if active)
  Serial.end();         // if Serial is over USB
  //USBDevice.detach();   // detach USB to allow bootloader to re-enumerate

  delay(100);  // Give host time to process USB detach

  __disable_irq();  // Disable all interrupts

  // Set stack pointer and jump to system memory reset handler
  __set_MSP(*(uint32_t *)bootloaderAddr);
  SysMemBootJump = (void (*)(void)) (*((uint32_t *)(bootloaderAddr + 4)));
  SysMemBootJump();
}

void display_show_minitime(){
  char buffertimeMini[21]; // With extra for null terminator
      snprintf(buffertimeMini, sizeof(buffertimeMini), "    %02d%02d", hour+daylight_savings, minute);
      myDisplay2.print(buffertimeMini);
      
      if(delta_accumulator < 1000000) { // If more than 33ms has passed or animation is just started
        if (delta_accumulator > 500000) {
          myDisplay2.printDirectBufferXOR(&colon_layer_buffer[80], 100);

        } else {
          
        }
        delta_accumulator += delta_time;
      } else {
        delta_accumulator = 0;
      }
}

void display_show_home(){
  
      if (CENTERButtonState && !lastCENTERButtonState) {
        startAnimation(0);
      }
      if (show_text) {
        myDisplay1.print("                ");
        char buffer[21]; // With extra for null terminator
        snprintf(buffer, sizeof(buffer), "H %02dM %02dS %02d", hour+daylight_savings, minute, second);
        myDisplay2.print(buffer);
        myDisplay3.print("                    ");
      }
      //Serial.println("show home");
}

void display_show_time(){
  
      //myDisplay1.print("                ");
      char buffertime3[21]; // With extra for null terminator
      snprintf(buffertime3, sizeof(buffertime3), "H %02dM %02dS %02dD %02dY %02d", hour, minute, second, day, year-2000);
      myDisplay3.print(buffertime3);
      char buffertime2[21];
      snprintf(buffertime2, sizeof(buffertime2), "M %02dD %02dH %02dM %02dS %02d", month, day, hour+daylight_savings, minute, second);
      myDisplay2.print(buffertime2);
      char buffertime1[17];
      snprintf(buffertime1, sizeof(buffertime1), "%sH %02dM %02dS %02d", day_strings[day_of_week], hour, minute, second);
      myDisplay1.print(buffertime1);
      //Serial.println("show time");
}

void display_set_brightness(){
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
      //Serial.println("set brightness");
}

void display_set_current(){
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
      //Serial.println("set current");
}

void display_set_time_zone_offset(){
  if (CENTERButtonState && !lastCENTERButtonState) {
        if (menu_engaged) {
          menu_engaged = false;
        } else{
          menu_engaged = true;
        }
      }
      if (menu_engaged) {
      if (UPButtonState && !lastUPButtonState) {
        if(current_timezone_offset+1!=24){
          current_timezone_offset ++;
          rtc.setEpoch(rtc.getEpoch()+3600);
        }
        
        
      } else if (DOWNButtonState && !lastDOWNButtonState) {
        if(current_current-1!=-24){
          current_timezone_offset --;
          rtc.setEpoch(rtc.getEpoch()-3600);
        }
        
      }
      }
      
      myDisplay1.print("                ");
      char bufferTimezone[21]; // With extra for null terminator
      
      snprintf(bufferTimezone, sizeof(bufferTimezone), "        SET ZONE%02d", current_timezone_offset);
      myDisplay2.print(bufferTimezone);
      myDisplay3.print("                    ");
}

void display_wave_mode(){
  if (animation_frame == 0) {
        startAnimation(1);
      }
      myDisplay1.print("                ");
      char bufferwave[21]; // With extra for null terminator
      snprintf(bufferwave, sizeof(bufferwave), "H %02dM %02dS %02d", hour+daylight_savings, minute, second);
      myDisplay2.print(bufferwave);
      myDisplay3.print("                    ");
      
      //Serial.println("show home");
}

void clear_animation_buffer(){
  memset(animation_buffer, 0, 280);
}
