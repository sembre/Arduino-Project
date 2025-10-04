#include <LiquidCrystal.h>
// Sambungan pin pins LCD
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); //RS,E,D4,D5,D6,D7
//pins
int endA[5] = {2,3,4,5,6}; //pins end A
int endB[5] = {A4,A3,A2,A1,A0}; //pins endB
//Hasil
int result[5] = {-1,-1,-1,-1,-1};
int test[5] = {-1,-1,-1,-1,-1};
int counter[5] = {-1,-1,-1,-1,-1};
bool fail =false;
void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(115200); //serial used for debugging only
  lcd.begin(16, 2); 
  //setup pins  
  for(int i=0; i<5; i++){
      pinMode(endA[i], OUTPUT);//set output pins (end A)
      pinMode(endB[i], INPUT_PULLUP);//set input pins (end B)
  }
}
void loop() {
 //run the test
 runTest_5x2();
}
void runTest_5x2(){
  String resultS="";
  //user interface
  delay(500); //debounce
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Check: 5 Circuit");
  lcd.setCursor(0,1);
  for(int i=0; i<5; i++){
     counter[i]=0;
    for(int j=0; j<5; j++){
        digitalWrite(endA[i], LOW); //set all outputs to LOW
    }
    for(int j=0; j<5; j++){  //check for crossed / open circuits vs closed, good, circuits
        digitalWrite(endA[j], HIGH); //cek input satu persatu
        test[i] = digitalRead(endB[i]); //read the output
        if(test[i] == 1 && j!=i){ //crossed or open circuit
          counter[i]++;
          result[i] = 5+j; 
        }
        else if(test[i] == 1 && j==i && result[i] < 5 ){ //Good, closed circuit
          result[i] = 0; 
        }
        digitalWrite(endA[j],LOW);
        //debugging
        /*
          Serial.print("test1 input core  ");
          Serial.print(i);
          Serial.print(" with output core  ");
          Serial.print(j);
          Serial.print(" test =");
          Serial.print(test[i]);
          Serial.print(" counter =");
          Serial.println(counter[i]);*/
    }
    Serial.print("Core ");
    Serial.print(i);
    Serial.print(" result = ");
    if(result[i] == 0){
       Serial.println(" Good");
       resultS+="1";
    }
    else if(counter[i] == 5){        
       Serial.println(" Open");
       resultS+="O";
       fail=true;
    }
    else {
       Serial.println(" Cross");
       resultS+="X";
       fail=true;
    }
  }  
 if(fail){
  digitalWrite(13, LOW);//Pin 13 MATI
   Serial.println("FAILED"); 
   lcd.print(" Terputus/OPEN");  
 }
 else{
    digitalWrite(13, HIGH);//Pin 13 Hidup
   Serial.println("PASSED");
   lcd.print("     PASSED");      
 }
 void(* resetFunc) (void) = 0; //declare reset function @ address 0
delay(500);               // wait for a second
  Serial.println("resetting");
delay(500);
    resetFunc();  //call reset
 } 
