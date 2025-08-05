/* -----------------------------------------------------------------
   HCMS39xx Library
   https://github.com/Andy4495/HCMS39xx

   07/11/21 - A.T. - Original

*/

#include "HCMS39xx.h"
#include "font5x7.h"

HCMS39xx::HCMS39xx(uint8_t num_chars, uint8_t data_pin, uint8_t rs_pin, uint8_t clk_pin, 
                   uint8_t ce_pin, uint8_t blank_pin, uint8_t osc_select_pin) {

    _num_chars      = num_chars;
    _data_pin       = data_pin; 
    _clk_pin        = clk_pin; 
    _rs_pin         = rs_pin; 
    _ce_pin         = ce_pin; 
    _blank_pin      = blank_pin; 
    _osc_select_pin = osc_select_pin; 

    _clkPort  = digitalPinToPort(_clk_pin);
    _dataPort = digitalPinToPort(_data_pin);
    _clkMask  = digitalPinToBitMask(_clk_pin);
    _dataMask = digitalPinToBitMask(_data_pin);

    _displayBuffer = new uint8_t[_num_chars * COLUMNS_PER_CHAR];

    pinMode(_data_pin, OUTPUT);
    pinMode(_clk_pin, OUTPUT); 
    pinMode(_rs_pin, OUTPUT); 
    pinMode(_ce_pin, OUTPUT); 

    digitalWrite(_clk_pin, LOW); 
    digitalWrite(_ce_pin, HIGH); 
    
    if (_blank_pin != NO_PIN) {
        pinMode(_blank_pin, OUTPUT); 
        digitalWrite(_blank_pin, HIGH); // default is for display to be blanked when initialized
    }
    if (_osc_select_pin != NO_PIN) {
        pinMode(_osc_select_pin, OUTPUT); 
    }
}

void HCMS39xx::begin() {
    uint8_t i; 

    // Set all dot values to LOW
    clear();

    // Set up the font
    _first_ascii_index = pgm_read_byte(&font5x7[0]) - 1; // Need to subtract 1 since the first entry in table is font meta-data

    // Per datasheet, load control word 0 with desired brightness and set sleep bit HIGH (awake)
    _control_word0 = WAKEUP | DEFAULT_BRIGHTNESS | DEFAULT_CURRENT; 
    setupControlData();
    for (i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
        sendByte(_control_word0);
    }
    endTransmission();

    _control_word1 = CONTROL_WORD1;
    setSimultaneousMode(); // This has the side-effect of setting the default value for control word 1
}



void HCMS39xx::print(int j) {
    char s[7]; // Largest  integer is 5 digits plus sign character plus null pointer

    snprintf(s, 7, "%d", j); 
    if (_num_chars > strlen(s)) clear(); // Clear the display if the number of characters is shorter than the display
    print(s);

}

void HCMS39xx::print(unsigned int j) {
    char s[6]; // Largest  integer is 5 digits plus null pointer

    snprintf(s, 6, "%d", j); 
    if (_num_chars > strlen(s)) clear(); // Clear the display if the number of characters is shorter than the display
    print(s);

}

void HCMS39xx::print(long j) {
    char s[12]; // Largest long integer is 10 digits plus sign character plus null pointer

    snprintf(s, 12, "%ld", j); 
    if (_num_chars > strlen(s)) clear(); // Clear the display if the number of characters is shorter than the display 
    print(s);

}

void HCMS39xx::print(unsigned long j) {
    char s[11]; // Largest unsigned long integer is 10 digits plus null pointer

    snprintf(s, 11, "%lu", j); 
    if (_num_chars > strlen(s)) clear(); // Clear the display if the number of characters is shorter than the display
    print(s); 
}

void HCMS39xx::printDirect(const uint8_t* s, uint8_t len) {
    uint8_t i; 

    setupDotData();
    for (i = 0; i < len; i++) {
        sendByte(s[i]);
    }
    endTransmission();
}

void HCMS39xx::clear() {
    memset(_displayBuffer, 0, _num_chars * COLUMNS_PER_CHAR);

    //setupDotData(); // No need to wipe display since re-writing it does that anyway
    //for (uint8_t i = 0; i < _num_chars * COLUMNS_PER_CHAR; i++) {
    //    sendByte(0);
    //}
    //endTransmission(); 
}

