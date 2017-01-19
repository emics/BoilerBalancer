//////////////////////////////////////////////////////
//                                                  //
//        Balance Boiler 1.05                       //
//                                                  //
//////////////////////////////////////////////////////

// INCLUSIONS
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <EasyTransfer.h>


// DEFINITIONS
#define _DEBUG_                   true
#define _TXSERIAL_                false
#define VERSION                   1.05
#define NUMBOILER                 6                      // number of boiler [min 2 max 6]
#define ONE_WIRE_BUS              9                      // one wire pin DS18B20
const int RelayPin[]              = {2, 3, 4, 5, 6, 7};  // relay attached to this pin {A, B, C, D, E, F}
DeviceAddress TempAddress[]       = {
  { 0x28, 0xFF, 0x31, 0xFA, 0x83, 0x16, 0x03, 0x49 },    // A
  { 0x28, 0xFF, 0xD9, 0xF7, 0x83, 0x16, 0x03, 0x30 },    // B
  { 0x28, 0xFF, 0xD6, 0x56, 0x63, 0x16, 0x03, 0xBE },    // C
  { 0x28, 0xFF, 0x2B, 0x81, 0x63, 0x16, 0x03, 0x73 },    // D  
  { 0x28, 0xFF, 0x4F, 0x7C, 0x63, 0x16, 0x03, 0x91 },    // E
  { 0x28, 0xFF, 0x7E, 0xC9, 0x73, 0x16, 0x04, 0x01 }     // F
};

//////////////////////////////////////////////////////
/////////////// END configuration Part ///////////////
//////////////////////////////////////////////////////

#define MODE_ALL_OFF              0                      // Mode all relay OFF
#define MODE_ALL_ON               1                      // Mode all relay ON
#define MODE_AUTO                 2                      // Mode Automatic
#define ITEM_OFF                  0                      // Menu Item OFF
#define ITEM_ON                   1                      // Menu Item ON
#define ITEM_MODE                 2                      // Menu Item Mode
#define OPEN_VALVE                HIGH                   // relay OFF valve OPEN
#define CLOSE_VALVE               LOW                    // relay ON valve CLOSE

// EEPROM                         
#define DEFAULTON                 60                     // default centigrade temp for on relay
#define DEFAULTOFF                40                     // default centigrade temp for off relay
#define EEMAGICADDR               0                      // eeprom address of magic byte
#define EEMAGICVALUE              78                     // eeprom value of magic byte
#define EETEMP_OFF_ADDR           2                      // eeprom address of OFF temperaure
#define EETEMP_ON_ADDR            3                      // eeprom address of ON temperaure
#define EEMODE_ADDR               4                      // eeprom MODE address (save current active MODE)

// BUTTON                         
#define BTNUP                     0
#define BTNDWN                    1
#define BTNSEL                    2

//DS18B20                         
#define TEMPERATURE_PRECISION     9

// system                         
int SelectedItem                  = ITEM_MODE;           // 0 = ITEM_OFF     1 = ITEM_ON    2 = ITEM_MODE
int ActiveMode                    = MODE_AUTO;           // default MODE
const char LetterBoiler[]         = {"ABCDEF"};
const String ModeName[]           = {"ALL OFF","ALL ON"," AUTO"};
const long DebounceDelay          = 50;                  // read button status with delay 50ms 
const long TimerSave              = 30000;               // save data 30 seconds after last push button
const long TimerStandby           = 60000;               // go in standby 1 minute after last push button
const long TimerTemperature       = 1000;                // read temperature every 1 second
const long TimerSerialData        = 10000;               // send data temperature on serial line every 10 seconds
const long TimerSwitch            = 10000;               // switch one relay at least after 10 seconds from other relay switch
unsigned long currentMillis       = 0;
unsigned long LastDebounce        = 0;                   // timer to manage buttons debounce
unsigned long LastReadTemperature = TimerTemperature + 1;// last time temperature is readed from sensors
unsigned long LastAction          = TimerStandby + 1;    // last time button is pressed
unsigned long LastSendData        = 0;
//unsigned long LastSwitch          = TimerSwitch + 1;     // init at high value for start switching immediatly after power on
boolean Saved                     = false;
boolean StandBy                   = true;

//  buttons                       
const int ButtonPin[]             = {10, 11, 12};        // push button is attached to this pin
int ButtonState[]                 = {HIGH,HIGH,HIGH};
boolean ButtonActive[]            = {false,false,false};

