#include "ESP32S3VGA.h"
#include "GfxWrapper.h"
#include <esp_now.h>
#include <WiFi.h>
#include <map>
#include <string.h> // For strcmp
#include <math.h>   // For math functions
#include <vector>

// =========================
// VGA Configuration
// =========================

// VGA Pin Configuration
const PinConfig pins(-1, -1, -1, -1, 1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, 3, 13, 12);

// VGA Device and Mode Setup
VGA vga;
Mode mode = Mode::MODE_320x240x60; // 320x240 resolution at 60Hz
GfxWrapper<VGA> gfx(vga, mode.hRes, mode.vRes);

// =========================
// ESP-NOW Configuration
// =========================

// Structure to receive data from the controller
typedef struct struct_message {
  char a[32];   // Identifier (e.g., unique ID for the controller)
  int b;        // Button for moving between menu items (e.g., -1 for up, 1 for down)
  float c;      // Placeholder (if needed for future extension)
  bool d;       // Button for selecting the current menu item
} struct_message;

// Map to store data from each controller
std::map<int, struct_message> controllerData;

struct_message myData;  // Struct to hold the received data

// =========================
// Application State
// =========================

enum AppState { 
  STARTUP, 
  MENU_MAIN, 
  MENU_PLAYER_COUNT,
  GAME_RUNNING
};
AppState appState = STARTUP;

// =========================
// Controller Mapping
// =========================

std::map<String, int> controllerMap;  // Key: MAC address string, Value: Controller number

// =========================
// Menu Items
// =========================

const char* mainMenuItems[] = {"Pong", "Snake", "Flappy Bird"};
const int totalMainMenuItems = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char* playerCountMenuItems[] = {"Single Player", "Two Player"};
const int totalPlayerCountMenuItems = sizeof(playerCountMenuItems) / sizeof(playerCountMenuItems[0]);

int currentMainMenuItem = 0;       // Current selection in the main menu
int currentPlayerCountItem = 0;    // Current selection in the player count menu
const char* selectedGame = nullptr; // Pointer to the selected game

// =========================
// Timing Variables
// =========================

unsigned long startupStartTime = 0;
const long startupDuration = 5000; // Display "Scorpio" for 5 seconds

// =========================
// Color Definitions
// =========================

uint16_t blackColor = 0x0000;
uint16_t whiteColor = 0xFFFF;
uint16_t highlightColor = 0xF800; // Red
uint16_t greenColor = 0x07E0;
uint16_t redColor = 0xF800;
uint16_t blueColor = 0x001F;
uint16_t paddleColor = 0xFFFF;
uint16_t ballColor = 0xFFFF;

// =========================
// Screen Dimensions
// =========================

int screenWidth;
int screenHeight;

// =========================
// Function Declarations
// =========================

// Menu Functions
void displayStartupScreen();
void displayMenuMain();
void displayMenuPlayerCount();
void clearScreen();

// ESP-NOW Callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);

// Utility Functions for Text Dimensions
int calculateTextWidth(const char* text, int textSize);
int calculateTextHeight(int textSize);

// Game Functions
void runSelectedGame();
void runPong(bool isSinglePlayer);
void handlePlayerInputs(bool isSinglePlayer);
void updateBall();
void checkPaddleCollisions();
void drawGame();
void resetBall();
void handlePaddleCollision(int paddleX, int paddleY, bool isPlayerPaddle);
void drawCenterLine();
void displayScores();
void checkWinCondition();
void displayWinMessage(const char* winner);
void setupServe(bool isSinglePlayer);

// Flappy Bird Functions
void resetFlappyBirdGame(bool isSinglePlayer);
void runFlappyBird(bool isSinglePlayer);
void runFlappyBirdSinglePlayer();
void runFlappyBirdTwoPlayer();
void drawFlappyBirdGameSinglePlayer();
void drawFlappyBirdGameTwoPlayer();
void handleFlappyBirdCollisionsSinglePlayer();
void handleFlappyBirdCollisionsTwoPlayer();

// Snake Game Functions
void resetSnakeGame(bool isSinglePlayer);
void runSnake(bool isSinglePlayer);
void runSnakeSinglePlayer();
void runSnakeTwoPlayer();
void updateSnakeSinglePlayer();
void updateSnakeTwoPlayer();
void drawSnakeGameSinglePlayer();
void drawSnakeGameTwoPlayer();
void handleSnakeInputSinglePlayer();
void handleSnakeInputTwoPlayer();
void displaySnakeGameOver(const char* message);

// =========================
// Game Variables (Pong)
// =========================

// Ball Properties
float ballX;
float ballY;
float ballRadius = 3;        // Radius of the ball
float ballSpeedX;
float ballSpeedY;
float ballSpeed = 3.0;       // Total speed of the ball
float speedIncrement = 0.2;  // Speed increment after each paddle hit

// Paddle Properties
int paddleWidth = 5;     // Width of the paddles (vertical paddles)
int paddleHeight = 40;   // Height of the paddles
int playerPaddleX = 20;  // X position of player paddle (left side)
int playerPaddleY;       // Y position of player paddle
int playerSpeed = 5;     // Speed of player paddle

int aiPaddleX = mode.hRes - 25; // X position of AI paddle (right side)
int aiPaddleY;                  // Y position of AI paddle
int aiSpeed = 4;                // Speed of AI paddle

// Scoring
int playerScore = 0;
int aiScore = 0;
int maxScore = 5;

// Game State Flags
enum GameState { GAME_PLAYING, GAME_OVER };
GameState gameState = GAME_PLAYING;
bool serve = true;
bool playerServe = true;

// Collision Cooldown
bool collisionOccurred = false;

// Timing variables
unsigned long previousMillis = 0;      // Store the last time the ball position was updated
const long interval = 16;              // Update interval in milliseconds (approx. 60 updates per second)
unsigned long collisionCooldown = 0;   // Cooldown timer to prevent repeated collisions

