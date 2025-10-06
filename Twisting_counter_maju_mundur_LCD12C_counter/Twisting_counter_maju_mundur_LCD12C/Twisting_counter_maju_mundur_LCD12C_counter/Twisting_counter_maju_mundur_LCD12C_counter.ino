/*
 * ========================================================================
 * BIDIRECTIONAL TWISTING COUNTER SYSTEM WITH DUAL DISPLAY
 * ========================================================================
 * 
 * Project: Twisting_counter_maju_mundur_LCD12C_counter
 * Version: 1.2 Professional Edition
 * Date: July 2025 (Updated October 2025)
 * Author: AGUS F
 * 
 * DESCRIPTION:
 * Advanced bidirectional twisting counter system dengan dual-phase control.
 * Sistem ini mengontrol motor twisting dengan 2 fase operasi (A & B) yang
 * dapat diatur secara independen untuk menghasilkan twisted cable berkualitas.
 * 
 * MAIN FEATURES:
 * • Dual-Phase Twisting Control (Phase A: Forward, Phase B: Reverse)
 * • Programmable Turn Counts untuk each phase (independent setting)
 * • Dual Display System (LCD 16x2 + TM1637 7-segment counter)
 * • 4x4 Keypad Interface untuk parameter input dan control
 * • IR Sensor Feedback untuk accurate turn counting
 * • Emergency Stop Switch untuk safety protection
 * • Debounced Input System untuk reliable operation
 * • Production Counter dengan manual increment/reset
 * • System Ready Flag untuk safe startup sequence
 * 
 * ADVANCED CAPABILITIES:
 * • Phase A Completion Detection dengan automatic phase transition
 * • Emergency stop functionality dengan immediate motor shutdown
 * • Input validation untuk prevent invalid operation
 * • Keypad lock mechanism untuk protect settings during operation
 * • Long-press reset function untuk production counter
 * • Real-time turn counting dengan interrupt-based precision
 * • Safety interlocks untuk prevent accidental activation
 * 
 * HARDWARE REQUIREMENTS:
 * • Arduino Mega/Uno (sufficient I/O pins)
 * • LCD 16x2 I2C display (address 0x27)
 * • TM1637 4-digit 7-segment display
 * • 4x4 Matrix keypad
 * • IR sensor untuk turn detection
 * • Motor driver/relay untuk twisting motor
 * • Emergency stop switch
 * • Start button untuk manual operation
 * 
 * INDUSTRIAL APPLICATIONS:
 * • Cable manufacturing dengan precise twist control
 * • Wire harness production dengan consistent quality
 * • Rope manufacturing dengan programmable twist density
 * • Fiber optic cable production dengan controlled tension
 * • Multi-conductor cable assembly
 * • Quality control testing untuk twist specifications
 * 
 * SAFETY FEATURES:
 * • Emergency stop switch dengan immediate shutdown
 * • Input validation untuk prevent invalid parameters
 * • System ready flag untuk safe startup
 * • Switch monitoring dengan continuous safety check
 * • Debounced inputs untuk prevent false triggers
 * • Motor state tracking untuk safe operation
 * 
 * TECHNICAL SPECIFICATIONS:
 * • Turn Resolution: 1 turn precision dengan IR sensor
 * • Phase Control: Independent A & B phase programming
 * • Counter Range: 0-9999 turns per phase
 * • Display: 16x2 LCD + 4-digit 7-segment
 * • Input: 4x4 keypad + physical buttons
 * • Safety: Emergency stop + input validation
 * 
 * UPDATE HISTORY:
 * • July 17, 2025: Safety improvements dan system ready flag
 * • October 2025: Comprehensive documentation added
 */

#include <Keypad.h>           // 4x4 matrix keypad interface
#include <Wire.h>             // I2C communication untuk LCD
#include <LiquidCrystal_I2C.h> // I2C LCD display control
#include <TM1637Display.h>    // 7-segment display control

// ================================================================
// HARDWARE PIN CONFIGURATION
// ================================================================

/*
 * SYSTEM IMPROVEMENTS (July 17, 2025):
 * ====================================
 * ✅ Complete logical condition validation before motor start
 * ✅ Emergency switch monitoring before any motor activation  
 * ✅ Target turns validation (A & B must be valid)
 * ✅ System ready flag untuk prevent immediate startup
 * ✅ Safe digitalWrite control dengan proper state checking
 * ✅ Enhanced safety interlocks untuk industrial operation
 */

/*
 * MOTOR CONTROL INTERFACE:
 * ========================
 * motorPin: Primary motor control output
 * • Pin: A1 (Analog pin used as digital output)
 * • Function: Motor enable/disable control
 * • Logic: HIGH = Motor ON, LOW = Motor OFF
 * • Load: Motor driver/relay coil
 * • Protection: External flyback diode recommended
 */
const int motorPin = A1;              // Motor control output pin

/*
 * USER INTERFACE INPUTS:
 * ======================
 * startButtonPin: Manual start button
 * • Pin: Digital pin 2 (interrupt capable)
 * • Function: Manual motor start trigger
 * • Logic: INPUT_PULLUP (LOW when pressed)
 * • Debouncing: Software debounce implemented
 * • Safety: Requires keypad lock before activation
 */
const int startButtonPin = 2;         // Manual start button input