// relay                          
boolean RelayStatus[]             = {false, false, false, false, false, false};
boolean RelaySafeMode[]           = {false, false, false, false, false, false};

// temperature                    
int OnTemp                        = DEFAULTON;
int OffTemp                       = DEFAULTOFF;
int TempValue[]                   = {0,0,0,0,0,0};

// LCD                            
#define I2C_ADDR                  0x20
#define BACKLIGHT_PIN             7
#define En_pin                    4
#define Rw_pin                    5
#define Rs_pin                    6
#define D4_pin                    0
#define D5_pin                    1
#define D6_pin                    2
#define D7_pin                    3
const int startPositionLCD[][2]   = {{0 ,0}, {7 ,0}, {14,0}, {0 ,1}, {7 ,1}, {14,1}};  // column and row position of each temp value on LCD 

// custom char
byte customDeg[8]                 = {B01000, B10100, B01000, B00011, B00100, B00100, B00011, B00000};
byte customON[8]                  = {B00000, B00000, B01100, B11110, B11110, B01100, B00000, B00000};
byte customUpArrow[8]             = {B00100, B01110, B10101, B00100, B00100, B00100, B00000, B00000};


// DECLARATION
// DS18B20 library
OneWire                           oneWire(ONE_WIRE_BUS);
DallasTemperature                 sensors(&oneWire);
// LCD Library
LiquidCrystal_I2C                 lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin); 
// EasyTransfer object
EasyTransfer                      ET;

struct SEND_DATA_STRUCTURE{
  int TempValue[];
  boolean RelayStatus[];
  int ActiveMode;
};

SEND_DATA_STRUCTURE txData;

//
// SETUP
//
void setup(void) {
  Serial.begin(9600);
  if (_DEBUG_) {
    Serial.print("BOILER: ");
    Serial.println(NUMBOILER);
  }
  if (_TXSERIAL_) {
    ET.begin(details(txData), &Serial);
  }
  initEEprom();
  initRelay();
  initButton();
  initLCD();
  initSensor();
  delay(5000);
  clearLCDtop();
  checkChangeMode();
}

//
// LOOP
//
void loop(void) {
  currentMillis = millis();
  checkButtons();  
  scanSensors();
  refreshLCD();
  saveData();
  sendSerialData();
}

void tempControl(void) {
  if (ActiveMode == MODE_AUTO) {
    for (int i = 0; i < NUMBOILER; i++) {
      // check if some sensor are in error
      if (TempValue[i] == -127) {
        // sensor in error -  OPEN VALVE
        // doVALVE(i, OPEN_VALVE, false);
      }
      else { 
        if (RelayStatus[i]) {
          // Relay already ON valve OPEN
          if (!RelaySafeMode[i]) { // check if not in SafeMode
            if (TempValue[i] <= OffTemp) {   
              // CLOSE valve - the temperature is cold
              if (_DEBUG_) Serial.print("CLOSE valve: ");
              if (_DEBUG_) Serial.println(LetterBoiler[i]);
              doVALVE(i, CLOSE_VALVE, false);
            }
          }
          else {
            // Exit Safe Mode from Relay already ON
            // and Temperature is mucher than midTemp
            int midTemp = (OnTemp + OffTemp) / 2;
            if (TempValue[i] >= midTemp) exitSafeMode();  // exit from SafeMode
          }
        }
        else {
          // Relay already OFF valve CLOSE
          if (TempValue[i] >= OnTemp) {
            // OPEN valve - the temperature is HOT
            if (_DEBUG_) Serial.print("OPEN valve: ");
            if (_DEBUG_) Serial.println(LetterBoiler[i]);
            doVALVE(i, OPEN_VALVE, false);
            if (isSafeMode()) exitSafeMode();  // exit from SafeMode
          }
        }
      }
    }
    checkSafeMode();
  }
}

