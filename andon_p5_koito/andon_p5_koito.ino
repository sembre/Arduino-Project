#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library
#include <Keypad.h>        // Keypad library

#define CLK 11 // Use this for Arduino Mega
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

// Define keypad layout
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {A12, A13, A14, A15};    // Connect to the row pinouts of the keypad
byte colPins[COLS] = {A8, A9, A10, A11};    // Connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variables to store previous values
static int prevPlan = 0;
static int prevActual = 0;
static int prevBalance = 0;

// Value shift variable
static int shiftValue = 1;

// Inputting plan value status variable
bool inputtingPlan = false;
int newPlan = 0;
int currentNumber = 0;

// Previous variables declaration
int previousNumber = 0;
int previousBalance = 0;

// Define your global variables here
int lastnumpressed = 0;
int timespressed = 0;
char letter = ' ';
String word = ' ';

// Define the maxtimespressed array
const int maxtimespressed[] = {0, 3, 4, 4, 4, 4, 4, 4, 4, 4, 0, 7};

void setup()
{
  matrix.begin();
  matrix.setRotation(0);
  matrix.setTextWrap(false);
  // Display initial elements on the screen
  matrix.fillScreen(matrix.Color333(0, 0, 0));
  matrix.drawRect(0, 0, 17, 25, matrix.Color333(7, 7, 0));
  matrix.drawRect(16, 0, 17, 25, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 17, 25, matrix.Color333(7, 7, 0));
  matrix.drawRect(48, 0, 16, 25, matrix.Color333(7, 7, 0));

  matrix.drawRect(0, 0, 64, 9, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 8, 64, 9, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 9, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 24, 64, 8, matrix.Color333(7, 7, 0));

  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(7, 3, 0));
  matrix.setCursor(5, 1);
  matrix.print("99"); // Plan 1
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(0, 7, 0));
  matrix.setCursor(5, 9);
  matrix.print("99"); // Actual 1
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(7, 0, 0));
  matrix.setCursor(5, 17);
  matrix.print("99"); // Balance 1
}

void definepress() {
  if (lastnumpressed == 1) {
    if (timespressed == 1) {
      letter = 'Q';
    }
    else if (timespressed == 2) {
      letter = 'Z';
    }
    else if (timespressed == 3) {
      letter = '1';
    }
  }
  else if (lastnumpressed == 2) {
    if (timespressed == 1) {
      letter = 'A';
    }
    else if (timespressed == 2) {
      letter = 'B';
    }
    else if (timespressed == 3) {
      letter = 'C';
    }
    else if (timespressed == 4) {
      letter = '2';
    }
  }
  // Add more code here...
}

void incrementtimespressed() {
  if (timespressed == maxtimespressed[lastnumpressed]) {
    timespressed = 1;
  }
  else {
    timespressed++;
  }
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    // Get character from keypad
    char inputChar = key;

    if (inputChar == '#') {
      // If '#' is pressed, reset the displayed text
      letter = ' ';
    }
    else {
      // Add the new character to the existing text
      letter = inputChar;
    }

    // Set the lastnumpressed and timespressed based on inputChar
    if (inputChar >= '0' && inputChar <= '9') {
      lastnumpressed = inputChar - '0';
      incrementtimespressed();
    }
    else if (inputChar == '*') {
      lastnumpressed = 10; // '*' is considered as 10
      incrementtimespressed();
    }
    else if (inputChar == '#') {
      lastnumpressed = 11; // '#' is considered as 11
      incrementtimespressed();
    }

    // Call definepress() function to determine the character to display
    definepress();

    // Clear the previous text at position (row 5, column 25)
    matrix.fillRect(5, 25, 40, 8, matrix.Color333(0, 0, 0));

    // Write the new character at position (row 5, column 25)
    matrix.setTextSize(1);
    matrix.setTextColor(matrix.Color333(7, 7, 7)); // Text color (white)
    matrix.setCursor(5, 25); // Text position on the screen
    matrix.print(letter); // Display the input character

    // Check if the entered character is the last character of the word
    if (inputChar == '#') {
      // Combine the entered characters to form a word
      word += letter;

      // Reset the lastnumpressed and timespressed variables
      lastnumpressed = 0;
      timespressed = 0;

      // Check if the entered word matches a specific word
      if (word == "HELLO") {
        // Do something when the word "HELLO" is entered
        // For example, display a message on the screen
        matrix.fillScreen(matrix.Color333(0, 0, 0));
        matrix.setTextSize(1);
        matrix.setTextColor(matrix.Color333(7, 7, 7)); // Text color (white)
        matrix.setCursor(0, 8); // Text position on the screen
        matrix.print("Hello, World!");

        // Reset the entered word
        word = "";
      }
      else {
        // Reset the entered word if it doesn't match the specific word
        word = "";
      }
    }
  }

  // Continue with the rest of the code...
}
