#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#define PI 3.14159265358979323846


// REPLACE WITH YOUR RECEIVER MAC Address (Game Console MAC)
// uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x59, 0xCB, 0xDC};
uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x59, 0x41, 0xF0};

// Structure to send data to the game console
typedef struct struct_message {
  char a[32];                   // 32 bytes
  int32_t b;                    // 4 bytes
  float c;                      // 4 bytes
  uint8_t d;                    // 1 byte
  // No padding if packed
  int32_t command1;             // 4 bytes
  int32_t command2;             // 4 bytes
  float mykalmanAnglePitch;     // 4 bytes
  float myKalmanAngleRoll;      // 4 bytes
  int32_t button1;              // 4 bytes
  int32_t button2;              // 4 bytes
  int32_t button3;              // 4 bytes
  int32_t vibrator;             // 4 bytes
} struct_message;



struct_message myData;  // Create a struct_message called myData

/////////////////////////////////////////////// BUTTON INPUT /////////////////////////////////////////////////////////////
const int buttonPin1 = 17;
const int buttonPin2 = 18;
const int buttonPin3 = 44;
const int vibrator = 43;
int xValue1 = 0;
int yValue1 = 0;
int xValue2 = 0;
int yValue2 = 0;

// Variables for debouncing
int buttonState = HIGH;         // Current state of the button
int lastButtonState = HIGH;     // Previous state of the button
unsigned long lastDebounceTime = 0;  // Timestamp of the last state change
unsigned long debounceDelay = 50;    // Debounce delay in milliseconds
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////// Gyroscope Inputs /////////////////////////////////////////////////////////
// THE MPU9250 I2C ADDRESSES
const uint8_t MPU9250_ADDRESS = 0x68; // MPU9250 I2C address
const int ACCEL_XOUT_H = 0x3B;   // Register address for accelerometer X-axis high byte
const int GYRO_XOUT_H = 0x43;    // Register address for gyroscope X-axis high byte
const int MAG_XOUT_L = 0x03;     // Register address for magnetometer X-axis low byte

float RateRoll, RatePitch, RateYaw;
float RateCalibrationRoll, RateCalibrationPitch, RateCalibrationYaw;
int RateCalibrationNumber;
float AccX, AccY, AccZ;
float AngleRoll, AnglePitch;
uint32_t LoopTimer;
float KalmanAngleRoll=0, KalmanUncertaintyAngleRoll=2*2;
float KalmanAnglePitch=0, KalmanUncertaintyAnglePitch=2*2;
float Kalman1DOutput[]={0,0};
void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement) {
  KalmanState=KalmanState+0.004*KalmanInput;
  KalmanUncertainty=KalmanUncertainty + 0.004 * 0.004 * 4 * 4;
  float KalmanGain=KalmanUncertainty * 1/(1*KalmanUncertainty + 3 * 3);
  KalmanState=KalmanState+KalmanGain * (KalmanMeasurement-KalmanState);
  KalmanUncertainty=(1-KalmanGain) * KalmanUncertainty;
  Kalman1DOutput[0]=KalmanState; 
  Kalman1DOutput[1]=KalmanUncertainty;
}

void gyro_signals(void) {

  // THE MPU9250 SENSOR READINGS
  int16_t accelX, accelY, accelZ;
  int16_t gyroX, gyroY, gyroZ;
  int16_t magX, magY, magZ;

  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // Wake up the MPU-9250
  Wire.endTransmission(true);

  // Read accelerometer data
  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU9250_ADDRESS, 6, true);
  
  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();

  // Read gyroscope data
  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(GYRO_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU9250_ADDRESS, 6, true);
    
  gyroX = Wire.read() << 8 | Wire.read();
  gyroY = Wire.read() << 8 | Wire.read(); 
  gyroZ = Wire.read() << 8 | Wire.read();

//   RateRoll=(float)gyroX/65.5;
//   RatePitch=(float)gyroY/65.5;
//   RateYaw=(float)gyroZ/65.5;
//   AccX=(float)accelX/4096;
//   AccY=(float)accelY/4096;
//   AccZ=(float)accelZ/4096;
  RateRoll=(float)gyroX;
  RatePitch=(float)gyroY;
  RateYaw=(float)gyroZ;
  AccX=(float)accelX;
  AccY=(float)accelY;
  AccZ=(float)accelZ;
  AngleRoll=atan(AccY/sqrt(AccX*AccX+AccZ*AccZ))*1/(3.142/180);
  AnglePitch=-atan(AccX/sqrt(AccY*AccY+AccZ*AccZ))*1/(3.142/180);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
// int command1 = COMMAND_NO; // This is the default command (none)
float ratio = 0.183;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


esp_now_peer_info_t peerInfo;