void HCMS39xx::print(const char* s) {
    uint8_t i; 

    //setupDotData(); // Start transmission
    for (i = 0; i < _num_chars; i++) { // Don't loop for more chars than defined for the display object
        if (s[i] != 0) {
        sendFontData(font5x7 + (uint8_t)(s[i] - _first_ascii_index) * (uint16_t)COLUMNS_PER_CHAR, COLUMNS_PER_CHAR, i);
        }
        else  { // If we find a NULL terminator, then break out of loop
            break;
        }
    }
    //endTransmission();
}

void HCMS39xx::printBuffer(const char* s) {
    // Clear buffer first (optional, depending on overlay usage)
    memset(_displayBuffer, 0, _num_chars * COLUMNS_PER_CHAR);

    for (uint8_t charPos = 0; charPos < _num_chars; charPos++) {
        // Stop if end of string reached
        if (s[charPos] == '\0') break;

        // Calculate font table pointer
        const uint8_t* fontPtr = font5x7 + 5 + ((uint8_t)(s[charPos] - _first_ascii_index)) * COLUMNS_PER_CHAR;

        for (uint8_t col = 0; col < COLUMNS_PER_CHAR; col++) {
            // Read font column from PROGMEM
            uint8_t fontColumn = pgm_read_byte(&fontPtr[col]);

            // Calculate index in display buffer
            uint8_t bufferIndex = charPos * COLUMNS_PER_CHAR + col;

            // Store font column into buffer
            _displayBuffer[bufferIndex] = fontColumn;
        }
    }
}

void HCMS39xx::copyCharToBuffer(uint8_t charIndex, const uint8_t* fontPtr) {
    for (uint8_t col = 0; col < COLUMNS_PER_CHAR; col++) {
        uint8_t fontColumn = pgm_read_byte(&fontPtr[col]);
        uint8_t index = charIndex * COLUMNS_PER_CHAR + col;
        _displayBuffer[index] = fontColumn;
    }
}

void HCMS39xx::printDirectBufferOverlay(const uint8_t* s, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        _displayBuffer[i] |= s[i];   // store direct pattern into buffer
    }
}

void HCMS39xx::printDirectBufferXOR(const uint8_t* s, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        _displayBuffer[i] ^= s[i];   // store direct pattern into buffer
    }
}

void HCMS39xx::refreshDisplay() {
    uint8_t col;
    setupDotData();

    for (col = 0; col < _num_chars * COLUMNS_PER_CHAR; col++) {
        sendByte(_displayBuffer[col]);
    }
    endTransmission();
}

void HCMS39xx::displaySleep() {
    _control_word0 = _control_word0 & ~SLEEP_MASK; 

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // simultaneous mode, then only need to send once
        sendByte(_control_word0);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word0);
        }
    }
    endTransmission();
}

void HCMS39xx::displayWakeup() {
    _control_word0 = _control_word0 | WAKEUP; 

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // simultaneous mode, then only need to send once
        sendByte(_control_word0);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word0); 
        }
    }
    endTransmission();
}

void HCMS39xx::displayBlank() {
    if (_blank_pin != NO_PIN) {
        digitalWrite(_blank_pin, HIGH);
    }
}

void HCMS39xx::displayUnblank() {
    if (_blank_pin != NO_PIN) {
        digitalWrite(_blank_pin, LOW); 
    }
}

void HCMS39xx::setBrightness(uint8_t value) {
    _control_word0 = (_control_word0 & ~BRIGHTNESS_MASK) | (value & BRIGHTNESS_MASK); 

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // simultaneous mode, then only need to send once
        sendByte(_control_word0);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word0); 
        }
    }
    endTransmission();
}

void HCMS39xx::setCurrent(DISPLAY_CURRENT value) {
    _control_word0 = (_control_word0 & ~PIXEL_CURRENT_MASK) | (value & PIXEL_CURRENT_MASK); 

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // simultaneous mode, then only need to send once
        sendByte(_control_word0);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word0); 
        }
    }
    endTransmission();
}