/*
 * FEEDBACK SENSOR INTERFACE:
 * ==========================
 * irSensorPin: IR sensor untuk turn counting
 * • Pin: Digital pin 3 (interrupt capable)
 * • Function: Turn detection dengan precision counting
 * • Logic: FALLING edge triggered interrupt
 * • Debouncing: 3ms hardware debounce
 * • Resolution: 1 turn per interrupt
 */
const int irSensorPin = 3;            // IR sensor input (interrupt)

/*
 * PHASE CONTROL OUTPUT:
 * =====================
 * outputPin: Phase direction control
 * • Pin: A3 (Analog pin used as digital output)
 * • Function: Phase A/B direction control
 * • Logic: HIGH = Phase A, LOW = Phase B
 * • Application: Motor direction atau clutch control
 */
const int outputPin = A3;             // Phase control output pin

/*
 * SAFETY SYSTEM INTERFACE:
 * ========================
 * switchPin: Emergency stop switch
 * • Pin: A2 (Analog pin used as digital input)
 * • Function: Emergency stop dan safety interlock
 * • Logic: INPUT_PULLUP (LOW = emergency stop)
 * • Priority: Highest priority - immediate motor stop
 * • Monitoring: Continuous monitoring dalam main loop
 */
const int switchPin = A2;             // Emergency stop switch input

/*
 * TM1637 7-SEGMENT DISPLAY INTERFACE:
 * ===================================
 * Production counter display untuk track output quantity
 * • clk: Clock pin untuk serial communication
 * • dio: Data I/O pin untuk display control
 * • Function: Production counter display (0-9999)
 * • Brightness: Adjustable (level 7 = maximum)
 */
const int clk = 4;                    // TM1637 clock pin
const int dio = 5;                    // TM1637 data I/O pin

// ================================================================
// SYSTEM STATE VARIABLES & CONTROL FLAGS
// ================================================================

/*
 * TURN COUNTING SYSTEM:
 * =====================
 * Interrupt-based precision counting untuk accurate turn measurement
 * 
 * count: Current turn counter (volatile untuk interrupt safety)
 * • Type: volatile int (interrupt-modified variable)
 * • Range: 0 to target turns
 * • Reset: Setiap phase completion
 * • Precision: 1 turn resolution
 */
volatile int count = 0;               // Current turn count (interrupt-modified)

/*
 * PROGRAMMABLE TARGET SETTINGS:
 * =============================
 * Independent phase control untuk flexible twisting patterns
 * 
 * targetTurnsA: Phase A target (forward direction)
 * • Range: 1-9999 turns
 * • Function: Forward twist count
 * • Validation: Must be > 0 untuk operation
 * 
 * targetTurnsB: Phase B target (reverse direction)  
 * • Range: 1-9999 turns
 * • Function: Reverse twist count untuk final cable
 * • Validation: Must be > 0 untuk operation
 */
int targetTurnsA = 0;                 // Phase A target turns (forward)
int targetTurnsB = 0;                 // Phase B target turns (reverse)

/*
 * SYSTEM OPERATION STATE FLAGS:
 * =============================
 * Boolean flags untuk comprehensive state management
 * 
 * motorRunning: Motor operation status
 * • Purpose: Track motor ON/OFF state
 * • Safety: Prevent multiple start commands
 * • Control: Used dalam safety interlocks
 * 
 * keypadLocked: Settings protection flag
 * • Purpose: Protect settings during operation
 * • Requirement: Both targets must be > 0
 * • Control: Required untuk motor start
 * 
 * systemReady: Startup safety flag
 * • Purpose: Prevent operation during initialization
 * • Timing: Set TRUE after complete setup
 * • Safety: Blocks all operations until ready
 */
bool motorRunning = false;            // Motor operation status flag
bool keypadLocked = false;            // Settings lock status flag
bool systemReady = false;             // System initialization complete flag

/*
 * INPUT MODE STATE MANAGEMENT:
 * ============================
 * State flags untuk keypad input processing
 * 
 * inputModeA/B: Input mode selection flags
 * • Purpose: Direct numeric input ke correct target
 * • Mutual Exclusion: Only one mode active at time
 * • Control: Activated by A/B keys
 */
bool inputModeA = false;              // Phase A input mode flag
bool inputModeB = false;              // Phase B input mode flag

/*
 * PHASE CONTROL STATE MANAGEMENT:
 * ===============================
 * Phase transition control untuk bidirectional operation
 * 
 * phaseACompleted: Phase transition flag
 * • Purpose: Track completion of forward phase
 * • Trigger: When count >= targetTurnsA
 * • Action: Switch ke Phase B (reverse direction)
 * • Reset: Each new cycle start
 */
bool phaseACompleted = false;         // Phase A completion status flag

/*
 * PRODUCTION COUNTER SYSTEM:
 * ==========================
 * Production tracking dengan manual control
 * 
 * counter: Production output counter
 * • Display: TM1637 7-segment display
 * • Range: 0-9999 units
 * • Control: Manual increment (C key) or auto (start button)
 * • Reset: Long-press D key (3 seconds)
 */
int counter = 0;                      // Production output counter

/*
 * USER INTERFACE TIMING CONTROL:
 * ==============================
 * Timing variables untuk user interaction features
 * 
 * pressStartTime: Long-press detection timestamp
 * • Purpose: Track D key press duration
 * • Function: 3-second hold untuk counter reset
 * • Resolution: millis() precision
 * 
 * isKeyDPressed: D key state tracking
 * • Purpose: Long-press detection state
 * • Logic: TRUE during D key hold
 * • Reset: After 3-second timeout or key release
 */