void initAutoMode(void) {
  if (ActiveMode == MODE_AUTO) {
    int midTemp = (OnTemp + OffTemp) / 2;
    if (_DEBUG_) Serial.print("Mid Temp: ");
    if (_DEBUG_) Serial.println(midTemp);
    for (int i = 0; i < NUMBOILER; i++) {
      if (TempValue[i] <= midTemp) {
        // CLOSE valve - the temperature is cold
        if (_DEBUG_) Serial.print("CLOSE valve: ");
        if (_DEBUG_) Serial.println(LetterBoiler[i]);
        doVALVE(i, CLOSE_VALVE, false);
      }
      else {
        // OPEN valve - the temperature is hot
        if (_DEBUG_) Serial.print("OPEN valve: ");
        if (_DEBUG_) Serial.println(LetterBoiler[i]);
        doVALVE(i, OPEN_VALVE, false);
      }
    }
  }
}

void checkSafeMode(void) {
  if (ActiveMode == MODE_AUTO) {    
    if (isAllClose()) {
      // if all VALVE are CLOSED 
      // OPEN the most hot
      int pos = foundMaxTemp();
      doVALVE(pos, OPEN_VALVE, true);
    }
    else {
      if (isSafeMode()) {
        //  already in safe mode 
        //  check if there are other HOT boiler
        //  and OPEN it
        int pos = foundMaxTemp();       
        for (int i = 0; i < NUMBOILER; i++) {
          if (RelaySafeMode[i]) {
            if (i != pos) {
              if ((unsigned int)(TempValue[i] - TempValue[pos]) > 3) {
                doVALVE(pos, OPEN_VALVE, true);
                doVALVE(i, CLOSE_VALVE, true);
                i = NUMBOILER;  // exit
              }
            }
          }
        } 
      }
    }
  }
}

boolean isSafeMode(void) {
  // check if all VALVE are not in Safe Mode (false)
  boolean tmp = false;
  for (int i = 0; i < NUMBOILER; i++) {
    if (RelaySafeMode[i]) {
      tmp = true;     // Found one VALVE in Safe Mode
      i = NUMBOILER;    // exit
    }
  }
  if (_DEBUG_) {
    if (tmp) Serial.println("Safe Mode");
  }
  return tmp;
}

void exitSafeMode(void) {
  // exit from Safe Mode
  // set all Relay SafeMode  = false
  for (int i = 0; i < NUMBOILER; i++) {
    if (RelaySafeMode[i]) { // check if a VALVE is in SafeMode
      //doVALVE(i, CLOSE_VALVE, true);  // delete this line
      RelaySafeMode[i] = false;
    }
  }
}

boolean isAllClose(void) {
  // check if all VALVE are CLOSED (true)
  boolean tmp = true;
  for (int i = 0; i < NUMBOILER; i++) {
    if (RelayStatus[i]) {
      tmp = false;    // Found one VALVE OPEN
      i = NUMBOILER;    // exit
    }
  }
  if (_DEBUG_) {
    if (tmp) Serial.println("All Valve are Close");
  }
  return tmp;
}

int foundMaxTemp(void) {
  // found max sensor Temp
  int maxTemp   = -127; // store max sensor temp
  int maxNumber;        // store max sensor number
  for (int i = 0; i < NUMBOILER; i++) {
    if (TempValue[i] > maxTemp) {
      maxNumber = i;
      maxTemp = TempValue[i];
    }
  }
  if (_DEBUG_) {
    Serial.print("Max Temperature: ");
    Serial.println(maxNumber);
  }
  return maxNumber;
}

void switchMode(boolean reverse) {
  if (!reverse) {
    switch (ActiveMode) {
      case MODE_ALL_OFF:
        ActiveMode = MODE_ALL_ON;
        break;
      case MODE_ALL_ON:
        ActiveMode = MODE_AUTO;
        initAutoMode();
        break;
      case MODE_AUTO: 
        ActiveMode = MODE_ALL_OFF;
        break;
    }
  }
  else {
    switch (ActiveMode) {
      case MODE_ALL_OFF:
        ActiveMode = MODE_AUTO;
        initAutoMode();
        break;
      case MODE_ALL_ON:
        ActiveMode = MODE_ALL_OFF;
        break;
      case MODE_AUTO: 
        ActiveMode = MODE_ALL_ON;
        break;
    }
  }
  if (_DEBUG_) Serial.print("switchMode:");
  if (_DEBUG_) Serial.println(ActiveMode);
  if (_TXSERIAL_) txData.ActiveMode = ActiveMode;
  checkChangeMode();
}