void HCMS39xx::setExtOsc() {
    if (_osc_select_pin != NO_PIN) {
        digitalWrite(_osc_select_pin, LOW); 
    }
}

void HCMS39xx::setIntOsc() {
    if (_osc_select_pin != NO_PIN) {
        digitalWrite(_osc_select_pin, HIGH); 
    }
}

void HCMS39xx::setExternalPrescaleDiv8() {
    _control_word1 = _control_word1 | EXT_PRESCALER_DIV8; 

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // simultaneous mode, then only need to send once
        sendByte(_control_word1);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word1); 
        }
    }
    endTransmission();
}

void HCMS39xx::setExternalPrescaleNormal() {
    _control_word1 = _control_word1 & ~EXT_PRESCALER_DIV8;

    setupControlData();
    if (_control_word1 & DATA_OUT_MODE_SIMUL) {  // Simultaneous mode, then only need to send once
        sendByte(_control_word1);
    }
    else { // Serial mode, then need to send for each driver chip
        for (int i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
            sendByte(_control_word1); 
        }
    }
    endTransmission();
}

void HCMS39xx::setSimultaneousMode() {
    uint8_t i; 
    
    _control_word1 = _control_word1 | DATA_OUT_MODE_SIMUL; 
    setupControlData();
    for (i = 0; i < _num_chars / CHARS_PER_DEVICE; i++) {
        sendByte(_control_word1); 
    }
    endTransmission();
}

void HCMS39xx::setSerialMode() {
    _control_word1 = _control_word1 & ~DATA_OUT_MODE_SIMUL;

    // Assume we are in simultaneous mode, so only send once
    setupControlData();
    sendByte(_control_word1); 
    endTransmission();
}

void HCMS39xx::setupDotData() {
    digitalWrite(_clk_pin, HIGH); 
    digitalWrite(_rs_pin, LOW); 
    digitalWrite(_ce_pin, LOW); 
}

void HCMS39xx::setupControlData() {
    digitalWrite(_clk_pin, HIGH); 
    digitalWrite(_rs_pin, HIGH); 
    digitalWrite(_ce_pin, LOW); 
}

void HCMS39xx::endTransmission() {
    digitalWrite(_ce_pin, HIGH);
    delayMicroseconds(1);
    digitalWrite(_clk_pin, LOW); 
    
}

void HCMS39xx::sendFontData(const uint8_t *b, uint8_t length,  uint8_t char_num) {
    uint8_t i; 
    uint8_t data; 

    for (i = 0; i < length; i++) {
        data = pgm_read_byte(&b[i]);
        //sendByte(data);
        _displayBuffer[(char_num * 5) + i] = data;
    }
}

uint8_t HCMS39xx::byteBuffer(uint8_t index) {
    return _displayBuffer[index];
}

void HCMS39xx::loadFontDataToBuffer(const uint8_t *b, uint8_t length) {
    uint8_t i; 
    uint8_t data; 

    //for (i = 0; i < length; i++) {
    //    data = pgm_read_byte(&b[i]);
    //    _displayBuffer[i] =data; 
    //}
    _displayBuffer[0] = 0x7F;  // column 1 of char 0
    _displayBuffer[1] = 0x7F;  // column 2 of char 0
    _displayBuffer[2] = 0x7F;  // column 3 of char 0
    _displayBuffer[3] = 0x7F;  // column 4 of char 0
    _displayBuffer[4] = 0x7F;  // column 5 of char 0
}

void HCMS39xx::sendByte(uint8_t b) {
    uint8_t i; 

    for (i = 0; i < 8; i++) {
        digitalWrite(_clk_pin, LOW);
        digitalWrite(_data_pin, (b & 0x80)); // msb first
        digitalWrite(_clk_pin, HIGH);
        b = b << 1; 
    }
}


/*
void HCMS39xx::sendByte(uint8_t b) {
    

    for (int i = 0; i < 8; i++) {
        // CLK LOW
        _clkPort->BRR = _clkMask;

        // DATA: write MSB
        _dataPort->BSRR = (_dataMask << ((!(b & 0x80)) * 16));

        // CLK HIGH
        _clkPort->BSRR = _clkMask;

        b <<= 1;
    }
}
    */