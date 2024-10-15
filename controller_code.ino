#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>

// REPLACE WITH YOUR RECEIVER MAC Address (Game Console MAC)
uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x59, 0x41, 0xF0};

// Structure to send data to the game console
typedef struct struct_message {
  char a[32];   // Additional info (e.g., ID or debugging)
  int b;        // Button for moving between games (Left Button)
  float c;      // Placeholder (if needed for future extension)
  bool d;       // Button for selecting the current game (Right Button)
  int command1; // JOYSTICK COMMAND1
  int command2; // JOYSTICK COMMAND2
  float mykalmanAnglePitch; // Pitch angle from Kalman filter
  float myKalmanAngleRoll;  // Roll angle from Kalman filter
  int button1;
  int button2;
  int button3;
  int vibrator;
} struct_message;

struct_message myData;  // Create a struct_message called myData

/////////////////////////////////////////////// BUTTON INPUT /////////////////////////////////////////////////////////////
const int buttonPin1 = 17;
const int buttonPin2 = 18;
const int buttonPin3 = 44;  // Corrected from buttonPin1
const int vibrator = 43;

// Variables for debouncing built-in buttons
int lastMoveState = HIGH;
int lastSelectState = HIGH;
unsigned long lastMoveDebounceTime = 0;
unsigned long lastSelectDebounceTime = 0;
const unsigned long debounceDelay = 50; // Debounce delay in milliseconds
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
float KalmanAngleRoll = 0, KalmanUncertaintyAngleRoll = 2 * 2;
float KalmanAnglePitch = 0, KalmanUncertaintyAnglePitch = 2 * 2;
float Kalman1DOutput[] = {0, 0};

void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement) {
  KalmanState = KalmanState + 0.004 * KalmanInput;
  KalmanUncertainty = KalmanUncertainty + 0.004 * 0.004 * 4 * 4;
  float KalmanGain = KalmanUncertainty * 1 / (1 * KalmanUncertainty + 3 * 3);
  KalmanState = KalmanState + KalmanGain * (KalmanMeasurement - KalmanState);
  KalmanUncertainty = (1 - KalmanGain) * KalmanUncertainty;
  Kalman1DOutput[0] = KalmanState;
  Kalman1DOutput[1] = KalmanUncertainty;
}

void gyro_signals(void) {
  // THE MPU9250 SENSOR READINGS
  int16_t accelX, accelY, accelZ;
  int16_t gyroX, gyroY, gyroZ;

  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // Wake up the MPU-9250
  Wire.endTransmission(true);

  // Read accelerometer data
  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU9250_ADDRESS, (size_t)6, (bool)true);

  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();

  // Read gyroscope data
  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(GYRO_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU9250_ADDRESS, (size_t)6, (bool)true);

  gyroX = Wire.read() << 8 | Wire.read();
  gyroY = Wire.read() << 8 | Wire.read();
  gyroZ = Wire.read() << 8 | Wire.read();

  RateRoll = (float)gyroX;
  RatePitch = (float)gyroY;
  RateYaw = (float)gyroZ;
  AccX = (float)accelX;
  AccY = (float)accelY;
  AccZ = (float)accelZ;
  AngleRoll = atan(AccY / sqrt(AccX * AccX + AccZ * AccZ)) * 1 / (3.142 / 180);
  AnglePitch = -atan(AccX / sqrt(AccY * AccY + AccZ * AccZ)) * 1 / (3.142 / 180);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////// Controller Inputs ////////////////////////////////////////////////////////

#define VRX_PIN 13 // Arduino pin connected to VRX pin
#define VRY_PIN 12 // Arduino pin connected to VRY pin

#define VIBRATION_PIN 3

#define LEFT_THRESHOLD  1000
#define RIGHT_THRESHOLD 3000
#define UP_THRESHOLD    3000
#define DOWN_THRESHOLD  1000

#define COMMAND_NO     0x00
#define COMMAND_LEFT   0x01
#define COMMAND_RIGHT  0x02
#define COMMAND_UP     0x04
#define COMMAND_DOWN   0x08

float xValue = 0.0; // To store value of the X axis
float yValue = 0.0; // To store value of the Y axis

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

esp_now_peer_info_t peerInfo;