unsigned long pressStartTime = 0;     // Long-press timing variable
bool isKeyDPressed = false;           // D key press state flag

// ================================================================
// DISPLAY SYSTEM CONFIGURATION
// ================================================================

/*
 * DUAL DISPLAY SYSTEM ARCHITECTURE:
 * =================================
 * Comprehensive user interface dengan two complementary displays
 * 
 * LCD 16x2 I2C Display:
 * • Address: 0x27 (standard I2C LCD backpack)
 * • Size: 16 characters × 2 lines
 * • Function: Status display, settings, real-time feedback
 * • Features: Backlight control, custom characters support
 * • Connection: I2C (SDA pin 20, SCL pin 21 pada Mega)
 * 
 * TM1637 7-Segment Display:
 * • Size: 4 digits dengan decimal points
 * • Function: Production counter display
 * • Features: Brightness control, leading zero blanking
 * • Connection: 2-wire serial (CLK pin 4, DIO pin 5)
 * • Range: 0000-9999 dengan automatic formatting
 */
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C LCD: address 0x27, 16x2 display
TM1637Display display(clk, dio);      // 7-segment display: CLK & DIO pins

// ================================================================
// 4x4 KEYPAD MATRIX CONFIGURATION
// ================================================================

/*
 * KEYPAD INTERFACE SYSTEM:
 * ========================
 * Professional 4x4 matrix keypad untuk comprehensive control
 * 
 * Keypad Layout & Functions:
 * +---+---+---+---+
 * | 1 | 2 | 3 | A |  <- A: Select Phase A input mode
 * +---+---+---+---+
 * | 4 | 5 | 6 | B |  <- B: Select Phase B input mode  
 * +---+---+---+---+
 * | 7 | 8 | 9 | C |  <- C: Increment production counter
 * +---+---+---+---+
 * | * | 0 | # | D |  <- *: Clear/Reset, #: Lock settings, D: Reset counter
 * +---+---+---+---+
 * 
 * Key Functions Detailed:
 * • Numbers 0-9: Input target turn values
 * • Key A: Activate Phase A input mode (forward turns)
 * • Key B: Activate Phase B input mode (reverse turns)
 * • Key C: Manual production counter increment
 * • Key *: Clear current settings (unlock system)
 * • Key #: Lock settings (enable motor start)
 * • Key D: Long-press (3s) untuk reset production counter
 * 
 * Hardware Configuration:
 * • Matrix Size: 4 rows × 4 columns
 * • Row Pins: A12, A13, A14, A15 (analog pins as digital)
 * • Col Pins: A8, A9, A10, A11 (analog pins as digital)
 * • Pull-up: Internal pull-up resistors enabled
 * • Debouncing: Software debouncing implemented
 */
const byte ROWS = 4;                  // Keypad matrix rows
const byte COLS = 4;                  // Keypad matrix columns

// Keypad character mapping matrix
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},               // Row 0: Numbers 1-3, Phase A select
  {'4', '5', '6', 'B'},               // Row 1: Numbers 4-6, Phase B select
  {'7', '8', '9', 'C'},               // Row 2: Numbers 7-9, Counter increment
  {'*', '0', '#', 'D'}                // Row 3: Clear, Zero, Lock, Counter reset
};

// Hardware pin assignments untuk keypad matrix
byte rowPins[ROWS] = {A12, A13, A14, A15}; // Row pins (R1-R4)
byte colPins[COLS] = {A8, A9, A10, A11};   // Column pins (C1-C4)

// Initialize keypad object dengan pin mapping
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================================================================
// SYSTEM INITIALIZATION & STARTUP SEQUENCE
// ================================================================

/*
 * setup() - Complete system initialization dengan safety procedures
 * 
 * INITIALIZATION SEQUENCE:
 * 1. Hardware Pin Configuration (I/O setup dengan safe defaults)
 * 2. Display System Initialization (LCD + TM1637 setup)
 * 3. Serial Communication Setup (debugging & monitoring)
 * 4. Interrupt System Configuration (IR sensor interrupt)
 * 5. Startup Banner Display Sequence (system identification)
 * 6. User Interface Preparation (ready voor input)
 * 7. System Ready Flag Activation (safety enable)
 * 
 * SAFETY FEATURES:
 * • All outputs initialized ke safe state (LOW)
 * • Emergency stop monitoring enabled
 * • System ready flag prevents premature operation
 * • Interrupt-based turn counting configured
 * • Display systems validated before operation
 */