// =========================
// Flappy Bird Game Variables
// =========================

// Screen dimensions
const int fbScreenWidth = 320;
const int fbScreenHeight = 240;

// Timing
unsigned long fbLastFrameTime = 0;
const int fbFrameInterval = 20; // milliseconds

// Bird properties
float fbBirdX;
float fbBirdY;
float fbBirdSpeedY;
float fbGravity = 0.3f;
float fbJumpStrength = -6.0f;

// Pipes
int fbPipeX;
int fbPipeGapY;
int fbPipeGapSize = 60;
int fbPipeSpeed = 2;

// Ground
int fbGroundOffset = 0;
int fbGroundSpeed = fbPipeSpeed / 2;

// Scores
int fbScore = 0;

// Game state
bool fbGameInitialized = false;
bool fbCollision = false;

// Multiplayer
bool fbIsSinglePlayer = true;
bool fbPlayer1Alive = true;
bool fbPlayer2Alive = true;
float fbBirdY1;
float fbBirdY2;
float fbBirdSpeedY1;
float fbBirdSpeedY2;
int fbPipeX1;
int fbPipeX2;
int fbPipeGapY1;
int fbPipeGapY2;
int fbScore1;
int fbScore2;

// =========================
// Snake Game Variables
// =========================

// Game Constants
const int gridSize = 10;  // Size of each grid cell in pixels
const int gridWidth = mode.hRes / gridSize;
const int gridHeight = mode.vRes / gridSize;

// Snake Properties
struct Point {
    int x;
    int y;
};

// Single-player variables
std::vector<Point> snake;
int snakeDirection = 0;  // 0=Up, 1=Right, 2=Down, 3=Left
Point food;
int snakeScore = 0;

// Two-player variables
std::vector<Point> snake1;
std::vector<Point> snake2;
int snakeDirection1 = 0;  // Player 1
int snakeDirection2 = 0;  // Player 2
Point food1;
Point food2;
int snakeScore1 = 0;
int snakeScore2 = 0;
bool snakeGameInitialized = false;
bool snakePlayer1Alive = true;
bool snakePlayer2Alive = true;
unsigned long snakePreviousMillis = 0;
const long snakeInterval = 100;  // Snake movement interval in milliseconds

// =========================
// Setup Function
// =========================

void setup() {
  // Initialize Serial for Debugging
  Serial.begin(115200);
  Serial.println("VGA Menu System with ESP-NOW Controller");

  // Initialize VGA with GfxWrapper
  vga.bufferCount = 2; // Double buffering for smoother display
  if (!vga.init(pins, mode, 16)) {
    Serial.println("VGA Initialization Failed!");
    while (1) delay(1); // Halt if VGA fails to initialize
  }
  vga.start();

  // Get screen dimensions
  screenWidth = gfx.width();
  screenHeight = gfx.height();

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setCursor(10, 40);
    gfx.println("ESP-NOW Init Failed");
    vga.show();
    while (1) delay(1); // Halt if ESP-NOW fails to initialize
  }

  // Register the callback function for receiving data
  esp_now_register_recv_cb(OnDataRecv);

  // Seed the random number generator
  randomSeed(analogRead(0));

  // Display startup screen
  displayStartupScreen();
}

// =========================
// Loop Function
// =========================

void loop() {
  unsigned long currentMillis = millis();

  switch(appState) {
    case STARTUP:
      if (currentMillis - startupStartTime >= startupDuration) {
        appState = MENU_MAIN;
        displayMenuMain();
      }
      break;

    case MENU_MAIN:
      // Menu is handled via controller inputs
      break;

    case MENU_PLAYER_COUNT:
      // Player count selection is handled via controller inputs
      break;

    case GAME_RUNNING:
      runSelectedGame();
      break;
  }

  // Refresh the VGA display
  vga.show();
}

// =========================
// ESP-NOW Callback Function
// =========================

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) {
    Serial.println("Received data of incorrect size");
    return;
  }

  String macStr = "";
  for(int i = 0; i < 6; i++) {
    if(mac[i] < 16) { // Add leading zero if necessary
      macStr += "0";
    }
    macStr += String(mac[i], HEX);
    if(i < 5) macStr += ":";
  }

  // Assign controller to a controller number if not already assigned
  if (controllerMap.find(macStr) == controllerMap.end()) {
    if(controllerMap.size() < 4) {  // Allow up to 4 controllers
      controllerMap[macStr] = controllerMap.size() + 1;  // Assign Controller 1, 2, etc.
      Serial.print("Assigned Controller ");
      Serial.print(macStr);
      Serial.print(" to Controller ");
      Serial.println(controllerMap[macStr]);
    } else {
      Serial.println("Maximum number of controllers reached");
      return; // Ignore additional controllers
    }
  }

  int controllerNumber = controllerMap[macStr];

  // Store incoming data for this controller
  struct_message data;
  memcpy(&data, incomingData, sizeof(data));
  controllerData[controllerNumber] = data;

  // Update controller inputs based on current app state
  if(appState == MENU_MAIN) {
    // Handle navigation in main menu
    if (data.d) { // Select button pressed
      // Handle selection based on currentMainMenuItem
      selectedGame = mainMenuItems[currentMainMenuItem];
      Serial.print("Selected Game: ");
      Serial.println(selectedGame);

      // Transition to player count selection if needed
      if (strcmp(selectedGame, "Pong") == 0 || 
          strcmp(selectedGame, "Snake") == 0 || 
          strcmp(selectedGame, "Flappy Bird") == 0) {
        appState = MENU_PLAYER_COUNT;
        displayMenuPlayerCount();
      }
      // Add additional game selections as needed
    } else {
      // Handle navigation
      if (data.b == 1) { // Move down
        currentMainMenuItem = (currentMainMenuItem + 1) % totalMainMenuItems;
        displayMenuMain();
      } else if (data.b == -1) { // Move up
        currentMainMenuItem = (currentMainMenuItem - 1 + totalMainMenuItems) % totalMainMenuItems;
        displayMenuMain();
      }
    }
  }
  else if(appState == MENU_PLAYER_COUNT) {
    // Handle navigation in player count menu
    if (data.d) { // Select button pressed
      // Handle selection based on currentPlayerCountItem
      const char* playerCount = playerCountMenuItems[currentPlayerCountItem];
      Serial.print("Selected Player Count: ");
      Serial.println(playerCount);
      // Implement action based on selectedGame and playerCount
      appState = GAME_RUNNING;
      // Initialize game variables here if needed
      if (strcmp(selectedGame, "Pong") == 0) {
        // Reset game variables
        playerScore = 0;
        aiScore = 0;
        serve = true;
        playerServe = true;
        gameState = GAME_PLAYING;
        resetBall();
      }
      else if (strcmp(selectedGame, "Flappy Bird") == 0) {
        // Reset Flappy Bird game
        resetFlappyBirdGame(currentPlayerCountItem == 0);
      }
      else if (strcmp(selectedGame, "Snake") == 0) {
        // Reset Snake game
        resetSnakeGame(currentPlayerCountItem == 0);
      }
    } else {
      // Handle navigation
      if (data.b == 1) { // Move down
        currentPlayerCountItem = (currentPlayerCountItem + 1) % totalPlayerCountMenuItems;
        displayMenuPlayerCount();
      } else if (data.b == -1) { // Move up
        currentPlayerCountItem = (currentPlayerCountItem - 1 + totalPlayerCountMenuItems) % totalPlayerCountMenuItems;
        displayMenuPlayerCount();
      }
    }
  }
  else if (appState == GAME_RUNNING) {
    // Game is running, inputs are handled within the game functions
  }
}