void checkButtons(void){
  ButtonState[BTNSEL] = digitalRead(ButtonPin[BTNSEL]);
  ButtonState[BTNUP]  = digitalRead(ButtonPin[BTNUP]);
  ButtonState[BTNDWN] = digitalRead(ButtonPin[BTNDWN]);

  if ((unsigned long)(currentMillis - LastDebounce) > DebounceDelay){
    if (ButtonState[BTNSEL] == LOW){ 
      LastAction = currentMillis;
      if (!ButtonActive[BTNSEL]){
        if (_DEBUG_) Serial.println("Button SEL Pressed");
        ButtonActive[BTNSEL] = true;
        if (StandBy){
          StandBy = false;
        }
        else{
          if (ActiveMode == MODE_AUTO){
            switch (SelectedItem){
              case ITEM_OFF:
                // select ON ITEM
                SelectedItem = ITEM_ON;
                break;
              case ITEM_ON:
                // select MODE ITEM
                SelectedItem = ITEM_MODE;
                break;
              case ITEM_MODE: 
                // select MODE OFF
                SelectedItem = ITEM_OFF;
                break;
            }
            writeCursorLCD();
          }
        }
      }
    }
    else{
      if (ButtonActive[BTNSEL]){
        if (_DEBUG_) Serial.println("Button SEL Released");
        ButtonActive[BTNSEL] = false;
      }
    }

    if (ButtonState[BTNUP] == LOW){
      LastAction = currentMillis;
      if (!ButtonActive[BTNUP]){
        if (_DEBUG_) Serial.println("Button UP Pressed");
        ButtonActive[BTNUP] = true;
        if (StandBy){
        StandBy = false;
        }
        else{
          Saved = false;
          switch (SelectedItem){
            case ITEM_OFF:
              // increase OFF Temp
              if (OffTemp < 100 && OffTemp < OnTemp) OffTemp++;
              break;
            case ITEM_ON:
              // increase ON Temp
              if (OnTemp < 100) OnTemp++;
              break;
            case ITEM_MODE: 
              // change MODE
              switchMode(true);
              break;
          }
        }
      }
    }
    else{
    if (ButtonActive[BTNUP]){
    if (_DEBUG_) Serial.println("Button UP Released");
    ButtonActive[BTNUP] = false;
    }
    }

    if (ButtonState[BTNDWN] == LOW){
      LastAction = currentMillis;
      if (!ButtonActive[BTNDWN]){
        if (_DEBUG_) Serial.println("Button DWN Pressed");
        ButtonActive[BTNDWN] = true;
        if (StandBy){
        StandBy = false;
        }
        else{
          Saved = false;
          switch (SelectedItem){
            case ITEM_OFF:
              // decrease OFF Temp
              if (OffTemp > 25) OffTemp--;
              break;
            case ITEM_ON:
              // decrease ON Temp
              if (OnTemp > 25 && OnTemp > OffTemp) OnTemp--;
              break;
            case ITEM_MODE: 
              // change MODE
              switchMode(false);
              break;
          }
        }
      }
    }
    else{
      if (ButtonActive[BTNDWN] == true){    
        if (_DEBUG_) Serial.println("Button DWN Released");
        ButtonActive[BTNDWN] = false;
      }
    }    
    LastDebounce = currentMillis;
  }
}

void checkChangeMode(void) {
  // 0 = MODE_ALL_OFF     1 = MODE_ALL_ON      2 = MODE_AUTO     
  clearLCDbottom();
  switch (ActiveMode) {
    case MODE_ALL_OFF:
      if (isSafeMode()) exitSafeMode();  // exit from SafeMode
      writeWaitLCD();
      closeAllVALVE();
      break;
    case MODE_ALL_ON:
      if (isSafeMode()) exitSafeMode();  // exit from SafeMode
      writeWaitLCD();
      openAllVALVE();
      break;
    case MODE_AUTO: 
      initAutoMode();
      break;
  }
  if (_DEBUG_) Serial.print("Mode: ");
  if (_DEBUG_) Serial.println(ModeName[ActiveMode]);
}

void saveData(void) {
  if (((unsigned long)(currentMillis - LastAction) > TimerSave) && !Saved) {
    if (_DEBUG_) Serial.println("Saving Data");
    EEPROM.update(EETEMP_OFF_ADDR, OffTemp);
    EEPROM.update(EETEMP_ON_ADDR, OnTemp);
    EEPROM.update(EEMODE_ADDR, ActiveMode);
  Saved = true;
  }
}