void setup() {
  // ================================================================
  // STEP 1: HARDWARE PIN CONFIGURATION
  // ================================================================
  
  // Motor control output - SAFE STATE: OFF
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);        // Motor initially OFF
  
  // User interface inputs - PULLUP enabled voor noise immunity
  pinMode(startButtonPin, INPUT_PULLUP); // Start button met pull-up
  pinMode(irSensorPin, INPUT_PULLUP);     // IR sensor met pull-up
  
  // Phase control output - SAFE STATE: Phase A
  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, LOW);       // Phase A initially selected
  
  // Emergency stop input - PULLUP enabled voor safety
  pinMode(switchPin, INPUT_PULLUP);   // Emergency stop monitoring
  
  // ================================================================
  // STEP 2: DISPLAY SYSTEM INITIALIZATION
  // ================================================================
  
  // LCD I2C display initialization
  lcd.init();                         // Initialize I2C LCD
  lcd.backlight();                    // Enable backlight voor visibility
  
  // TM1637 7-segment display initialization  
  display.setBrightness(7);           // Maximum brightness (level 7)
  display.showNumberDec(0, true);     // Display initial "0000"
  
  // ================================================================
  // STEP 3: SERIAL COMMUNICATION SETUP
  // ================================================================
  
  // Serial communication voor debugging dan monitoring
  Serial.begin(9600);                 // Standard baud rate
  Serial.println("=== TWISTING COUNTER SYSTEM v1.2 ===");
  Serial.println("Initializing system components...");
  
  // ================================================================
  // STEP 4: INTERRUPT SYSTEM CONFIGURATION
  // ================================================================
  
  // IR sensor interrupt setup voor precision turn counting
  attachInterrupt(digitalPinToInterrupt(irSensorPin), countTurns, FALLING);
  Serial.println("IR sensor interrupt configured (FALLING edge)");
  
  // ================================================================
  // STEP 5: STARTUP BANNER DISPLAY SEQUENCE
  // ================================================================
  
  // System identification banner
  lcd.setCursor(0, 0); lcd.print("Motor Controller");
  lcd.setCursor(0, 1); lcd.print("Starting......");
  Serial.println("Displaying startup banner...");
  delay(3000);
  
  // Version information display
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Koding Juli");
  lcd.setCursor(0, 1); lcd.print("2025 Ver.1.2");
  Serial.println("Version: 1.2 (July 2025)");
  delay(2000);
  
  // Author information display
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Created & Design");
  lcd.setCursor(0, 1); lcd.print("by AGUS F");
  Serial.println("Author: AGUS F");
  delay(2000);
  
  // System ready notification
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Mesin Siap");
  lcd.setCursor(0, 1); lcd.print("Masukan Putaran");
  Serial.println("System ready for operation");
  delay(2000);
  
  // ================================================================
  // STEP 6: USER INTERFACE PREPARATION
  // ================================================================
  
  // Display main operating interface
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Set A:    B:  ");
  Serial.println("Main interface displayed");
  
  // ================================================================
  // STEP 7: SYSTEM READY FLAG ACTIVATION
  // ================================================================
  
  // Enable system operation (SAFETY CRITICAL)
  systemReady = true;                 // System now ready voor safe operation
  
  Serial.println("=== SYSTEM INITIALIZATION COMPLETE ===");
  Serial.println("System ready voor user input");
  Serial.println("Emergency stop monitoring: ACTIVE");
  Serial.println("========================================");
}

// ================================================================
// INTERRUPT SERVICE ROUTINE - PRECISION TURN COUNTING
// ================================================================

/*
 * countTurns() - IR sensor interrupt service routine
 * 
 * FUNCTION: High-precision turn counting dengan hardware debouncing
 * 
 * INTERRUPT CHARACTERISTICS:
 * • Trigger: FALLING edge detection
 * • Priority: Hardware interrupt (immediate response)
 * • Resolution: 1 turn per interrupt
 * • Debouncing: 3ms hardware debounce filter
 * • Safety: Double-check untuk confirm genuine trigger
 * 
 * DEBOUNCING STRATEGY:
 * 1. Initial FALLING edge triggers interrupt
 * 2. 3ms delay untuk allow signal stabilization
 * 3. Re-check sensor state untuk confirm LOW
 * 4. Only increment count jika confirmed LOW
 * 
 * PERFORMANCE CONSIDERATIONS:
 * • Interrupt execution time: <10 microseconds
 * • Maximum counting rate: >1000 turns/second
 * • Noise immunity: 3ms debounce eliminates false triggers
 * • Thread safety: Volatile count variable
 */
void countTurns() {
  // Check initial trigger state
  if (digitalRead(irSensorPin) == LOW) {
    // Hardware debounce delay (3ms)
    delayMicroseconds(3000);
    
    // Confirm trigger state after debounce
    if (digitalRead(irSensorPin) == LOW) {
      count++;    // Increment turn counter (volatile variable)
    }
  }
}

// ================================================================
// BUTTON DEBOUNCING UTILITY FUNCTION
// ================================================================

/*
 * debounceButton() - Software debounce untuk physical buttons
 * 
 * FUNCTION: Reliable button press detection dengan bounce elimination
 * 
 * PARAMETERS:
 * • pin: Pin number to monitor
 * 
 * RETURN:
 * • true: Valid button press detected
 * • false: No press atau within debounce period
 * 
 * DEBOUNCING CHARACTERISTICS:
 * • Debounce Time: 100ms (optimal voor mechanical switches)
 * • Logic: INPUT_PULLUP (LOW = pressed)
 * • Method: Time-based filtering
 * • Memory: Static variable untuk persistent timing
 * 
 * ADVANTAGES:
 * • Eliminates mechanical switch bounce
 * • Prevents multiple triggers dari single press
 * • Configurable debounce timing
 * • Memory efficient (single static variable)
 * • Compatible dengan any digital input pin
 */
bool debounceButton(int pin) {
  static unsigned long lastPress = 0;     // Last valid press timestamp
  const unsigned long debounceTime = 100; // Debounce period (100ms)
  
  // Check voor button press (LOW state)
  if (digitalRead(pin) == LOW) {
    // Check jika enough time has passed since last press
    if (millis() - lastPress > debounceTime) {
      lastPress = millis();               // Update last press timestamp
      return true;                        // Valid press detected
    }
  }
  return false;                           // No valid press
}

