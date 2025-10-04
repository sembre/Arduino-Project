
#include <U8g2lib.h>




//#define DEBUG        // Verbose serial output. 
#define  A_PHASE 2
#define  B_PHASE 3
long  flag_A = 0;  //Assign a value to the token bit
 

// OLED definitions
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display

#define FIRST_ROW_Y 16
#define FIRST_ROW_X 16
#define BOX_H 38

#define hysteresis 5 //hysteresis 5 readings, sensitivity 2mm ((wheelDia*PI)/400)*hysteresis

const int PinSW = 8;     // Used for the push button switch
const int UomSW = 9;     //Unit of measure conversion switch
bool bImp = 0;
const float wheelDia = 51.0;  //wheel diameter in mm
const float singleTick = (wheelDia*PI)/400; //one tick lenght in mm
unsigned long keytime = 0;
short count = 0;

void setup()
{
  
  pinMode(A_PHASE, INPUT_PULLUP);
  pinMode(B_PHASE, INPUT_PULLUP);
  u8g2.begin();
   #ifdef DEBUG
    Serial.begin(9600);
    Serial.println(F("Measurino is ready."));
   #endif
  pinMode(PinSW, INPUT_PULLUP);
  pinMode(UomSW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt( A_PHASE), interrupt, RISING); //Interrupt trigger mode: RISING
  } // setup()


void interrupt()// Interrupt function
{ 
  char i;
  i = digitalRead( B_PHASE);
  if (i == 1)
    count +=1;
  else
    count -=1;

  if (abs(count) >= hysteresis)  
  {
    flag_A = flag_A+count;
    count = 0;
  }  
}


// Read the current position of the encoder and print out when changed.
void loop()
{

 if (millis() - 100 > keytime) //Detect key press once every 100ms
  {
  if (!(digitalRead(PinSW))) {        // check if pushbutton is pressed
            flag_A = 0;            // if YES, then reset counter to ZERO
            while (!digitalRead(PinSW)) {}  // wait til switch is released
            delay(10);                      // debounce
            #ifdef DEBUG
              Serial.println(F("Reset"));        // Using the word RESET instead of COUNT here to find out a buggy encoder
            #endif
      }
      keytime = millis();

  if (!(digitalRead(UomSW))) {        // check if pushbutton is pressed   
          bImp = !bImp;
          while (!digitalRead(UomSW)) {};
          delay(10);
      }
  }


    u8g2.firstPage();
    do {
      u8g2.drawFrame(0,FIRST_ROW_Y+1,128,BOX_H);
      if (!bImp)
        displayUnitsDec(flag_A);
      else
        displayUnitsImp(flag_A);
      u8g2.setFont(u8g2_font_t0_14_tf);
      u8g2.setCursor(5,9); u8g2.print(F("UKUR PANJANG WIRE"));
      u8g2.setFont(u8g2_font_u8glib_4_tf);
      u8g2.setCursor(65,64); u8g2.print(F("AGUS FITRIYANTO"));
    } while ( u8g2.nextPage() ); 
    
} // loop ()


void displayUnitsDec(long ticks) 
{
  u8g2.setCursor(10,40);
  u8g2.setFont(u8g2_font_logisoso18_tf);
  if (abs(ticks) >= 1000000/singleTick) {
    u8g2.print(float((float(ticks)/1000000)*singleTick),3);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(98,40);
    u8g2.print(F(" km"));    
    //switch to kilometers
     #ifdef DEBUG
      Serial.print(float((float(ticks)/1000000)*singleTick),3);
      Serial.println(F(" km."));
     #endif
  }
  if (abs(ticks) >= 1000/singleTick) {
    //switch to meters
    u8g2.print(float((float(ticks)/1000)*singleTick),3);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(108,40);
    u8g2.print(F(" m"));    
     #ifdef DEBUG
      Serial.print(float((float(ticks)/1000)*singleTick),3);
      Serial.println(F(" m."));
     #endif
  } else {
    u8g2.print(float(ticks*singleTick),1);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(98,40);
    u8g2.print(F(" mm"));
    #ifdef DEBUG
      Serial.print(ticks*singleTick);
      Serial.println(F(" mm."));
    #endif
  }
}

void displayUnitsImp(long ticks) 
{
  u8g2.setCursor(10,40);
  u8g2.setFont(u8g2_font_logisoso18_tf);
  if (abs(ticks) >= 1609344/singleTick) {
    u8g2.print(float((float(ticks)/1609344)*singleTick),2);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(98,40);
    u8g2.print(F(" mi"));    
    //switch to miles
     #ifdef DEBUG
      Serial.print(float((float(ticks)/1609344)*singleTick),3);
      Serial.println(F(" mi."));
     #endif
  }
  if (abs(ticks) >= 914.4/singleTick) {
    //switch to yards
    u8g2.print(float((float(ticks)/914.4)*singleTick),2);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(98,40);
    u8g2.print(F(" yd"));    
     #ifdef DEBUG
      Serial.print(float((float(ticks)/914.4)*singleTick),2);
      Serial.println(F(" yd."));
     #endif
  } else {
    u8g2.print(float(ticks*singleTick/25.4),2);
    u8g2.setFont(u8g2_font_t0_16_tf);
    u8g2.setCursor(98,40);
    u8g2.print(F(" in"));
    #ifdef DEBUG
      Serial.print(ticks*singleTick/25.4);
      Serial.println(F(" in."));
    #endif
  }
}
// The End