// LCD Session
void refreshLCD(void) {
  if (StandBy) {
  screenSaver();
  }
  else {
    if ((unsigned long)(currentMillis - LastAction) > TimerStandby) {
    if (!StandBy) {
      screenSaver();
      StandBy = true;
    }
    }
    else {
    // write mode 
    writeModeLCD();
    // write sensor temperatures
    writeSensTempLCD();
    // write temperature thresold
    writeThersoldLCD();
    // write cursor
    writeCursorLCD();
    }
   }
}

void writeSensTempLCD(void) {
  for (int i = 0; i < NUMBOILER; i++) {
    lcd.setCursor(startPositionLCD[i][0],startPositionLCD[i][1]);
    lcd.print(LetterBoiler[i]);
    if (RelayStatus[i])
      lcd.write((uint8_t)1);  // ON Char
    else {
      lcd.print(" ");     // space for OFF char
    }
    if (TempValue[i] == -127) {
      lcd.print("err ");
    }
    else {
      if (TempValue[i] < 10) {
        lcd.print(" ");     // add a space for one digit number
      }
      lcd.print(TempValue[i]);
      lcd.write((uint8_t)0);    // degree char
    }
  }
}

void writeCursorLCD(void) {
  lcd.setCursor(7,3);
  lcd.print("     ");     // clear cursor space
  switch (SelectedItem) {
    case ITEM_OFF:
      lcd.setCursor(11,3);
      lcd.print((char)126); // right arrow
      break;
    case ITEM_ON:
      lcd.setCursor(7,3);
      lcd.print((char)127); // left arrow
      break;
    case ITEM_MODE: 
      lcd.setCursor(9,3);
      lcd.write((uint8_t)2); // up arrow
      break;
  }
}

void writeModeLCD(void) {
  // write Mode
  lcd.setCursor(7,2);
  lcd.print(ModeName[ActiveMode]);
  lcd.setCursor(14,2);
  if (isSafeMode()) {
    lcd.print("SAFE ");
    for (int i = 0; i < NUMBOILER; i++) {
      if (RelaySafeMode[i]) {
        lcd.print(LetterBoiler[i]);
      }
    }
  }
  else {
    lcd.print("      ");
  }
}

void writeWaitLCD(void) {
  // write WAIT
  lcd.setCursor(7,2);
  lcd.print(" WAIT ");
  lcd.setCursor(14,2);
  lcd.print("      ");
}

void writeThersoldLCD(void) {
  // write temp threshold if MODE_AUTO
  if (ActiveMode == MODE_AUTO) {
    lcd.setCursor(0,3);
    lcd.print("ON ");
    lcd.print(OnTemp);
    lcd.write((uint8_t)0);   // degree char
    lcd.setCursor(13,3);
    lcd.print("OFF ");
    lcd.print(OffTemp);
    lcd.write((uint8_t)0);   // degree char
  }
}

//  init Session
void initLCD(void) {
  if (_DEBUG_) Serial.println("initLCD");
  lcd.begin(20,4);
  //lcd.backlight();
  lcd.createChar(0, customDeg);
  lcd.createChar(1, customON);
  lcd.createChar(2, customUpArrow);  

  lcd.setCursor(0, 0);
  lcd.print("Powered by");
  lcd.setCursor(3, 1);
  lcd.print("C.D. Impianti");
  lcd.setCursor(5, 2);
  lcd.print("tel 392/0581809");
  lcd.setCursor(0, 3);
  lcd.print("E#");
  lcd.setCursor(15, 3);
  lcd.print("v");
  lcd.print(VERSION);
  lcd.home();
}

void initRelay(void) {
  if (_DEBUG_) Serial.println("initRelay");
  for (int i = 0; i < NUMBOILER; i++) {
    pinMode(RelayPin[i], OUTPUT);
  }
  closeAllVALVE();
}

void initButton(void) {
  if (_DEBUG_) Serial.println("initButton");
  pinMode(ButtonPin[BTNSEL], INPUT_PULLUP);
  pinMode(ButtonPin[BTNDWN], INPUT_PULLUP);
  pinMode(ButtonPin[BTNUP],  INPUT_PULLUP);
}