// =========================
// Menu Display Functions
// =========================

// Utility function to estimate text width based on number of characters and text size
int calculateTextWidth(const char* text, int textSize) {
  int baseWidth = 6; // Approximate width per character at textSize 1
  return strlen(text) * baseWidth * textSize;
}

// Utility function to estimate text height based on text size
int calculateTextHeight(int textSize) {
  int baseHeight = 8; // Approximate height per character at textSize 1
  return baseHeight * textSize;
}

// Display Startup Screen
void displayStartupScreen() {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  int textSize = 3;
  gfx.setTextSize(textSize);

  // Text to display
  const char* startupText = "Scorpio";

  // Calculate text dimensions
  int textWidth = calculateTextWidth(startupText, textSize);
  int textHeight = calculateTextHeight(textSize);

  // Calculate cursor position to center the text
  int cursorX = (screenWidth - textWidth) / 2;
  int cursorY = (screenHeight - textHeight) / 2;

  // Set cursor and print text
  gfx.setCursor(cursorX, cursorY);
  gfx.println(startupText);

  startupStartTime = millis();
}

// Display Main Menu
void displayMenuMain() {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  int textSize = 2;
  gfx.setTextSize(textSize);

  // Header
  gfx.setCursor(10, 10);
  gfx.println("Select a Game:");

  // Menu Items
  for (int i = 0; i < totalMainMenuItems; i++) {
    if (i == currentMainMenuItem) {
      gfx.setTextColor(highlightColor);
    } else {
      gfx.setTextColor(whiteColor);
    }

    // Calculate cursor position
    int cursorX = 20;
    int cursorY = 50 + i * 30;

    gfx.setCursor(cursorX, cursorY);
    gfx.println(mainMenuItems[i]);
  }

  // Reset text color after drawing
  gfx.setTextColor(whiteColor);
}

// Display Player Count Selection Menu
void displayMenuPlayerCount() {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  int textSize = 2;
  gfx.setTextSize(textSize);

  // Header
  gfx.setCursor(10, 10);
  gfx.println("Select Mode:");

  // Menu Items
  for (int i = 0; i < totalPlayerCountMenuItems; i++) {
    if (i == currentPlayerCountItem) {
      gfx.setTextColor(highlightColor);
    } else {
      gfx.setTextColor(whiteColor);
    }

    // Calculate cursor position
    int cursorX = 20;
    int cursorY = 50 + i * 30;

    gfx.setCursor(cursorX, cursorY);
    gfx.println(playerCountMenuItems[i]);
  }

  // Reset text color after drawing
  gfx.setTextColor(whiteColor);
}

// =========================
// Game Logic Functions
// =========================

// Run the selected game
void runSelectedGame() {
  if (strcmp(selectedGame, "Pong") == 0) {
    bool isSinglePlayer = (currentPlayerCountItem == 0);
    runPong(isSinglePlayer);
  }
  else if (strcmp(selectedGame, "Flappy Bird") == 0) {
    bool isSinglePlayer = (currentPlayerCountItem == 0);
    runFlappyBird(isSinglePlayer);
  }
  else if (strcmp(selectedGame, "Snake") == 0) {
    bool isSinglePlayer = (currentPlayerCountItem == 0);
    runSnake(isSinglePlayer);
  }
  // Add other games here
}

