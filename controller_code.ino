#include <esp_now.h>
#include <WiFi.h>


// REPLACE WITH YOUR RECEIVER MAC Address (Game Console MAC)
uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x59, 0xCB, 0xDC};

// Structure to send data to the game console
typedef struct struct_message {
  char a[32];   // Additional info (e.g., ID or debugging)
  int b;        // Button for moving between games (Left Button)
  float c;      // Placeholder (if needed for future extension)
  bool d;       // Button for selecting the current game (Right Button)
  // int joystickX; // Joystick X-axis value
  // int joystickY; // Joystick Y-axis value
  int command; //JOYSTICK COMMAND
} struct_message;

struct_message myData;  // Create a struct_message called myData

/////////////////////////////////////////////// Controller Inputs ////////////////////////////////////////////////////////

const int buttonPin = 11;

#define VRX_PIN  13 // Arduino pin connected to VRX pin
#define VRY_PIN  12 // Arduino pin connected to VRY pin
#define BUT_PIN 10
#define VIBRATION_PIN 3

// #define LEFT_THRESHOLD  250
// #define RIGHT_THRESHOLD 412
// #define UP_THRESHOLD    412
// #define DOWN_THRESHOLD  250
#define LEFT_THRESHOLD  1000
#define RIGHT_THRESHOLD 3000
#define UP_THRESHOLD    3000
#define DOWN_THRESHOLD  1000

#define COMMAND_NO     0x00
#define COMMAND_LEFT   0x01
#define COMMAND_RIGHT  0x02
#define COMMAND_UP     0x04
#define COMMAND_DOWN   0x08

float xValue = 0.0 ; // To store value of the X axis
float yValue = 0.0 ; // To store value of the Y axis
int bValue = 0;
// int command = COMMAND_NO; // This is the default command (none)
float ratio = 0.183;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


esp_now_peer_info_t peerInfo;

// Built-in Button input pins on LilyGO T-Beam S3
const int movePin = 0;  // Left Button (GPIO 0)
const int selectPin = 14;  // Right Button (GPIO 14)
const int joystickXPin = 13;  // X-axis pin
const int joystickYPin = 12;  // Y-axis pin

// Variables to store previous button states (for debouncing)
int lastMoveState = HIGH;
int lastSelectState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // Debounce delay in milliseconds

// Callback when data is sent
// 
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // THIS IS THE INITIAL COMMAND
  myData.command = COMMAND_NO;

  // Initialize built-in button input pins with internal pull-up resistors
  pinMode(movePin, INPUT_PULLUP);
  pinMode(selectPin, INPUT_PULLUP);
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register send callback to get the status of transmitted packets
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer (Game Console MAC Address)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {

  // Read the current state of the joystick
  xValue = analogRead(joystickXPin);  // Read joystick X-axis value
  yValue = analogRead(joystickYPin);  // Read joystick Y-axis value

  // Interpret the joy stick data
  // converts the analog value to commands 
  // reset commands
  myData.command = COMMAND_NO;

  // check left/right commands
  if (xValue < LEFT_THRESHOLD)
    //Bitwise OR
    myData.command = myData.command | COMMAND_LEFT;
  else if (xValue > RIGHT_THRESHOLD)
    myData.command = myData.command | COMMAND_RIGHT;

  // check up/down commands
  if (yValue > UP_THRESHOLD)
    myData.command = myData.command | COMMAND_UP;
  else if (yValue < DOWN_THRESHOLD)
    myData.command = myData.command | COMMAND_DOWN;

  // Read the current state of the buttons
  int moveState = digitalRead(movePin);
  int selectState = digitalRead(selectPin);

  // Check for move button press (button goes from HIGH to LOW)
  if (moveState == LOW && lastMoveState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    myData.b = 1;  // Move between games
    lastDebounceTime = millis();  // Update debounce time
  } else {
    myData.b = 0;  // No movement
  }

  // Check for select button press (button goes from HIGH to LOW)
  if (selectState == LOW && lastSelectState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    myData.d = true;  // Select the current game
    lastDebounceTime = millis();  // Update debounce time
  } 
  else {
    myData.d = false;  // No selection
  }

  // Store the current button states for next loop
  lastMoveState = moveState;
  lastSelectState = selectState;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  } else {
    Serial.println("Error sending the data");
  }

  delay(50);  // Small delay to avoid flooding the console with messages
}
