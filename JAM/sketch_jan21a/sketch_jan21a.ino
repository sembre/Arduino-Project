#include <LiquidCrystal.h>
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); //RS,E,D4,D5,D6,D7
//pins
byte aa,a,b,c,d,e,f ;
String (jam);
void setup() {
  lcd.begin(16, 2);
}

void loop() {
  lcd.clear();
  lcd.setCursor (0,0); lcd.print("Proyek Jam");
  jam="Jam : "+String(a)+String(b)+":" +String(c);
  jam=jam +String(d)+":"+String(e)+String(f);
  lcd.setCursor (0,1); lcd.print(jam);
  delay(1000);
  f=f+1;
  if (f==10) {   f=0; e=e+1;}
  if (e==6) {   e=0; d=d+1;}
  if (d==10) {   d=0; c=c+1;}
  if (c==6) {   c=0; b=b+1;}
  if (b==10) {   b=0; a=a+1;}
  aa=(a*10)+b ;  if (aa==24) {   a=0; b=0;}
   
}