// Pong Game Logic
void runPong(bool isSinglePlayer) {
  unsigned long currentMillis = millis();

  if (gameState == GAME_OVER) {
    displayWinMessage(playerScore >= maxScore ? "Player 1 Wins!" : isSinglePlayer ? "AI Wins!" : "Player 2 Wins!");
    if (controllerData[1].d) {  // Button press to return to menu
      delay(200);
      appState = MENU_MAIN;
      displayMenuMain();
    }
    return;
  }

  if (serve) {
    // Setup serve based on who serves
    setupServe(isSinglePlayer);
    return;
  }

  // Handle input for player 1 and player 2 or AI based on game mode
  handlePlayerInputs(isSinglePlayer);

  // Update collision cooldown
  if (collisionOccurred && (currentMillis - collisionCooldown) > 50) {
    collisionOccurred = false;
  }

  // Update the ball's position
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Update ball position
    ballX += ballSpeedX;
    ballY += ballSpeedY;

    // Check for collision with the top and bottom edges
    if ((ballY - ballRadius) <= 0 || (ballY + ballRadius) >= gfx.height()) {
      ballSpeedY = -ballSpeedY;
      ballY += ballSpeedY;
    }

    // Check paddle collisions
    checkPaddleCollisions();

    // Check for scoring
    if (ballX - ballRadius <= 0) {
      // Player 2 or AI scores
      aiScore++;
      checkWinCondition();
      if (gameState != GAME_OVER) {
        serve = true;
        playerServe = false;
        resetBall();
      }
    } else if (ballX + ballRadius >= gfx.width()) {
      // Player 1 scores
      playerScore++;
      checkWinCondition();
      if (gameState != GAME_OVER) {
        serve = true;
        playerServe = true;
        resetBall();
      }
    }

    // Draw the game
    drawGame();
  }
}

// Handle player inputs for both single-player and two-player mode
void handlePlayerInputs(bool isSinglePlayer) {
  // Controller 1: Player 1 Paddle
  if (controllerData[1].b == -1) {
    playerPaddleY -= playerSpeed;
    if (playerPaddleY < 0) playerPaddleY = 0;
  }
  if (controllerData[1].b == 1) {
    playerPaddleY += playerSpeed;
    if (playerPaddleY + paddleHeight > gfx.height()) playerPaddleY = gfx.height() - paddleHeight;
  }

  if (isSinglePlayer) {
    // AI Logic
    if (ballX > mode.hRes / 2 && !serve) {
      if (ballY < aiPaddleY + paddleHeight / 2) {
        aiPaddleY -= aiSpeed;
      } else if (ballY > aiPaddleY + paddleHeight / 2) {
        aiPaddleY += aiSpeed;
      }
    }
    if (aiPaddleY < 0) aiPaddleY = 0; // Boundary check
    if (aiPaddleY + paddleHeight > gfx.height()) aiPaddleY = gfx.height() - paddleHeight; // Boundary check
  } else {
    // Controller 2: Player 2 Paddle
    if (controllerMap.size() >= 2) {
      if (controllerData[2].b == -1) {
        aiPaddleY -= playerSpeed;
        if (aiPaddleY < 0) aiPaddleY = 0;
      }
      if (controllerData[2].b == 1) {
        aiPaddleY += playerSpeed;
        if (aiPaddleY + paddleHeight > gfx.height()) aiPaddleY = gfx.height() - paddleHeight;
      }
    }
  }
}

// Function to setup serve
void setupServe(bool isSinglePlayer) {
  // Clear screen
  gfx.fillScreen(blackColor);
  drawCenterLine();
  displayScores();
  // Draw paddles
  gfx.fillRect(playerPaddleX, playerPaddleY, paddleWidth, paddleHeight, paddleColor);
  gfx.fillRect(aiPaddleX, aiPaddleY, paddleWidth, paddleHeight, paddleColor);
  // Draw ball
  gfx.fillCircle(ballX, ballY, ballRadius, ballColor);
  // Display "Press Select to Serve"
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(1);
  gfx.setCursor(screenWidth / 2 - 50, screenHeight / 2 - 10);
  gfx.println("Press Select to Serve");
  vga.show();

  if (controllerData[1].d) {
    serve = false;
    float angle = (random(-30, 30)) * PI / 180.0;
    ballSpeedY = ballSpeed * sin(angle);
    ballSpeedX = playerServe ? ballSpeed * cos(angle) : -ballSpeed * cos(angle);
  }
}

// Function to update ball and check collisions
void updateBall() {
  // Ball position updated in runPong()
}

// Check for paddle collisions
void checkPaddleCollisions() {
  if (!collisionOccurred) {
    // Collision with AI paddle
    if ((ballX + ballRadius >= aiPaddleX) && (ballX + ballRadius <= aiPaddleX + paddleWidth) && (ballY >= aiPaddleY) && (ballY <= aiPaddleY + paddleHeight)) {
      handlePaddleCollision(aiPaddleX, aiPaddleY, false);
      collisionOccurred = true;
      collisionCooldown = millis();
    }

    // Collision with Player paddle
    if ((ballX - ballRadius <= playerPaddleX + paddleWidth) && (ballX - ballRadius >= playerPaddleX) && (ballY >= playerPaddleY) && (ballY <= playerPaddleY + paddleHeight)) {
      handlePaddleCollision(playerPaddleX, playerPaddleY, true);
      collisionOccurred = true;
      collisionCooldown = millis();
    }
  }
}

// Draw the game elements
void drawGame() {
  // Clear the previous frame
  gfx.fillScreen(blackColor);

  // Draw center line and scores
  drawCenterLine();
  displayScores();

  // Draw the ball at the updated position
  gfx.fillCircle(ballX, ballY, ballRadius, ballColor);

  // Draw the player paddle
  gfx.fillRect(playerPaddleX, playerPaddleY, paddleWidth, paddleHeight, paddleColor);

  // Draw the AI or Player 2 paddle
  gfx.fillRect(aiPaddleX, aiPaddleY, paddleWidth, paddleHeight, paddleColor);
}

