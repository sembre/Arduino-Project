#include <SPI.h>
#include <DMD3.h>
#include <DejaVuSans9.h>
#include <DejaVuSansBold9.h>
#include <DejaVuSansItalic9.h>
#include <Mono5x7.h>

DMD3 display;

void scan(){
  display.refresh();
}

void setup() {
  Serial.begin(115200);
  Timer1.initialize(2000);
  Timer1.attachInterrupt(scan);
  Timer1.pwm(9,20);
}

void loop() {
  drawJalan();
  delay(500);
  drawPagi();
  delay(500);
}

static const char message[] = "Besok Libur...!!!";

void drawJalan(){
  int width = display.width();
  display.setFont(DejaVuSans9);
  int msgWidth = display.textWidth(message);
  int fullScroll = msgWidth + width + 1;
  for (int x = 0; x < fullScroll; ++x) {
    display.clear();
    display.drawText(width - x, 3, message);
    delay(50);
  }
}

void drawPagi(){
  char text[14]={"Pagi"};
  display.setFont(DejaVuSans9);
  for (byte x=0;x<sizeof(text);x++){
    for(byte y=0;y<(x*6)+8;y++){
      display.clear();
      for(byte z=0;z<sizeof(text);z++){
        if (z<x){
          display.drawChar((z*6)+4,3,text[z]);
        }
      }

      display.drawChar(y,4,text[x]);
      display.swapBuffers();
      delay(5);
    }
  }
  delay(500);

  for (byte x=sizeof(text);x>0;x--){
    for(byte y=(x*6)+8;y<97;y++){
      display.clear();
      for(byte z=0;z<sizeof(text);z++){
        if (z<x-1){
          display.drawChar((z*6)+4,3,text[z]);
        }
      }

      display.drawChar(y,4,text[x-1]);
      display.swapBuffers();
      delay(5);
    }
  }
  delay(500);
}
