/*! \file HandleTestJig.ino */

/*! \mainpage HandleTestJig Index Page
 * 
 * \section Author and Date
 * 
 * Author: Kunpeng Zhang (kunpeng.zhang@biotexmedical.com)
 * Date: 07/06/2023
 * 
 * 
 * \section Protocol
 * Protocol for BLT and HandleTestJig Communication (BLT <--> Arduino)
 * Baud 115200 (ASCII), The Commands always started with a "B" and end with a "E".
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// #include "version.h"

uint8_t channelNum=0;   ///< The channel (bay) number, when set to 1 to 8, the related bay will be chosen, to actual turn on the bay, the power needs to be turned on by command B20E
uint8_t newReceived=0; ///< Received data from comm port, this only contains one character, it needs to be saved to inputString
uint8_t debugOutput=1; ///< Verbose output control, default debug output is off, use command B255E to turn on/of the debug output
//const char versionNum[6]="1.000";

int upKeyTime=0;
int downKeyTime=0;
int leftKeyTime=0;
int rightKeyTime=0;
int cursorPos=0;
int currentRes=60;
int previousRes=1;

const PROGMEM uint16_t resValue[] = {
  0,//never use
  2,5,7,10,12,15,17,20,23,25,
  28,30,33,35,38,41,43,46,48,51,
  54,56,59,62,64,67,69,72,74,77,
  80,82,85,87,90,93,95,98,100,103,
  105,108,111,113,116,118,121,124,126,129,
  131,134,136,139,142,144,147,149,152,155,
  157,160,162,165,167,170,173,175,178,180,
  183,186,188,191,193,196,199,201,204,206,
  209,211,214,217,219,222,224,227,230,232,
  235,237,240,243,245,248,250,253,255,258,
  261,263,266,268,271,274,276,279,281,284,
  286,289,292,294,297,299,302,305,307,310,
  312,315,317,320,323,325,328,330,333,336,
  338,341,343,346,349,351,354,356,359,361,
  364,367,369,372,374,377,380,382,385,387,
  390,392,395,398,400,403,405,408,411,413,
  416,418,421,424,426,429,431,434,436,439,
  442,444,447,449,452,455,457,460,462,465,
  468,470,473,475,478,480,483,486,488,491
};

/*****************************I2C for INA239 ***************************/
SPISettings INA239_settings(1000000,MSBFIRST,SPI_MODE1); //!< Create a configuration variable for SPI bus. 1000000 is bitrate, MSBFIRST is transition msb first, SPI_MODE1 is the transmission method (defined in Arduino SPI library).

#define SCLK 13
#define SDO  12
#define SDI  11
#define SYNC 10

#define LeftBtn   A0
#define DownBtn   A1
#define UpBtn     A2
#define RightBtn  A3

unsigned long previousTime;
bool updateScreen=true;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3C for 128x64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void testdrawstyles(void) 
{
  display.clearDisplay();

//  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
//  display.println(F("Hello, world!"));
//
//  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
//  display.println(3.141592);

  display.setTextSize(5);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  
  if(currentRes<10)
  {
    display.print("00");
    display.print(currentRes,DEC);
    display.println("K");
  }
  else if(currentRes<100)
  {
    display.print("0");
    display.print(currentRes,DEC);
    display.println("K");
  }
  else if(currentRes>=100)
  {
    display.print(currentRes,DEC);
    display.println("K");
  }
  

  if(cursorPos==0)
  {
    display.print("  ^");
  }
  else if(cursorPos==1)
  {
    display.print(" ^ ");
  }
  else if(cursorPos==2)
  {
    display.print("^  ");
  }

  display.display();
}