// Function to Handle Paddle Collision and Adjust Ball Angle
void handlePaddleCollision(int paddleX, int paddleY, bool isPlayerPaddle) {
  // Calculate hit position relative to paddle center
  float hitPosition = (ballY - (paddleY + paddleHeight / 2)) / (paddleHeight / 2);

  // Clamp hitPosition to [-1, 1] to avoid extreme angles
  if (hitPosition < -1) hitPosition = -1;
  if (hitPosition > 1) hitPosition = 1;

  // Determine the angle based on hit position (-75 to +75 degrees)
  float maxAngle = 75.0; // Maximum angle for deflection
  float bounceAngle = hitPosition * maxAngle;

  // Convert angle to radians for calculation
  float bounceAngleRad = bounceAngle * PI / 180.0;

  // Update ball speeds based on angle, keeping the total speed constant
  ballSpeedY = ballSpeed * sin(bounceAngleRad);
  ballSpeedX = (isPlayerPaddle ? 1 : -1) * ballSpeed * cos(bounceAngleRad);

  // Increase ball speed slightly after each paddle hit
  ballSpeed += speedIncrement;

  // Move the ball away from the paddle slightly to prevent sticking
  ballX += (isPlayerPaddle ? 1 : -1) * (ballRadius + 2);
}

// Function to Reset the Ball Position
void resetBall() {
  ballX = mode.hRes / 2;
  ballY = mode.vRes / 2;
  ballSpeed = 3.0; // Reset speed
  collisionOccurred = false;
  playerPaddleY = mode.vRes / 2 - paddleHeight / 2;
  aiPaddleY = mode.vRes / 2 - paddleHeight / 2;
}

// Function to Draw Center Line
void drawCenterLine() {
  for (int i = 0; i < mode.vRes; i += 10) {
    gfx.drawFastVLine(mode.hRes / 2, i, 5, whiteColor);
  }
}

// Function to Display Scores
void displayScores() {
  gfx.setTextSize(1);
  gfx.setTextColor(whiteColor);

  // Player Score
  gfx.setCursor(mode.hRes / 2 - 60, 10);
  gfx.print("P1: ");
  gfx.print(playerScore);

  // AI or Player 2 Score
  gfx.setCursor(mode.hRes / 2 + 10, 10);
  if (controllerMap.size() >= 2 && currentPlayerCountItem == 1) {
    gfx.print("P2: ");
  } else {
    gfx.print("AI: ");
  }
  gfx.print(aiScore);
}

// Function to Check Win Condition
void checkWinCondition() {
  if (playerScore >= maxScore || aiScore >= maxScore) {
    gameState = GAME_OVER;
  }
}

// Function to Display Win Message
void displayWinMessage(const char* winner) {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(2);
  gfx.setCursor(mode.hRes / 2 - 60, mode.vRes / 2 - 20);
  gfx.print(winner);
  gfx.setTextSize(1);
  gfx.setCursor(mode.hRes / 2 - 40, mode.vRes / 2 + 10);
  gfx.print("Press Select");
  gfx.setCursor(mode.hRes / 2 - 20, mode.vRes / 2 + 20);
  gfx.print("to return");
  vga.show();
}

// =========================
// Flappy Bird Game Functions
// =========================

void resetFlappyBirdGame(bool isSinglePlayer) {
  fbIsSinglePlayer = isSinglePlayer;
  fbGameInitialized = true;
  fbLastFrameTime = millis();

  fbCollision = false;

  fbPipeSpeed = 2;
  fbGravity = 0.3f;
  fbJumpStrength = -6.0f;
  fbPipeGapSize = 60;

  if (fbIsSinglePlayer) {
    // Single-player initialization
    fbBirdX = fbScreenWidth / 4;
    fbBirdY = fbScreenHeight / 2;
    fbBirdSpeedY = 0;

    fbPipeX = fbScreenWidth;
    fbPipeGapY = random(fbScreenHeight / 4, 3 * fbScreenHeight / 4);

    fbScore = 0;
  } else {
    // Two-player initialization
    fbBirdX = fbScreenWidth / 4;

    fbBirdY1 = fbScreenHeight / 4;
    fbBirdSpeedY1 = 0;
    fbPipeX1 = fbScreenWidth;
    fbPipeGapY1 = random(fbPipeGapSize / 2, fbScreenHeight / 2 - fbPipeGapSize / 2);

    fbBirdY2 = 3 * fbScreenHeight / 4;
    fbBirdSpeedY2 = 0;
    fbPipeX2 = fbScreenWidth;
    fbPipeGapY2 = fbPipeGapY1 + fbScreenHeight / 2; // To keep pipes aligned

    fbPlayer1Alive = true;
    fbPlayer2Alive = true;

    fbScore1 = 0;
    fbScore2 = 0;
  }
}

void runFlappyBird(bool isSinglePlayer) {
  if (!fbGameInitialized) {
    resetFlappyBirdGame(isSinglePlayer);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - fbLastFrameTime >= fbFrameInterval) {
    fbLastFrameTime = currentMillis;

    if (fbIsSinglePlayer) {
      runFlappyBirdSinglePlayer();
    } else {
      runFlappyBirdTwoPlayer();
    }
  }
}

void runFlappyBirdSinglePlayer() {
  // Process input
  if (controllerData[1].d) {
    fbBirdSpeedY = fbJumpStrength;
  }

  // Update bird position
  fbBirdSpeedY += fbGravity;
  fbBirdY += fbBirdSpeedY;

  // Update pipe
  fbPipeX -= fbPipeSpeed;
  if (fbPipeX < -50) {
    fbPipeX = fbScreenWidth;
    fbPipeGapY = random(fbPipeGapSize / 2, fbScreenHeight - fbPipeGapSize / 2);
    fbScore++;
  }

  // Check collisions
  handleFlappyBirdCollisionsSinglePlayer();

  // Draw game
  drawFlappyBirdGameSinglePlayer();

  if (fbCollision) {
    // Game over
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(2);
    gfx.setCursor(fbScreenWidth / 2 - 60, fbScreenHeight / 2 - 20);
    gfx.print("Game Over!");
    gfx.setTextSize(1);
    gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 + 10);
    gfx.print("Press Select to return");
    if (controllerData[1].d) {
      // Return to main menu
      appState = MENU_MAIN;
      displayMenuMain();
      fbGameInitialized = false;
    }
  }
}

