#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library
#include <Keypad.h>        // Keypad library

#define CLK 11 // USE THIS ON ARDUINO MEGA
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

// Define the keypad layout
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A8, A9, A10, A11};    //connect to the row pinouts of the keypad
byte colPins[COLS] = {A12, A13, A14, A15};    //connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variabel penyimpanan nilai sebelumnya
static int prevPlan = 0;
static int prevActual = 0;
static int prevBalance = 0;

// Variabel valueshift
static int shiftValue = 1;

void setup()
{
  matrix.begin();
  matrix.setRotation(0); 
  matrix.setTextWrap(false);

  // Tampilkan elemen-elemen awal pada layar
  matrix.fillScreen(matrix.Color333(0, 0, 0));
  matrix.drawRect(0, 0, 64, 32, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 9, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 17, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 25, matrix.Color333(7, 7, 0));
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(7, 3, 0));
  matrix.setCursor(1, 1);
  matrix.print("Pln: 0");
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(0, 7, 0));
  matrix.setCursor(1, 9);
  matrix.print("Act: 0");
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(7, 0, 0));
  matrix.setCursor(1, 17);
  matrix.print("Bal: 0");
}

void loop()
{
  matrix.drawRect(0, 0, 64, 32, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 9, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 17, matrix.Color333(7, 7, 0));
  matrix.drawRect(0, 0, 64, 25, matrix.Color333(7, 7, 0));
  static uint32_t counter = 0;
  static int plan = 0;
  static int actual = 0;
  static int balance = 0;
  static bool isSettingPlan = false;
  static char planInput[10];
  static byte planInputIndex = 0;

  // Check for keypress
  char key = keypad.getKey();
  char prevKey = NO_KEY;
  byte prevKeyCol = 40;
// Deklarasikan array untuk menyimpan urutan tombol keypad yang ditekan
// Deklarasikan array untuk menyimpan urutan tombol keypad yang ditekan
char pressedKeys[10];
int numKeysPressed = 0;

if (key != NO_KEY) {
  // Tambahkan tombol keypad yang baru ditekan ke dalam array
  pressedKeys[numKeysPressed] = key;
  numKeysPressed++;

  // Hapus tampilan tombol keypad sebelumnya
  matrix.fillRect(40, 1, 80, 8, matrix.Color333(0, 0, 0));

  // Tampilkan tombol keypad dan hasil key secara berurutan
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(7, 7, 7));
  matrix.setCursor(40, 1);
  for (int i = numKeysPressed - 1; i >= 0; i--) {
    if (pressedKeys[i] < 10) {
      matrix.print("0");
    }
    matrix.print(pressedKeys[i]);
    matrix.print(" ");
  }
}





  if (key)
  {
    // Process the key
    if (key == 'A')
    {
      // Decrement counter by 1
      counter--;
      // Increase actual by 1
      actual += shiftValue;
    }
    else if (key == 'B')
    {
      // Increment counter by 1
      counter++;
      // Increase actual by 1
      actual -= shiftValue;
    }
    else if (key == '*')
    {
      // Reset counter, plan, actual, and balance to 0
      counter = 0;
      plan = 0;
      actual = 0;
      balance = 0;
    }
    else if (key == 'D')
    {
      // Enter plan input mode
      isSettingPlan = true;
      planInputIndex = 0;
      memset(planInput, 0, sizeof(planInput));

      // Display the input mode on the matrix
      matrix.fillScreen(matrix.Color333(0, 0, 0));
      matrix.setTextSize(1);
      matrix.setTextColor(matrix.Color333(7, 7, 7));
      matrix.setCursor(1, 1);
      matrix.print("Pln :");

    }
    else if (key == '#')
    {
      // Exit plan input mode and save the plan value
      isSettingPlan = false;
      plan = atoi(planInput);

      // Display the plan value on the matrix
      matrix.fillScreen(matrix.Color333(0, 0, 0));
      char planStr[10];
      sprintf(planStr, "Pln: %d", plan);
      matrix.setTextSize(1);
      matrix.setTextColor(matrix.Color333(7, 3, 0));
      matrix.setCursor(1, 1);
      matrix.print(planStr);

      // Reset the actual and balance values
      actual = 0;
      balance = 0;
    }

    // Check if in plan input mode
    if (isSettingPlan)
    {
      // Store the plan input
      if (key >= '0' && key <= '9' && planInputIndex < sizeof(planInput) - 1)
      {
        // Convert the pressed key to an integer value
        int digit = key - '0';
        
        // Shift the previous plan input digits to the left (multiply by 10)
        int shiftedValue = atoi(planInput) * 10;
        
        // Add the current digit to the shifted value
        int newInputValue = shiftedValue + digit;
        
        // Store the new plan input value
        sprintf(planInput, "%d", newInputValue);
        planInputIndex++;
      }
    }

    // Calculate balance
    if (actual < plan) {
      balance = -1 * (plan - actual);
    } else {
      balance = actual - plan;
    }
  }

  // Update nilai pada layar hanya jika ada perubahan
  if (prevPlan != plan || prevActual != actual || prevBalance != balance) {
    matrix.fillScreen(matrix.Color333(0, 0, 0));

    // Display the plan value on the matrix
    if (isSettingPlan)
    {
      matrix.setTextSize(1);
      matrix.setTextColor(matrix.Color333(7, 7, 7));
      matrix.setCursor(25, 1);
      matrix.print("Pln: ");

      // Calculate the magnitude of the plan input value
      int magnitude = 0;
      int tempValue = atoi(planInput);
      while (tempValue >= 10) {
        tempValue /= 10;
        magnitude++;
      }

      // Display the plan input value in the appropriate magnitude format
      if (magnitude >= 3 && magnitude < 6) {
        float value = atof(planInput) / 1000;
        matrix.print(value, 1);
        matrix.print("K");
      } else if (magnitude >= 6 && magnitude < 9) {
        float value = atof(planInput) / 1000000;
        matrix.print(value, 1);
        matrix.print("M");
      } else if (magnitude >= 9) {
        float value = atof(planInput) / 1000000000;
        matrix.print(value, 1);
        matrix.print("B");
      } else {
        matrix.print(planInput);
      }
    }
    else
    {
      char planStr[10];
      sprintf(planStr, "Pln: %d", plan);
      matrix.setTextSize(1);
      matrix.setTextColor(matrix.Color333(7, 3, 0));
      matrix.setCursor(1, 1);
      matrix.print(planStr);
    }

    // Convert the actual value to a string
    char actualStr[10];
    sprintf(actualStr, "Act: %d", actual);

    // Display the actual value on the matrix
    matrix.setTextSize(1);
    matrix.setTextColor(matrix.Color333(0, 7, 0));
    matrix.setCursor(1, 9);
    matrix.print(actualStr);

    // Convert the balance value to a string
    char balanceStr[100];
    if (balance >= 1) {
      sprintf(balanceStr, "Bal: +%d", balance);
    } else {
      sprintf(balanceStr, "Bal: %d", balance);
    }

    // Display the balance value on the matrix
    matrix.setTextSize(1);
    matrix.setTextColor(matrix.Color333(7, 0, 0));
    matrix.setCursor(1, 17);
    matrix.print(balanceStr);

    // Simpan nilai sebelumnya
    prevPlan = plan;
    prevActual = actual;
    prevBalance = balance;
  }
}