void initEEprom(void) {
  if (_DEBUG_) Serial.println("initEEPROM");
  if (EEPROM.read(EEMAGICADDR) == EEMAGICVALUE) {
    // already inizialized
    if (_DEBUG_) Serial.println("EEPROM already inizialized");
    OnTemp     = EEPROM.read(EETEMP_ON_ADDR);
    OffTemp    = EEPROM.read(EETEMP_OFF_ADDR);
    ActiveMode = EEPROM.read(EEMODE_ADDR);
    if (_DEBUG_) Serial.print("EEPROM OnTemp: ");
    if (_DEBUG_) Serial.println(OnTemp);
    if (_DEBUG_) Serial.print("EEPROM OffTemp: ");
    if (_DEBUG_) Serial.println(OffTemp);    
    if (_DEBUG_) Serial.print("EEPROM Mode: ");
    if (_DEBUG_) Serial.println(ActiveMode);
  }
  else {
    // first boot never inizialized
    if (_DEBUG_) Serial.println("First Boot");
    EEPROM.update(EEMAGICADDR, EEMAGICVALUE);
    EEPROM.update(EETEMP_ON_ADDR, OnTemp);
    EEPROM.update(EETEMP_OFF_ADDR, OffTemp);
    EEPROM.update(EEMODE_ADDR, ActiveMode);
  }
}

void initSensor(void) {
  if (_DEBUG_) Serial.println("initSensor");
  if (_DEBUG_) Serial.print("Found ");
  if (_DEBUG_) Serial.print(sensors.getDeviceCount(), DEC);
  if (_DEBUG_) Serial.println(" devices.");
  for (int i = 0; i < NUMBOILER; i++) {
    sensors.setResolution(TempAddress[i], TEMPERATURE_PRECISION);
  }
}

//  Temperature Sensor Session
void scanSensors(void) {
  // scan temperature sensors every interval TimerTemperature
  if((unsigned long)(currentMillis - LastReadTemperature) > TimerTemperature) {
    sensors.requestTemperatures();    
    for (int i = 0; i < NUMBOILER; i++) {
      TempValue[i] = (int) sensors.getTempC(TempAddress[i]);
      if (_TXSERIAL_) txData.TempValue[i] = TempValue[i];
      if (_DEBUG_) Serial.print(LetterBoiler[i]);
      if (_DEBUG_) Serial.print(": ");
      if (_DEBUG_) Serial.print(TempValue[i]);
      if (_DEBUG_) Serial.print("C - ");
    }
    if (_DEBUG_) Serial.println(" ");
    LastReadTemperature = currentMillis;
    tempControl();
  }
}

// LCD Session
void screenSaver() {
  writeModeLCD();
  writeSensTempLCD();
  lcd.setCursor(0,3);
  lcd.print("                    ");
}

void clearLCDtop(void) {
  lcd.setCursor(0,0);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("                    ");
}

void clearLCDbottom(void) {
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
}

// Relay Session
void closeAllVALVE(void) {
  if (_DEBUG_) Serial.println("closeAllVALVE");
  for (int i = 0; i < NUMBOILER; i++) {
    doVALVE(i, CLOSE_VALVE, false);
    writeSensTempLCD();
    //delay(1000);
  }
}

void openAllVALVE(void) {
  if (_DEBUG_) Serial.println("openAllVALVE");
  for (int i = 0; i < NUMBOILER; i++) {
    doVALVE(i, OPEN_VALVE, false);
    writeSensTempLCD();
    //delay(1000);
  }
}

void doVALVE(int number, int action, bool safeMode) {
  // prevent switching relay too rapidly limited from TimerSwitch
  //if ((unsigned long)(currentMillis - LastSwitch) > TimerSwitch)
  //{
    // action to CLOSE or OPEN VALVE
    digitalWrite(RelayPin[number], action);

    if (action == OPEN_VALVE)
    {
      RelayStatus[number] = true;
      if (safeMode) RelaySafeMode[number] = true;
    }
    else 
    {
      RelayStatus[number] = false;
      if (safeMode) RelaySafeMode[number] = false;
    }
    if (_TXSERIAL_) txData.RelayStatus[number] = RelayStatus[number];
  //  LastSwitch = currentMillis;
  //}
}

// Serial Data
void sendSerialData(void) {
  // send seral data every interval TimerSerialData
  if ((unsigned long)(currentMillis - LastSendData) > TimerSerialData) {
    //send the data id _DEBUG_ is disabled
    if (!_DEBUG_) {
      ET.sendData();
    }
    LastSendData = currentMillis;
  }
}