void runFlappyBirdTwoPlayer() {
  // Process input for Player 1
  if (fbPlayer1Alive && controllerData[1].d) {
    fbBirdSpeedY1 = fbJumpStrength;
  }
  // Process input for Player 2
  if (fbPlayer2Alive && controllerData[2].d) {
    fbBirdSpeedY2 = fbJumpStrength;
  }

  // Update bird positions
  if (fbPlayer1Alive) {
    fbBirdSpeedY1 += fbGravity;
    fbBirdY1 += fbBirdSpeedY1;
  }
  if (fbPlayer2Alive) {
    fbBirdSpeedY2 += fbGravity;
    fbBirdY2 += fbBirdSpeedY2;
  }

  // Update pipes
  fbPipeX1 -= fbPipeSpeed;
  fbPipeX2 -= fbPipeSpeed;

  if (fbPipeX1 < -50) {
    fbPipeX1 = fbScreenWidth;
    fbPipeGapY1 = random(fbPipeGapSize / 2, fbScreenHeight / 2 - fbPipeGapSize / 2);
    fbScore1++;
  }
  if (fbPipeX2 < -50) {
    fbPipeX2 = fbScreenWidth;
    fbPipeGapY2 = random(fbScreenHeight / 2 + fbPipeGapSize / 2, fbScreenHeight - fbPipeGapSize / 2);
    fbScore2++;
  }

  // Check collisions
  handleFlappyBirdCollisionsTwoPlayer();

  // Draw game
  drawFlappyBirdGameTwoPlayer();

  // Check for game over
  if (!fbPlayer1Alive || !fbPlayer2Alive) {
    // Game over
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(2);
    if (!fbPlayer1Alive && fbPlayer2Alive) {
      gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 - 20);
      gfx.print("Player 2 Wins!");
    } else if (fbPlayer1Alive && !fbPlayer2Alive) {
      gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 - 20);
      gfx.print("Player 1 Wins!");
    } else {
      gfx.setCursor(fbScreenWidth / 2 - 60, fbScreenHeight / 2 - 20);
      gfx.print("It's a Tie!");
    }
    gfx.setTextSize(1);
    gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 + 10);
    gfx.print("Press Select to return");
    if (controllerData[1].d || controllerData[2].d) {
      appState = MENU_MAIN;
      displayMenuMain();
      fbGameInitialized = false;
    }
  }
}

void handleFlappyBirdCollisionsSinglePlayer() {
  // Check collision with ground or ceiling
  if (fbBirdY > fbScreenHeight || fbBirdY < 0) {
    fbCollision = true;
  }
  // Check collision with pipes
  if (fbBirdX + 10 > fbPipeX && fbBirdX - 10 < fbPipeX + 50) {
    if (fbBirdY - 10 < fbPipeGapY - fbPipeGapSize / 2 || fbBirdY + 10 > fbPipeGapY + fbPipeGapSize / 2) {
      fbCollision = true;
    }
  }
}

void handleFlappyBirdCollisionsTwoPlayer() {
  // Player 1
  if (fbPlayer1Alive) {
    if (fbBirdY1 > fbScreenHeight / 2 || fbBirdY1 < 0) {
      fbPlayer1Alive = false;
    }
    if (fbBirdX + 10 > fbPipeX1 && fbBirdX - 10 < fbPipeX1 + 50) {
      if (fbBirdY1 - 10 < fbPipeGapY1 - fbPipeGapSize / 2 || fbBirdY1 + 10 > fbPipeGapY1 + fbPipeGapSize / 2) {
        fbPlayer1Alive = false;
      }
    }
  }
  // Player 2
  if (fbPlayer2Alive) {
    if (fbBirdY2 > fbScreenHeight || fbBirdY2 < fbScreenHeight / 2) {
      fbPlayer2Alive = false;
    }
    if (fbBirdX + 10 > fbPipeX2 && fbBirdX - 10 < fbPipeX2 + 50) {
      if (fbBirdY2 - 10 < fbPipeGapY2 - fbPipeGapSize / 2 || fbBirdY2 + 10 > fbPipeGapY2 + fbPipeGapSize / 2) {
        fbPlayer2Alive = false;
      }
    }
  }
}

void drawFlappyBirdGameSinglePlayer() {
  gfx.fillScreen(blackColor);

  // Draw bird
  gfx.fillCircle((int)fbBirdX, (int)fbBirdY, 10, whiteColor);

  // Draw pipes
  gfx.fillRect(fbPipeX, 0, 50, fbPipeGapY - fbPipeGapSize / 2, greenColor);
  gfx.fillRect(fbPipeX, fbPipeGapY + fbPipeGapSize / 2, 50, fbScreenHeight - fbPipeGapY - fbPipeGapSize / 2, greenColor);

  // Draw score
  gfx.setTextColor(whiteColor);
  gfx.setCursor(10, 10);
  gfx.print("Score: ");
  gfx.print(fbScore);
}

void drawFlappyBirdGameTwoPlayer() {
  gfx.fillScreen(blackColor);

  // Draw dividing line
  gfx.drawFastHLine(0, fbScreenHeight / 2, fbScreenWidth, whiteColor);

  // Player 1 area (top half)
  if (fbPlayer1Alive) {
    // Draw bird
    gfx.fillCircle((int)fbBirdX, (int)fbBirdY1, 10, redColor);
    // Draw pipes
    gfx.fillRect(fbPipeX1, 0, 50, fbPipeGapY1 - fbPipeGapSize / 2, greenColor);
    gfx.fillRect(fbPipeX1, fbPipeGapY1 + fbPipeGapSize / 2, 50, fbScreenHeight / 2 - fbPipeGapY1 - fbPipeGapSize / 2, greenColor);
    // Draw score
    gfx.setTextColor(whiteColor);
    gfx.setCursor(10, 10);
    gfx.print("P1 Score: ");
    gfx.print(fbScore1);
  }

  // Player 2 area (bottom half)
  if (fbPlayer2Alive) {
    // Draw bird
    gfx.fillCircle((int)fbBirdX, (int)fbBirdY2, 10, blueColor);
    // Draw pipes
    gfx.fillRect(fbPipeX2, fbScreenHeight / 2, 50, fbPipeGapY2 - fbPipeGapSize / 2 - fbScreenHeight / 2, greenColor);
    gfx.fillRect(fbPipeX2, fbPipeGapY2 + fbPipeGapSize / 2, 50, fbScreenHeight - fbPipeGapY2 - fbPipeGapSize / 2, greenColor);
    // Draw score
    gfx.setTextColor(whiteColor);
    gfx.setCursor(10, fbScreenHeight / 2 + 10);
    gfx.print("P2 Score: ");
    gfx.print(fbScore2);
  }
}

