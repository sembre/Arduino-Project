#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library

#define CLK 11 // USE THIS ON ARDUINO MEGA
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

void setup()
{
  matrix.begin();
  matrix.setRotation(1); // Set display rotation to 180 degrees (vertical polarization)
  matrix.setTextWrap(false);
}

void loop()
{
  static uint32_t counter = 0;

  matrix.fillScreen(matrix.Color333(0, 0, 0)); // Clear the screen

  // Convert the counter value to a string
  char counterStr[10];
  sprintf(counterStr, "%d", counter);

  // Display the counter value on the matrix
  matrix.setTextSize(2);
  matrix.setTextColor(matrix.Color333(7, 7, 7));
  matrix.setCursor(2, 3);
  matrix.print(counterStr);

  // Increment the counter
  counter++;

  delay(100); // Wait for 1 second
}