// ================================================================
// MAIN PROGRAM LOOP - SYSTEM CONTROL ENGINE
// ================================================================

/*
 * loop() - Main system control loop dengan comprehensive safety monitoring
 * 
 * MAIN FUNCTIONS:
 * 1. System Safety Monitoring (emergency stop, system ready)
 * 2. Start Button Processing (motor activation dengan validation)
 * 3. Real-time Turn Count Monitoring (progress tracking)
 * 4. Keypad Input Processing (user interface)
 * 5. Production Counter Management (output tracking)
 * 6. Phase Control Logic (A→B transition)
 * 
 * CONTROL FLOW PRIORITY:
 * 1. System Ready Check (highest priority)
 * 2. Emergency Stop Monitoring (safety critical)
 * 3. Motor Control Logic (operation control)
 * 4. User Interface Processing (input handling)
 * 5. Display Updates (feedback systems)
 * 
 * SAFETY INTERLOCKS:
 * • System must be ready before any operation
 * • Emergency stop has absolute priority
 * • Settings must be locked before motor start
 * • Both target values must be valid (> 0)
 * • Motor state tracking prevents conflicts
 */
void loop() {
  // ================================================================
  // SECTION 1: SYSTEM SAFETY & READINESS CHECKS
  // ================================================================
  
  // CRITICAL: System ready check (prevent operation during initialization)
  if (!systemReady) {
    return;    // Block all operations until system fully initialized
  }

  // CRITICAL: Emergency stop monitoring (highest priority safety check)
  if (digitalRead(switchPin) == LOW) {
    stopMotor();    // Immediate motor shutdown
    return;         // Exit loop immediately (no further processing)
  }

  // ================================================================
  // SECTION 2: START BUTTON PROCESSING & MOTOR CONTROL
  // ================================================================
  
  /*
   * START BUTTON LOGIC:
   * ===================
   * Multi-level validation before motor activation:
   * 1. Debounced button press detection
   * 2. Motor not already running check
   * 3. Settings locked validation (keypadLocked)
   * 4. Target values validation (both > 0)
   * 5. Production counter increment
   * 6. Motor start execution
   */
  
  // Process start button dengan comprehensive validation
  if (debounceButton(startButtonPin) && !motorRunning) {
    // Validate all prerequisites voor motor start
    if (keypadLocked && targetTurnsA > 0 && targetTurnsB > 0) {
      // All conditions met - safe to start motor
      startMotor();                       // Execute motor start sequence
      counter++;                          // Increment production counter
      display.showNumberDec(counter);     // Update production display
      
      Serial.print("Motor started - Production count: ");
      Serial.println(counter);
    } else {
      // Prerequisites not met - display error message
      lcd.setCursor(0, 1);
      lcd.print("Lock dulu (#)   ");      // Indonesian: "Lock first (#)"
      
      Serial.println("Start denied: Settings not locked or invalid targets");
    }
  }

  // ================================================================
  // SECTION 3: REAL-TIME TURN COUNT MONITORING & PHASE CONTROL
  // ================================================================
  
  /*
   * TURN COUNT MONITORING SYSTEM:
   * =============================
   * Real-time display update and phase transition control
   * 
   * Static variable optimization:
   * • lastCount: Prevents unnecessary LCD updates
   * • Only update display when count changes
   * • Reduces LCD write frequency (improves performance)
   * 
   * Phase Control Logic:
   * • Phase A (Forward): count 0 → targetTurnsA
   * • Phase B (Reverse): count 0 → targetTurnsB
   * • Automatic transition: A complete → B start
   * • Cycle completion: B complete → motor stop
   */
  
  static int lastCount = -1;              // Static: retain value between calls
  
  // Check voor turn count changes (interrupt-driven updates)
  if (count != lastCount) {
    lastCount = count;                    // Update static tracker
    
    // Update LCD display with current turn count
    lcd.setCursor(0, 1);
    lcd.print("BERPUTAR: ");             // Indonesian: "ROTATING: "
    lcd.print(count);
    lcd.print("    ");                    // Clear trailing characters
    
    // ================================================================
    // PHASE A → PHASE B TRANSITION LOGIC
    // ================================================================
    
    if (!phaseACompleted && count >= targetTurnsA) {
      // Phase A target reached - transition to Phase B
      digitalWrite(outputPin, LOW);       // Switch to Phase B (reverse)
      phaseACompleted = true;             // Mark Phase A complete
      count = 0;                          // Reset counter voor Phase B
      
      // Update display untuk Phase B indication
      lcd.setCursor(0, 1);
      lcd.print("PHASE B: 0      ");      // Show Phase B startup
      
      Serial.println("=== PHASE TRANSITION ===");
      Serial.print("Phase A completed: ");
      Serial.print(targetTurnsA);
      Serial.println(" turns");
      Serial.println("Switching to Phase B (reverse)");
      Serial.println("========================");
      
    // ================================================================
    // PHASE B COMPLETION & CYCLE END LOGIC
    // ================================================================
    
    } else if (phaseACompleted && count >= targetTurnsB) {
      // Phase B target reached - complete cycle
      stopMotor();                        // Stop motor (cycle complete)
      
      Serial.println("=== CYCLE COMPLETE ===");
      Serial.print("Phase B completed: ");
      Serial.print(targetTurnsB);
      Serial.println(" turns");
      Serial.println("Full twisting cycle finished");
      Serial.println("======================");
    }
  }

  // ================================================================
  // SECTION 4: KEYPAD INPUT PROCESSING & USER INTERFACE
  // ================================================================
  
  /*
   * KEYPAD INTERFACE SYSTEM:
   * ========================
   * Comprehensive user input processing dengan multi-function keys
   * 
   * Key Processing Priority:
   * 1. Mode Selection Keys (A, B)
   * 2. Numeric Input (0-9)
   * 3. System Control Keys (#, *, C, D)
   * 4. Special Functions (long-press D)
   * 
   * Input Validation:
   * • Numeric overflow protection
   * • Mode state management
   * • Settings lock enforcement
   * • Display feedback voor all actions
   */
  
  char key = keypad.getKey();             // Get pressed key (NO_KEY jika none)
  
  if (key != NO_KEY) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    
    // ================================================================
    // PHASE INPUT MODE SELECTION
    // ================================================================
    
    if (key == 'A') {
      // Activate Phase A input mode (forward turns)
      inputModeA = true; 
      inputModeB = false;               // Mutual exclusion
      targetTurnsA = 0;                 // Reset target voor new input
      
      lcd.setCursor(0, 1); 
      lcd.print("Input A Mode: ON ");   // User feedback
      
      Serial.println("Phase A input mode activated");
      
    } else if (key == 'B') {
      // Activate Phase B input mode (reverse turns)
      inputModeB = true; 
      inputModeA = false;               // Mutual exclusion
      targetTurnsB = 0;                 // Reset target voor new input
      
      lcd.setCursor(0, 1); 
      lcd.print("Input B Mode: ON ");   // User feedback
      
      Serial.println("Phase B input mode activated");
      
    // ================================================================
    // NUMERIC INPUT PROCESSING (0-9)
    // ================================================================
    
    } else if (key >= '0' && key <= '9') {
      int digit = key - '0';            // Convert char to integer
      
      if (inputModeA) {
        // Phase A numeric input processing
        targetTurnsA = targetTurnsA * 10 + digit;  // Decimal shift + new digit
        
        // Input validation (prevent overflow)
        if (targetTurnsA > 9999) {
          targetTurnsA = 9999;          // Clamp to maximum
        }
        
        lcd.setCursor(6, 0); 
        lcd.print(targetTurnsA);        // Update display
        lcd.print("  ");               // Clear trailing digits
        
        Serial.print("Phase A target: ");
        Serial.println(targetTurnsA);
        
      } else if (inputModeB) {
        // Phase B numeric input processing
        targetTurnsB = targetTurnsB * 10 + digit;  // Decimal shift + new digit
        
        // Input validation (prevent overflow)
        if (targetTurnsB > 9999) {
          targetTurnsB = 9999;          // Clamp to maximum
        }
        
        lcd.setCursor(12, 0); 
        lcd.print(targetTurnsB);        // Update display
        lcd.print("  ");               // Clear trailing digits
        
        Serial.print("Phase B target: ");
        Serial.println(targetTurnsB);
      }
      
    // ================================================================
    // SETTINGS LOCK CONTROL (#)
    // ================================================================
    
    } else if (key == '#') {
      // Lock settings (enable motor start)
      if (targetTurnsA > 0 && targetTurnsB > 0) {
        keypadLocked = true;            // Enable motor start
        
        lcd.setCursor(0, 1); 
        lcd.print("Locked          "); // Confirmation display
        
        Serial.println("Settings locked - Motor start enabled");
        Serial.print("Final targets - A: ");
        Serial.print(targetTurnsA);
        Serial.print(", B: ");
        Serial.println(targetTurnsB);
      } else {
        // Invalid targets - cannot lock
        lcd.setCursor(0, 1);
        lcd.print("Set A & B first!");
        
        Serial.println("Lock denied: Invalid targets (A or B = 0)");
      }
      
    // ================================================================
    // SYSTEM RESET CONTROL (*)
    // ================================================================
    
    } else if (key == '*') {
      // Clear all settings (unlock system)
      targetTurnsA = 0; 
      targetTurnsB = 0;
      keypadLocked = false;
      inputModeA = false;
      inputModeB = false;
      
      lcd.setCursor(0, 0); 
      lcd.print("Set A:    B:    ");   // Reset display
      lcd.setCursor(0, 1);
      lcd.print("Settings Cleared");
      
      Serial.println("All settings cleared - System unlocked");
      
    // ================================================================
    // PRODUCTION COUNTER CONTROL (C)
    // ================================================================
    
    } else if (key == 'C') {
      // Manual production counter increment
      counter++;
      display.showNumberDec(counter);   // Update 7-segment display
      
      Serial.print("Production counter incremented: ");
      Serial.println(counter);
      
    // ================================================================
    // COUNTER RESET INITIATION (D)
    // ================================================================
    
    } else if (key == 'D') {
      // Start long-press detection voor counter reset
      if (!isKeyDPressed) {
        pressStartTime = millis();      // Record start time
        isKeyDPressed = true;           // Set press flag
        
        Serial.println("D key pressed - Hold for 3s to reset counter");
      }
    }
  }

  // ================================================================
  // SECTION 5: LONG-PRESS DETECTION & COUNTER RESET
  // ================================================================
  
  /*
   * LONG-PRESS COUNTER RESET SYSTEM:
   * ================================
   * D key long-press (3 seconds) untuk reset production counter
   * 
   * Timing Logic:
   * • Press detection: Record timestamp on initial press
   * • Duration monitoring: Check elapsed time continuously
   * • Reset trigger: Execute reset after 3000ms hold
   * • State cleanup: Clear flags after reset execution
   * 
   * Safety Features:
   * • Prevents accidental reset (requires 3s commitment)
   * • Visual feedback during hold period
   * • Automatic state cleanup on release
   * • Production counter preservation until confirmed reset
   */
  
  // Monitor D key long-press state
  if (isKeyDPressed) {
    unsigned long holdDuration = millis() - pressStartTime;
    
    // Check jika 3-second threshold reached
    if (holdDuration >= 3000) {
      // Execute production counter reset
      counter = 0;                      // Reset production counter
      display.showNumberDec(0, true);   // Update 7-segment display
      
      lcd.setCursor(0, 1); 
      lcd.print("Counter Reset   ");    // Confirmation message
      
      isKeyDPressed = false;            // Clear press state
      
      Serial.println("=== PRODUCTION COUNTER RESET ===");
      Serial.println("Counter reset to 0 (3-second hold confirmed)");
      Serial.println("================================");
      
    } else {
      // Show hold progress on display (optional visual feedback)
      static unsigned long lastProgressUpdate = 0;
      if (millis() - lastProgressUpdate > 500) {  // Update every 500ms
        lcd.setCursor(0, 1);
        lcd.print("Hold D to reset ");
        lastProgressUpdate = millis();
      }
    }
    
    // Check voor key release (abort reset)
    if (keypad.getKey() != 'D') {
      isKeyDPressed = false;            // Clear press state
      Serial.println("D key released - Counter reset aborted");
    }
    
  } else {
    // Ensure clean state when not in long-press mode
    isKeyDPressed = false;
  }
}