// Built-in Button input pins on LilyGO T-Beam S3
const int movePin = 0;     // Left Button (GPIO 0)
const int selectPin = 14;  // Right Button (GPIO 14)
const int joystickXPin = 13; // X-axis pin
const int joystickYPin = 12; // Y-axis pin

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize struct variables
  myData.command1 = COMMAND_NO;
  myData.command2 = COMMAND_NO;
  myData.mykalmanAnglePitch = 0;
  myData.myKalmanAngleRoll = 0;

  // BUTTON & VIBRATOR SETUP
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(vibrator, OUTPUT);

  // GYROSCOPE SETUP
  Wire.setClock(400000);
  Wire.begin(43, 44);
  delay(250);
  Wire.beginTransmission(MPU9250_ADDRESS);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  for (RateCalibrationNumber = 0; RateCalibrationNumber < 2000; RateCalibrationNumber++) {
    gyro_signals();
    RateCalibrationRoll += RateRoll;
    RateCalibrationPitch += RatePitch;
    RateCalibrationYaw += RateYaw;
    delay(1);
  }
  RateCalibrationRoll /= 2000;
  RateCalibrationPitch /= 2000;
  RateCalibrationYaw /= 2000;
  LoopTimer = micros();

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
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  // BUTTON CODE
  // Read the current state of buttons
  myData.button1 = digitalRead(buttonPin1);
  myData.button2 = digitalRead(buttonPin2);
  myData.button3 = digitalRead(buttonPin3);

  // GYROSCOPE LOOP
  gyro_signals();
  RateRoll -= RateCalibrationRoll;
  RatePitch -= RateCalibrationPitch;
  RateYaw -= RateCalibrationYaw;

  kalman_1d(KalmanAngleRoll, KalmanUncertaintyAngleRoll, RateRoll, AngleRoll);
  KalmanAngleRoll = Kalman1DOutput[0];
  myData.myKalmanAngleRoll = KalmanAngleRoll;
  KalmanUncertaintyAngleRoll = Kalman1DOutput[1];

  kalman_1d(KalmanAnglePitch, KalmanUncertaintyAnglePitch, RatePitch, AnglePitch);
  KalmanAnglePitch = Kalman1DOutput[0];
  myData.mykalmanAnglePitch = KalmanAnglePitch;
  KalmanUncertaintyAnglePitch = Kalman1DOutput[1];

  // JOYSTICK
  // Read the current state of the joystick
  xValue = analogRead(joystickXPin); // Read joystick X-axis value
  yValue = analogRead(joystickYPin); // Read joystick Y-axis value

  // Reset commands
  myData.command1 = COMMAND_NO;
  myData.command2 = COMMAND_NO;

  // Check left/right commands for joystick 1
  if (xValue < LEFT_THRESHOLD)
    myData.command1 |= COMMAND_LEFT;
  else if (xValue > RIGHT_THRESHOLD)
    myData.command1 |= COMMAND_RIGHT;

  // Check up/down commands for joystick 1
  if (yValue > UP_THRESHOLD)
    myData.command1 |= COMMAND_UP;
  else if (yValue < DOWN_THRESHOLD)
    myData.command1 |= COMMAND_DOWN;

  // Check left/right commands for joystick 2 (if applicable)
  // Modify as needed if you have a second joystick

  // Read the current state of the built-in buttons
  int moveState = digitalRead(movePin);
  int selectState = digitalRead(selectPin);

  // Check for move button press (button goes from HIGH to LOW)
  if (moveState == LOW && lastMoveState == HIGH && (millis() - lastMoveDebounceTime) > debounceDelay) {
    myData.b = 1; // Move between games
    lastMoveDebounceTime = millis(); // Update debounce time
  } else {
    myData.b = 0; // No movement
  }

  // Check for select button press (button goes from HIGH to LOW)
  if (selectState == LOW && lastSelectState == HIGH && (millis() - lastSelectDebounceTime) > debounceDelay) {
    myData.d = true; // Select the current game
    lastSelectDebounceTime = millis(); // Update debounce time
  } else {
    myData.d = false; // No selection
  }

  // Store the current button states for next loop
  lastMoveState = moveState;
  lastSelectState = selectState;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  } else {
    Serial.println("Error sending the data");
  }

  delay(50); // Small delay to avoid flooding the console with messages
}