// =========================
// Snake Game Functions
// =========================

void resetSnakeGame(bool isSinglePlayer) {
  snakeGameInitialized = true;
  snakePreviousMillis = millis();
  snakePlayer1Alive = true;
  snakePlayer2Alive = true;

  if (isSinglePlayer) {
    // Single-player initialization
    snake.clear();
    Point head = { gridWidth / 2, gridHeight / 2 };
    snake.push_back(head);
    snakeDirection = 0;  // Start moving up
    snakeScore = 0;
    // Spawn food
    spawnFoodSinglePlayer();
  } else {
    // Two-player initialization
    // Player 1
    snake1.clear();
    Point head1 = { gridWidth / 4, gridHeight / 4 };
    snake1.push_back(head1);
    snakeDirection1 = 0;
    snakeScore1 = 0;
    // Player 2
    snake2.clear();
    Point head2 = { 3 * gridWidth / 4, 3 * gridHeight / 4 };
    snake2.push_back(head2);
    snakeDirection2 = 2;
    snakeScore2 = 0;
    // Spawn food for both players
    spawnFoodTwoPlayer();
  }
}

void runSnake(bool isSinglePlayer) {
  if (!snakeGameInitialized) {
    resetSnakeGame(isSinglePlayer);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - snakePreviousMillis >= snakeInterval) {
    snakePreviousMillis = currentMillis;
    if (isSinglePlayer) {
      handleSnakeInputSinglePlayer();
      updateSnakeSinglePlayer();
      drawSnakeGameSinglePlayer();
    } else {
      handleSnakeInputTwoPlayer();
      updateSnakeTwoPlayer();
      drawSnakeGameTwoPlayer();
    }
  }
}

void handleSnakeInputSinglePlayer() {
  // Controller 1: Player 1 Snake
  if (controllerData[1].b == -1) {
    // Turn left
    snakeDirection = (snakeDirection + 3) % 4;
  }
  if (controllerData[1].b == 1) {
    // Turn right
    snakeDirection = (snakeDirection + 1) % 4;
  }
}

void handleSnakeInputTwoPlayer() {
  // Controller 1: Player 1 Snake
  if (controllerData[1].b == -1) {
    // Turn left
    snakeDirection1 = (snakeDirection1 + 3) % 4;
  }
  if (controllerData[1].b == 1) {
    // Turn right
    snakeDirection1 = (snakeDirection1 + 1) % 4;
  }

  // Controller 2: Player 2 Snake
  if (controllerData[2].b == -1) {
    // Turn left
    snakeDirection2 = (snakeDirection2 + 3) % 4;
  }
  if (controllerData[2].b == 1) {
    // Turn right
    snakeDirection2 = (snakeDirection2 + 1) % 4;
  }
}

void updateSnakeSinglePlayer() {
  if (!snakePlayer1Alive) {
    displaySnakeGameOver("Game Over!");
    if (controllerData[1].d) {
      appState = MENU_MAIN;
      displayMenuMain();
      snakeGameInitialized = false;
    }
    return;
  }

  // Calculate new head position
  Point newHead = snake.front();
  switch (snakeDirection) {
    case 0: newHead.y--; break;  // Up
    case 1: newHead.x++; break;  // Right
    case 2: newHead.y++; break;  // Down
    case 3: newHead.x--; break;  // Left
  }

  // Check for collisions with walls
  if (newHead.x < 0 || newHead.x >= gridWidth || newHead.y < 0 || newHead.y >= gridHeight) {
    snakePlayer1Alive = false;
    return;
  }

  // Check for collisions with self
  for (auto segment : snake) {
    if (segment.x == newHead.x && segment.y == newHead.y) {
      snakePlayer1Alive = false;
      return;
    }
  }

  // Insert new head
  snake.insert(snake.begin(), newHead);

  // Check if food is eaten
  if (newHead.x == food.x && newHead.y == food.y) {
    snakeScore++;
    spawnFoodSinglePlayer();
  } else {
    // Remove tail segment
    snake.pop_back();
  }
}