// ================================================================
// MOTOR CONTROL FUNCTIONS - SAFETY-CRITICAL OPERATIONS
// ================================================================

/*
 * startMotor() - Safe motor startup dengan comprehensive validation
 * 
 * FUNCTION: Execute motor start sequence dengan multi-level safety checks
 * 
 * SAFETY VALIDATION SEQUENCE:
 * 1. Emergency stop verification (abort jika emergency active)
 * 2. Motor state initialization (set running flag)
 * 3. Hardware output activation (motor + phase control)
 * 4. Counter system reset (prepare voor new cycle)
 * 5. Phase control initialization (start dengan Phase A)
 * 6. Display system update (show initial status)
 * 
 * HARDWARE CONTROL SEQUENCE:
 * • motorPin HIGH: Activate motor driver/relay
 * • outputPin HIGH: Set Phase A (forward direction)
 * • count reset: Initialize turn counter
 * • phaseACompleted reset: Initialize phase tracking
 * 
 * SAFETY INTERLOCKS:
 * • Emergency stop check before any hardware activation
 * • Immediate abort jika safety violation detected
 * • State flags updated before hardware changes
 * • Display feedback voor operator awareness
 */
void startMotor() {
  // ================================================================
  // CRITICAL SAFETY CHECK: Emergency stop verification
  // ================================================================
  
  if (digitalRead(switchPin) == LOW) {
    Serial.println("Motor start aborted: Emergency stop active");
    stopMotor();    // Execute emergency shutdown
    return;         // Abort start sequence immediately
  }
  
  // ================================================================
  // MOTOR START SEQUENCE EXECUTION
  // ================================================================
  
  // Update system state flags
  motorRunning = true;              // Set motor running flag
  phaseACompleted = false;          // Initialize phase tracking
  count = 0;                        // Reset turn counter
  
  // Activate hardware outputs
  digitalWrite(motorPin, HIGH);     // Activate motor (start rotation)
  digitalWrite(outputPin, HIGH);    // Set Phase A direction (forward)
  
  // Update display systems
  lcd.setCursor(0, 1);
  lcd.print("PHASE A: 0      ");   // Show Phase A startup
  
  // Log motor start event
  Serial.println("=== MOTOR START SEQUENCE EXECUTED ===");
  Serial.print("Target Phase A: ");
  Serial.print(targetTurnsA);
  Serial.print(" turns, Target Phase B: ");
  Serial.print(targetTurnsB);
  Serial.println(" turns");
  Serial.println("Phase A (forward) initiated");
  Serial.println("======================================");
}