/**
  Initial all the Pins and Variables, Setup SPI and UART, the SPI is set to 1MHz, UART is set to 115200 bps
  @param void
  @return void
*/
void setup() 
{

    digitalWrite(SYNC, HIGH);
    pinMode(SYNC, OUTPUT);

    pinMode(SCLK, OUTPUT);
    pinMode(SDO, OUTPUT);

    pinMode(SDI, INPUT_PULLUP);


    pinMode(LeftBtn, INPUT_PULLUP);
    pinMode(DownBtn, INPUT_PULLUP);
    pinMode(UpBtn, INPUT_PULLUP);
    pinMode(RightBtn, INPUT_PULLUP);

//****************************Communication initial**********************/
    SPI.begin();
    
    Serial.begin(115200);
    Serial.setTimeout(100); ///<long delay allows testing from terminal windows. Follow cmd with 'E' or other non digit to end cmd.

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
    {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
    // testdrawstyles();
//************************Initial global variables*******************************/
   previousTime = millis();
}

String inputString = "";      ///< a String to hold incoming data
bool stringComplete = false;  ///< Current string is complete

/*
Infinite loop for the main process steps
*/
void loop() 
{
  // put your main code here, to run repeatedly:

  if(millis() - previousTime> 1)//10ms
  {
      previousTime = millis();

      if( digitalRead(LeftBtn)==LOW )//keydown
      {
          leftKeyTime ++;
          if(leftKeyTime==100)//300ms
          { 
            leftKeyTime=0;
            if(cursorPos<2)
            {
              cursorPos++;
              updateScreen=true;
            }  
            
          }
      }
      else
      {
        leftKeyTime=0;        
      }

      if( digitalRead(RightBtn)==LOW )//keydown
      {
          rightKeyTime ++;
          if(rightKeyTime==100)//300ms
          { 
            rightKeyTime=0;
            if(cursorPos>0)
            {
              cursorPos--;
              updateScreen=true;
            }  
            
          }
      }
      else
      {
        rightKeyTime=0;
      }

      if( digitalRead(UpBtn) == LOW )//keydown
      {
        upKeyTime++;
        if(upKeyTime==100)//300ms
        { 
          upKeyTime=0;
          int8_t digit[3];
          previousRes = currentRes;
          digit[0] = currentRes%10;
          digit[1] = (currentRes/10) %10;
          digit[2] = currentRes/100;
          digit[cursorPos]++;
          if(digit[cursorPos]>9)
            digit[cursorPos]=0;
            
          currentRes = digit[2]*100+digit[1]*10+digit[0];
          if( currentRes<=190 && currentRes > 0)
          {
            updateScreen = true;
          }
          else
          {
            currentRes = previousRes;
          }
        }
      }
      else
      {
        upKeyTime=0;
      }

      if( digitalRead(DownBtn) == LOW )//keydown
      {
        downKeyTime++;
        if(downKeyTime==100)//300ms
        { 
          downKeyTime=0;
          int8_t digit[3];
          previousRes = currentRes;
          digit[0] = currentRes%10;
          digit[1] = (currentRes/10) %10;
          digit[2] = currentRes/100;
          digit[cursorPos]--;
          if(digit[cursorPos]<0)
            digit[cursorPos]=9;
            
          currentRes = digit[2]*100+digit[1]*10+digit[0];
          if( currentRes<=190 && currentRes > 0)
          {
            updateScreen = true;
          }
          else
          {
            currentRes = previousRes;
          }
        }
      }
      else
      {
        downKeyTime=0;
      }
  }
  int DataReceived;

  if (stringComplete) //update display with UART received data
  {
      DataReceived=inputString.toInt();
      if(DataReceived<=190 && DataReceived > 0)
      {
        previousRes = currentRes;
        currentRes = DataReceived;
        updateScreen=true;
      }

      if(debugOutput)
          Serial.println(DataReceived);

      inputString = "";
      stringComplete = false;
  }

  if(updateScreen==true)
  {
    updateScreen=false;

    uint16_t displayRes=pgm_read_word_near(resValue+currentRes);
    // int displayRes=50;
    if(displayRes<256)
    {
      digitalWrite(SYNC, LOW);
      SPI.beginTransaction(INA239_settings);
      SPI.transfer(0x10);  //register to write
      SPI.transfer(255-displayRes);
      SPI.endTransaction();
      digitalWrite(SYNC, HIGH);

      delay(1);

      digitalWrite(SYNC, LOW);
      SPI.beginTransaction(INA239_settings);
      SPI.transfer(0x11);  //register to write
      SPI.transfer(0xFF);
      SPI.endTransaction();

      digitalWrite(SYNC, HIGH);
    }

    else if(displayRes>=256&&displayRes<511)
    {
      digitalWrite(SYNC, LOW);
      SPI.beginTransaction(INA239_settings);
      SPI.transfer(0x10);  //register to write
      SPI.transfer(0x00);
      SPI.endTransaction();
      digitalWrite(SYNC, HIGH);

      delay(1);

      digitalWrite(SYNC, LOW);
      SPI.beginTransaction(INA239_settings);
      SPI.transfer(0x11);  //register to write
      SPI.transfer(510-displayRes);
      SPI.endTransaction();
      digitalWrite(SYNC, HIGH);
    }

    testdrawstyles();
  }

}




/*
SerialEvent occurs whenever a new data comes in the hardware serial RX
*/
void serialEvent() 
{
    while (Serial.available()) 
    {
        // get the new byte:
        char inChar = (char)Serial.read();
        // add it to the inputString:
        if(inChar=='B'||inChar=='b')
        {
            inputString="";
        }
        else if(inChar=='E'||inChar=='e')
        {
            stringComplete = true;
        }
        else if(inChar>='0'&&inChar<='9')
        {
            inputString += inChar;
        }
    }
}