// Built-in Button input pins on LilyGO T-Beam S3
const int movePin = 0;  // Left Button (GPIO 0)
const int selectPin = 14;  // Right Button (GPIO 14)
const int joystickXPin1 = 13;  // X-axis pin
const int joystickYPin1 = 12;  // Y-axis pin
const int joystickXPin2 = 11;  // X-axis pin
const int joystickYPin2 = 10;  // Y-axis pin

// Variables to store previous button states (for debouncing)
int lastMoveState = HIGH;
int lastSelectState = HIGH;

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
  myData.command1 = COMMAND_NO;
  myData.command2 = COMMAND_NO;

  /////////////////////////// BUTTON & VIBRATOR SETUP /////////////////////////////////////////////////////////////////////////////
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(vibrator, OUTPUT);
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /////////////////////////// GYROSCOPE SETUP /////////////////////////////////////////////////////////////////////////////////////
  Wire.setClock(400000);
  Wire.begin(21,16);
  delay(250);
  Wire.beginTransmission(0x68); 
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  for (RateCalibrationNumber=0; RateCalibrationNumber<2000; RateCalibrationNumber ++) {
    gyro_signals();
    RateCalibrationRoll+=RateRoll;
    RateCalibrationPitch+=RatePitch;
    RateCalibrationYaw+=RateYaw;
    delay(1);
  }
  RateCalibrationRoll/=2000;
  RateCalibrationPitch/=2000;
  RateCalibrationYaw/=2000;
  LoopTimer=micros();
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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


  //////////////////////////////////////// BUTTON CODE //////////////////////////////////////////////////////////////////////////////////
  // Read the current state of button1
  myData.button1 = digitalRead(buttonPin1);
  // Read the current state of button2
  myData.button2 = digitalRead(buttonPin2);
  // Read the current state of button3
  myData.button3 = digitalRead(buttonPin3);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////// GYROSCOPE LOOP ///////////////////////////////////////////////////////////////////////////////
  gyro_signals();
  RateRoll-=RateCalibrationRoll;
  RatePitch-=RateCalibrationPitch;
  RateYaw-=RateCalibrationYaw;
  kalman_1d(KalmanAngleRoll, KalmanUncertaintyAngleRoll, RateRoll, AngleRoll);
  KalmanAngleRoll=Kalman1DOutput[0]; 
  myData.mykalmanAnglePitch = KalmanAngleRoll;
  KalmanUncertaintyAngleRoll=Kalman1DOutput[1];
  kalman_1d(KalmanAnglePitch, KalmanUncertaintyAnglePitch, RatePitch, AnglePitch);
  KalmanAnglePitch=Kalman1DOutput[0]; 
  myData.mykalmanAnglePitch = KalmanAnglePitch;
  KalmanUncertaintyAnglePitch=Kalman1DOutput[1];
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////// JOYSTICK /////////////////////////////////////////////////////////////////////////////

// Swap X and Y readings and invert them
xValue1 = 4095 - analogRead(joystickYPin1);  // Swapped and inverted for joystick 1
yValue1 = 4095 - analogRead(joystickXPin1);  // Swapped and inverted for joystick 1
xValue2 = 4095 - analogRead(joystickYPin2);  // Swapped and inverted for joystick 2
yValue2 = 4095 - analogRead(joystickXPin2);  // Swapped and inverted for joystick 2

  // Interpret the joy stick data
  // converts the analog value to commands 
  // reset commands
  myData.command1 = COMMAND_NO;
  myData.command2 = COMMAND_NO;

  // check left/right commands for joystick 1
  if (xValue1 < LEFT_THRESHOLD)
    //Bitwise OR
    myData.command1 = myData.command1 | COMMAND_LEFT;
  else if (xValue1 > RIGHT_THRESHOLD)
    myData.command1 = myData.command1 | COMMAND_RIGHT;

  // check up/down commands for joystick 1
  if (yValue1 > UP_THRESHOLD)
    myData.command1 = myData.command1 | COMMAND_UP;
  else if (yValue1 < DOWN_THRESHOLD)
    myData.command1 = myData.command1 | COMMAND_DOWN;

  // check left/right commands for joystick 2
  if (xValue2 < LEFT_THRESHOLD)
    //Bitwise OR
    myData.command2 = myData.command2 | COMMAND_LEFT;
  else if (xValue2 > RIGHT_THRESHOLD)
    myData.command2 = myData.command2 | COMMAND_RIGHT;

  // check up/down commands for joystick 2
  if (yValue2 > UP_THRESHOLD)
    myData.command2 = myData.command2 | COMMAND_UP;
  else if (yValue2 < DOWN_THRESHOLD)
    myData.command2 = myData.command2 | COMMAND_DOWN;

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