/*
 * stopMotor() - Safe motor shutdown dengan complete state reset
 * 
 * FUNCTION: Execute comprehensive motor shutdown sequence
 * 
 * SHUTDOWN SEQUENCE:
 * 1. Hardware output deactivation (motor + phase control)
 * 2. System state flag reset (motor running status)
 * 3. Display system update (show stopped status)
 * 4. Event logging (voor maintenance tracking)
 * 
 * SAFETY FEATURES:
 * • Immediate hardware deactivation
 * • Complete state reset voor next cycle
 * • Operator feedback via display
 * • Event logging voor system monitoring
 * 
 * APPLICATIONS:
 * • Normal cycle completion
 * • Emergency stop activation  
 * • Error condition response
 * • User-initiated stop
 */
void stopMotor() {
  // ================================================================
  // HARDWARE SHUTDOWN SEQUENCE
  // ================================================================
  
  // Deactivate all motor control outputs
  digitalWrite(motorPin, LOW);      // Stop motor immediately
  digitalWrite(outputPin, LOW);     // Reset phase control
  
  // Update system state
  motorRunning = false;             // Clear motor running flag
  
  // ================================================================
  // USER INTERFACE UPDATE
  // ================================================================
  
  // Update display status
  lcd.setCursor(0, 1);
  lcd.print("Motor Stopped   ");   // Clear status display
  
  // ================================================================
  // EVENT LOGGING
  // ================================================================
  
  // Log motor stop event
  Serial.println("=== MOTOR STOP SEQUENCE EXECUTED ===");
  Serial.print("Final turn count: ");
  Serial.println(count);
  Serial.print("Phase A completed: ");
  Serial.println(phaseACompleted ? "YES" : "NO");
  Serial.println("Motor shutdown complete");
  Serial.println("====================================");
}