void updateSnakeTwoPlayer() {
  if (!snakePlayer1Alive || !snakePlayer2Alive) {
    // Game over
    if (!snakePlayer1Alive && snakePlayer2Alive) {
      displaySnakeGameOver("Player 2 Wins!");
    } else if (snakePlayer1Alive && !snakePlayer2Alive) {
      displaySnakeGameOver("Player 1 Wins!");
    } else {
      displaySnakeGameOver("It's a Tie!");
    }
    if (controllerData[1].d || controllerData[2].d) {
      appState = MENU_MAIN;
      displayMenuMain();
      snakeGameInitialized = false;
    }
    return;
  }

  // Update Player 1
  Point newHead1 = snake1.front();
  switch (snakeDirection1) {
    case 0: newHead1.y--; break;  // Up
    case 1: newHead1.x++; break;  // Right
    case 2: newHead1.y++; break;  // Down
    case 3: newHead1.x--; break;  // Left
  }

  // Check for collisions with walls
  if (newHead1.x < 0 || newHead1.x >= gridWidth || newHead1.y < 0 || newHead1.y >= gridHeight) {
    snakePlayer1Alive = false;
  }

  // Check for collisions with self
  for (auto segment : snake1) {
    if (segment.x == newHead1.x && segment.y == newHead1.y) {
      snakePlayer1Alive = false;
    }
  }

  // Check for collisions with other player
  for (auto segment : snake2) {
    if (segment.x == newHead1.x && segment.y == newHead1.y) {
      snakePlayer1Alive = false;
    }
  }

  if (snakePlayer1Alive) {
    // Insert new head
    snake1.insert(snake1.begin(), newHead1);

    // Check if food is eaten
    if (newHead1.x == food1.x && newHead1.y == food1.y) {
      snakeScore1++;
      spawnFoodTwoPlayer();
    } else {
      // Remove tail segment
      snake1.pop_back();
    }
  }

  // Update Player 2
  Point newHead2 = snake2.front();
  switch (snakeDirection2) {
    case 0: newHead2.y--; break;  // Up
    case 1: newHead2.x++; break;  // Right
    case 2: newHead2.y++; break;  // Down
    case 3: newHead2.x--; break;  // Left
  }

  // Check for collisions with walls
  if (newHead2.x < 0 || newHead2.x >= gridWidth || newHead2.y < 0 || newHead2.y >= gridHeight) {
    snakePlayer2Alive = false;
  }

  // Check for collisions with self
  for (auto segment : snake2) {
    if (segment.x == newHead2.x && segment.y == newHead2.y) {
      snakePlayer2Alive = false;
    }
  }

  // Check for collisions with other player
  for (auto segment : snake1) {
    if (segment.x == newHead2.x && segment.y == newHead2.y) {
      snakePlayer2Alive = false;
    }
  }

  if (snakePlayer2Alive) {
    // Insert new head
    snake2.insert(snake2.begin(), newHead2);

    // Check if food is eaten
    if (newHead2.x == food2.x && newHead2.y == food2.y) {
      snakeScore2++;
      spawnFoodTwoPlayer();
    } else {
      // Remove tail segment
      snake2.pop_back();
    }
  }
}

void drawSnakeGameSinglePlayer() {
  gfx.fillScreen(blackColor);

  // Draw the food
  gfx.fillRect(food.x * gridSize, food.y * gridSize, gridSize, gridSize, redColor);

  // Draw the snake
  for (auto segment : snake) {
    gfx.fillRect(segment.x * gridSize, segment.y * gridSize, gridSize, gridSize, greenColor);
  }

  // Draw the score
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(1);
  gfx.setCursor(5, 5);
  gfx.print("Score: ");
  gfx.print(snakeScore);
}

void drawSnakeGameTwoPlayer() {
  gfx.fillScreen(blackColor);

  // Draw dividing line
  gfx.drawFastHLine(0, screenHeight / 2, screenWidth, whiteColor);

  // Player 1 area (top half)
  if (snakePlayer1Alive) {
    // Draw the food
    gfx.fillRect(food1.x * gridSize, food1.y * gridSize, gridSize, gridSize, redColor);

    // Draw the snake
    for (auto segment : snake1) {
      gfx.fillRect(segment.x * gridSize, segment.y * gridSize, gridSize, gridSize, greenColor);
    }

    // Draw the score
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(1);
    gfx.setCursor(5, 5);
    gfx.print("P1 Score: ");
    gfx.print(snakeScore1);
  }

  // Player 2 area (bottom half)
  if (snakePlayer2Alive) {
    // Adjust coordinates for bottom half
    int yOffset = screenHeight / 2;

    // Draw the food
    gfx.fillRect(food2.x * gridSize, food2.y * gridSize + yOffset, gridSize, gridSize, redColor);

    // Draw the snake
    for (auto segment : snake2) {
      gfx.fillRect(segment.x * gridSize, segment.y * gridSize + yOffset, gridSize, gridSize, blueColor);
    }

    // Draw the score
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(1);
    gfx.setCursor(5, screenHeight / 2 + 5);
    gfx.print("P2 Score: ");
    gfx.print(snakeScore2);
  }
}

void displaySnakeGameOver(const char* message) {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(2);
  gfx.setCursor(screenWidth / 2 - 80, screenHeight / 2 - 20);
  gfx.print(message);
  gfx.setTextSize(1);
  gfx.setCursor(screenWidth / 2 - 80, screenHeight / 2 + 10);
  gfx.print("Press Select to return");
}

void spawnFoodSinglePlayer() {
  bool validPosition = false;
  while (!validPosition) {
    food.x = random(0, gridWidth);
    food.y = random(0, gridHeight);

    // Ensure food is not on the snake
    validPosition = true;
    for (auto segment : snake) {
      if (segment.x == food.x && segment.y == food.y) {
        validPosition = false;
        break;
      }
    }
  }
}

void spawnFoodTwoPlayer() {
  // Food for Player 1
  bool validPosition1 = false;
  while (!validPosition1) {
    food1.x = random(0, gridWidth);
    food1.y = random(0, gridHeight / 2);

    // Ensure food is not on the snake
    validPosition1 = true;
    for (auto segment : snake1) {
      if (segment.x == food1.x && segment.y == food1.y) {
        validPosition1 = false;
        break;
      }
    }
  }

  // Food for Player 2
  bool validPosition2 = false;
  while (!validPosition2) {
    food2.x = random(0, gridWidth);
    food2.y = random(gridHeight / 2, gridHeight);

    // Ensure food is not on the snake
    validPosition2 = true;
    for (auto segment : snake2) {
      if (segment.x == food2.x && segment.y == food2.y) {
        validPosition2 = false;
        break;
      }
    }
  }
}

// =========================
// Utility Functions
// =========================

// Clear the screen (optional utility)
void clearScreen() {
  gfx.fillScreen(blackColor);
}