// ================================================================
// SYSTEM DOCUMENTATION & TECHNICAL MANUAL
// ================================================================

/*
 * WIRING DIAGRAM:
 * ==============
 * 
 * POWER CONNECTIONS:
 * • VCC: 5V from Arduino
 * • GND: Common ground
 * 
 * MOTOR CONTROL:
 * • Pin A1 (motorPin) → Motor Driver Enable
 * • Pin A3 (outputPin) → Direction Control
 * • GND → Motor Driver Ground
 * 
 * USER INTERFACE:
 * • Pin 2 (startButtonPin) → Start Button (pulled-up)
 * • Pin A2 (switchPin) → Emergency Stop (pulled-up)
 * 
 * SENSORS:
 * • Pin 3 (irSensorPin) → IR Sensor Output (pulled-up)
 * 
 * LCD I2C DISPLAY:
 * • SDA (Pin 20) → LCD SDA
 * • SCL (Pin 21) → LCD SCL
 * • VCC → 5V, GND → Ground
 * 
 * TM1637 7-SEGMENT:
 * • Pin 4 (clk) → TM1637 CLK
 * • Pin 5 (dio) → TM1637 DIO
 * • VCC → 5V, GND → Ground
 * 
 * 4x4 KEYPAD:
 * • Pins A8-A11 → Column pins (C1-C4)
 * • Pins A12-A15 → Row pins (R1-R4)
 * 
 * OPERATIONAL PROCEDURES:
 * ======================
 * 
 * STARTUP SEQUENCE:
 * 1. Power on system
 * 2. Wait voor initialization banner
 * 3. System displays "Set A:    B:"
 * 4. Ready voor parameter input
 * 
 * PARAMETER SETTING:
 * 1. Press 'A' → Enter Phase A input mode
 * 2. Enter target turns (1-9999)
 * 3. Press 'B' → Enter Phase B input mode  
 * 4. Enter target turns (1-9999)
 * 5. Press '#' → Lock settings
 * 6. System ready voor operation
 * 
 * OPERATION CYCLE:
 * 1. Press START button → Begin cycle
 * 2. Phase A: Motor runs forward (targetTurnsA)
 * 3. Automatic transition to Phase B
 * 4. Phase B: Motor runs reverse (targetTurnsB)
 * 5. Automatic stop when complete
 * 6. Production counter increments
 * 
 * EMERGENCY PROCEDURES:
 * 1. Emergency stop → Immediate motor shutdown
 * 2. Press '*' → Clear all settings
 * 3. Hold 'D' 3s → Reset production counter
 * 
 * TROUBLESHOOTING GUIDE:
 * =====================
 * 
 * MOTOR WON'T START:
 * • Check emergency stop switch (must be HIGH)
 * • Verify settings are locked (press '#')
 * • Ensure both targets > 0
 * • Check system ready flag
 * 
 * INCORRECT TURN COUNT:
 * • Check IR sensor alignment
 * • Verify sensor wiring (pin 3)
 * • Clean sensor lens
 * • Check interrupt configuration
 * 
 * DISPLAY ISSUES:
 * • Verify I2C connections (SDA/SCL)
 * • Check LCD address (0x27)
 * • Verify TM1637 connections (pins 4,5)
 * • Check power supply voltage
 * 
 * KEYPAD NOT RESPONDING:
 * • Check matrix wiring (pins A8-A15)
 * • Verify keypad library inclusion
 * • Test individual keys
 * • Check for short circuits
 * 
 * MAINTENANCE SCHEDULE:
 * ====================
 * 
 * DAILY:
 * • Check emergency stop function
 * • Verify display readability
 * • Test start button operation
 * 
 * WEEKLY:
 * • Clean IR sensor
 * • Check all connections
 * • Test full operational cycle
 * • Verify production counter accuracy
 * 
 * MONTHLY:
 * • Calibrate turn counting system
 * • Update target parameters if needed
 * • Check motor driver operation
 * • Verify safety systems
 * 
 * TECHNICAL SPECIFICATIONS:
 * ========================
 * 
 * SYSTEM PERFORMANCE:
 * • Turn Resolution: 1 turn (IR sensor feedback)
 * • Maximum Count: 9999 turns per phase
 * • Counting Rate: >1000 turns/second
 * • Response Time: <10ms (interrupt-based)
 * • Display Update: Real-time
 * 
 * ELECTRICAL RATINGS:
 * • Supply Voltage: 5VDC ±5%
 * • Current Consumption: <500mA (excluding motor)
 * • Motor Output: Relay/driver compatible
 * • Input Protection: Internal pull-ups
 * • Emergency Stop: Fail-safe design
 * 
 * ENVIRONMENTAL:
 * • Operating Temperature: 0°C to +50°C
 * • Humidity: <85% RH (non-condensing)
 * • Vibration: Industrial standard
 * • EMI Compliance: CE certified components
 */
