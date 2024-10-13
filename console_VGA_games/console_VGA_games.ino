#include "ESP32S3VGA.h"
#include "GfxWrapper.h"
#include <esp_now.h>
#include <WiFi.h>
#include <map>
#include <string.h> // For strcmp
#include <math.h>   // For math functions

// Joystick Command Definitions
#define COMMAND_NO     0x00
#define COMMAND_LEFT   0x01
#define COMMAND_RIGHT  0x02
#define COMMAND_UP     0x04
#define COMMAND_DOWN   0x08

// =========================
// VGA Configuration
// =========================

// VGA Pin Configuration
const PinConfig pins(-1, -1, -1, -1, 18, -1, -1, -1, -1, -1, 43, -1, -1, -1, -1, 44, 21, 16);

// VGA Device and Mode Setup
VGA vga;
Mode mode = Mode::MODE_320x240x60; // 320x240 resolution at 60Hz
GfxWrapper<VGA> gfx(vga, mode.hRes, mode.vRes);

// =========================
// ESP-NOW Configuration
// =========================

// Structure to receive data from the controller
typedef struct struct_message {
  char a[32];       // Identifier (e.g., unique ID for the controller)
  int b;        // Button for moving between menu items
  float c;          // Movement input or placeholder
  bool d;        // Button for selecting the current menu item or action
  int command;  // Joystick command
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

const char* mainMenuItems[] = {"Pong", "Snake", "Flappy Bird", "Doom"};
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

// 3D Game Functions
void run3DGame();
void initializeMonsters();
void updateBullets();
void renderMonsters(float ZBuffer[]);
void renderBullets();
bool checkCollisionWithMonsters(float x, float y);
void drawFrame3D();
void handle3DGameInputs();
void display3DGameOverScreen();

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
// 3D Game Variables
// =========================

// Map Configuration for 3D Game
const int mapWidth3D = 16;
const int mapHeight3D = 16;

const char worldMap3D[] =
    "################"
    "#..............#"
    "#..............#"
    "#...##.........#"
    "#..............#"
    "#..............#"
    "#..............#"
    "#......###.....#"
    "#..............#"
    "#..............#"
    "#..............#"
    "#..............#"
    "#......##......#"
    "#..............#"
    "#..............#"
    "################";

// Player Properties
float posX = 8.0f, posY = 8.0f;   // Player position
float dirX = -1.0f, dirY = 0.0f;  // Initial direction vector
float planeX = 0.0f, planeY = 0.66f; // 2D raycaster version of camera plane

// Timing for 3D Game
unsigned long previousMillis3D = 0;
const long interval3D = 16;  // Frame update interval (~60 FPS)

// Monster and Bullet Structures
struct Monster {
    float x;
    float y;
    bool alive;
};

struct Bullet {
    float x;
    float y;
    float dirX;
    float dirY;
    bool active;
};

const int maxMonsters = 10;
Monster monsters[maxMonsters];

const int maxBullets = 5;
Bullet bullets[maxBullets];

// Game State for 3D Game
enum GameState3D { GAME_PLAYING_3D, GAME_OVER_3D };
GameState3D gameState3D = GAME_PLAYING_3D;

// =========================
// Setup Function
// =========================

void setup() {
  // Initialize Serial for Debugging
  Serial.begin(115200);
  Serial.println("VGA Menu System with ESP-NOW Controller");
  Serial.print("Size of struct_message: ");
  Serial.println(sizeof(struct_message));  // Should print 49

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
      } else {
        // Keep displaying the startup screen
        displayStartupScreen();
      }
      break;

    case MENU_MAIN:
      // Continuously display the main menu to prevent flickering
      displayMenuMain();
      break;

    case MENU_PLAYER_COUNT:
      // Continuously display the player count menu to prevent flickering
      displayMenuPlayerCount();
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
    Serial.print("Received data of incorrect size: ");
    Serial.println(len);
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
          strcmp(selectedGame, "Flappy Bird") == 0 ||
          strcmp(selectedGame, "Doom") == 0) {
        appState = MENU_PLAYER_COUNT;
        currentPlayerCountItem = 0; // Reset player count selection
      }
    } else {
      // Handle navigation
      if (data.b == 1) { // Move down
        currentMainMenuItem = (currentMainMenuItem + 1) % totalMainMenuItems;
      } else if (data.b == -1) { // Move up
        currentMainMenuItem = (currentMainMenuItem - 1 + totalMainMenuItems) % totalMainMenuItems;
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
      else if (strcmp(selectedGame, "Doom") == 0) {
        // Initialize 3D game variables
        posX = 8.0f;
        posY = 8.0f;
        dirX = -1.0f;
        dirY = 0.0f;
        planeX = 0.0f;
        planeY = 0.66f;
        initializeMonsters();
        gameState3D = GAME_PLAYING_3D;
      }
    } else {
      // Handle navigation
      if (data.b == 1) { // Move down
        currentPlayerCountItem = (currentPlayerCountItem + 1) % totalPlayerCountMenuItems;
      } else if (data.b == -1) { // Move up
        currentPlayerCountItem = (currentPlayerCountItem - 1 + totalPlayerCountMenuItems) % totalPlayerCountMenuItems;
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
  else if (strcmp(selectedGame, "Doom") == 0) {
    run3DGame();
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
  if (controllerData[1].b) { // Use Button 2 to jump
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
// 3D Game Functions
// =========================

// (3D game functions remain the same as previously provided)

void run3DGame() {
  unsigned long currentMillis = millis();

  if (gameState3D == GAME_OVER_3D) {
    display3DGameOverScreen();
    if (controllerData[1].d) {  // Use Select button to return to menu
      delay(200);
      appState = MENU_MAIN;
      displayMenuMain();
      return;
    }
  } else {
    // Handle inputs
    handle3DGameInputs();

    // Update bullets
    updateBullets();

    // Update and draw frame at regular intervals
    if (currentMillis - previousMillis3D >= interval3D) {
      previousMillis3D = currentMillis;
      drawFrame3D();
    }
  }
}

// 3D Game Functions
void handle3DGameInputs() {
  float moveSpeed = 0.05f; // Movement speed
  float rotSpeed = 0.05f;  // Rotation speed

  // Movement Forward/Backward
  if (controllerData[1].command & COMMAND_UP) { // Move forward
    float newPosX = posX + dirX * moveSpeed;
    float newPosY = posY + dirY * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.' &&
        !checkCollisionWithMonsters(newPosX, newPosY)) {
      posX = newPosX;
      posY = newPosY;
    }
  }
  if (controllerData[1].command & COMMAND_DOWN) { // Move backward
    float newPosX = posX - dirX * moveSpeed;
    float newPosY = posY - dirY * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.' &&
        !checkCollisionWithMonsters(newPosX, newPosY)) {
      posX = newPosX;
      posY = newPosY;
    }
  }

  // Strafing Left/Right
  if (controllerData[1].command & COMMAND_LEFT) { // Strafe left
    float newPosX = posX - dirY * moveSpeed;
    float newPosY = posY + dirX * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.' &&
        !checkCollisionWithMonsters(newPosX, newPosY)) {
      posX = newPosX;
      posY = newPosY;
    }
  }
  if (controllerData[1].command & COMMAND_RIGHT) { // Strafe right
    float newPosX = posX + dirY * moveSpeed;
    float newPosY = posY - dirX * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.' &&
        !checkCollisionWithMonsters(newPosX, newPosY)) {
      posX = newPosX;
      posY = newPosY;
    }
  }

  // Rotation with `b`
  if (controllerData[1].b == -1) { // Rotate left
    float oldDirX = dirX;
    dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
    dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
    float oldPlaneX = planeX;
    planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
    planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
  }
  if (controllerData[1].b == 1) { // Rotate right
    float oldDirX = dirX;
    dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
    dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
    float oldPlaneX = planeX;
    planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
    planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
  }

  // Shooting with `d`
  static bool canShoot = true;
  if (controllerData[1].d && canShoot) { // Use `d` (select button) for shooting
    for (int i = 0; i < maxBullets; i++) {
      if (!bullets[i].active) {
        bullets[i].x = posX;
        bullets[i].y = posY;
        bullets[i].dirX = dirX;
        bullets[i].dirY = dirY;
        bullets[i].active = true;
        canShoot = false; // Prevent continuous shooting
        break;
      }
    }
  }
  if (!controllerData[1].d) {
    canShoot = true;
  }
}



  // Exit to main menu (e.g., by pressing a specific button combination)
  // Implement exit logic if needed

// Function to Draw Each Frame of the 3D Game
void drawFrame3D() {
  // Clear the screen
  gfx.fillScreen(0);

  float ZBuffer[mode.hRes]; // Z-buffer to store wall distances

  // Raycasting loop for each vertical stripe
  for (int x = 0; x < mode.hRes; x++) {
    // Calculate ray position and direction
    float cameraX = 2 * x / float(mode.hRes) - 1; // x-coordinate in camera space
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    // Map position
    int mapX = int(posX);
    int mapY = int(posY);

    // Length of ray from current position to next x or y side
    float sideDistX;
    float sideDistY;

    // Length of ray from one x or y side to next x or y side
    float deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1 / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1 / rayDirY);
    float perpWallDist;

    // Direction to go in x and y (+1 or -1)
    int stepX;
    int stepY;

    int hit = 0; // Was there a wall hit?
    int side;    // Was a NS or EW wall hit?

    // Calculate step and initial sideDist
    if (rayDirX < 0) {
      stepX = -1;
      sideDistX = (posX - mapX) * deltaDistX;
    } else {
      stepX = 1;
      sideDistX = (mapX + 1.0 - posX) * deltaDistX;
    }
    if (rayDirY < 0) {
      stepY = -1;
      sideDistY = (posY - mapY) * deltaDistY;
    } else {
      stepY = 1;
      sideDistY = (mapY + 1.0 - posY) * deltaDistY;
    }

    // Perform DDA
    while (hit == 0) {
      // Jump to next map square in x or y direction
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      // Check if ray has hit a wall
      if (worldMap3D[mapY * mapWidth3D + mapX] == '#') hit = 1;
    }

    // Calculate distance projected on camera direction
    if (side == 0)
      perpWallDist = (sideDistX - deltaDistX);
    else
      perpWallDist = (sideDistY - deltaDistY);

    // Calculate height of line to draw on screen
    int lineHeight = (int)(mode.vRes / perpWallDist);

    // Calculate lowest and highest pixel to fill in current stripe
    int drawStart = -lineHeight / 2 + mode.vRes / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + mode.vRes / 2;
    if (drawEnd >= mode.vRes) drawEnd = mode.vRes - 1;

    // Choose wall color
    uint16_t color = 0xF800; // Red walls

    // Give x and y sides different brightness
    if (side == 1) {
      color = color >> 1; // Darken color
    }

    // Draw the pixels of the stripe as a vertical line
    gfx.drawFastVLine(x, drawStart, drawEnd - drawStart, color);

    // Store the perpendicular wall distance for the Z-buffer
    ZBuffer[x] = perpWallDist;
  }

  // Render monsters and bullets
  renderMonsters(ZBuffer);
  renderBullets();
}

// Function to Display Game Over Screen for 3D Game
void display3DGameOverScreen() {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(2);
  gfx.setCursor(mode.hRes / 2 - 60, mode.vRes / 2 - 20);
  gfx.print("GAME OVER");
  gfx.setTextSize(1);
  gfx.setCursor(mode.hRes / 2 - 70, mode.vRes / 2 + 10);
  gfx.print("Press Select to return");
}

// Function to Initialize Monsters
void initializeMonsters() {
  // Clear all monsters
  for (int i = 0; i < maxMonsters; i++) {
    monsters[i].alive = false;
  }

  // Example positions for monsters
  monsters[0] = { 10.5f, 7.5f, true };
  monsters[1] = { 12.5f, 5.5f, true };
  monsters[2] = { 7.5f, 12.5f, true };
  // Add more monsters as needed
}

// Function to Update Bullets
void updateBullets() {
  float bulletSpeed = 0.1f;
  for (int i = 0; i < maxBullets; i++) {
    if (bullets[i].active) {
      bullets[i].x += bullets[i].dirX * bulletSpeed;
      bullets[i].y += bullets[i].dirY * bulletSpeed;

      // Check if bullet hits a wall
      if (worldMap3D[int(bullets[i].y) * mapWidth3D + int(bullets[i].x)] == '#') {
        bullets[i].active = false;
      }

      // Check for collision with monsters
      for (int j = 0; j < maxMonsters; j++) {
        if (monsters[j].alive) {
          float dx = bullets[i].x - monsters[j].x;
          float dy = bullets[i].y - monsters[j].y;
          float distance = sqrt(dx * dx + dy * dy);
          if (distance < 0.5f) { // Collision threshold
            monsters[j].alive = false;
            bullets[i].active = false;
            // Check if all monsters are defeated
            bool allDefeated = true;
            for (int k = 0; k < maxMonsters; k++) {
              if (monsters[k].alive) {
                allDefeated = false;
                break;
              }
            }
            if (allDefeated) {
              gameState3D = GAME_OVER_3D;
            }
            break;
          }
        }
      }
    }
  }
}

// Function to Check Collision with Monsters
bool checkCollisionWithMonsters(float x, float y) {
  for (int i = 0; i < maxMonsters; i++) {
    if (monsters[i].alive) {
      float dx = x - monsters[i].x;
      float dy = y - monsters[i].y;
      float distance = sqrt(dx * dx + dy * dy);
      if (distance < 0.5f) { // Collision threshold
        return true; // Collision detected
      }
    }
  }
  return false; // No collision
}

// Function to Render Monsters
void renderMonsters(float ZBuffer[]) {
  for (int i = 0; i < maxMonsters; i++) {
    if (monsters[i].alive) {
      // Translate sprite position relative to camera
      float spriteX = monsters[i].x - posX;
      float spriteY = monsters[i].y - posY;

      // Calculate inverse determinant for correct matrix multiplication
      float invDet = 1.0f / (planeX * dirY - dirX * planeY);

      // Transform sprite with the inverse camera matrix
      float transformX = invDet * (dirY * spriteX - dirX * spriteY);
      float transformY = invDet * (-planeY * spriteX + planeX * spriteY);

      // Check if sprite is in front of camera plane
      if (transformY <= 0) continue;

      // Calculate sprite screen position
      int spriteScreenX = int((mode.hRes / 2) * (1 + transformX / transformY));

      // Calculate sprite dimensions
      int spriteHeight = abs(int(mode.vRes / transformY));
      int spriteWidth = spriteHeight / 2; // For 1:1:2 ratio

      // Calculate drawing boundaries
      int drawStartY = -spriteHeight / 2 + mode.vRes / 2;
      int drawEndY = spriteHeight / 2 + mode.vRes / 2;
      int drawStartX = -spriteWidth / 2 + spriteScreenX;
      int drawEndX = spriteWidth / 2 + spriteScreenX;

      // Skip rendering if the sprite is completely off-screen
      if (drawEndX < 0 || drawStartX >= mode.hRes) continue;

      // Clamp drawing coordinates to screen boundaries
      if (drawStartY < 0) drawStartY = 0;
      if (drawEndY >= mode.vRes) drawEndY = mode.vRes - 1;
      if (drawStartX < 0) drawStartX = 0;
      if (drawEndX >= mode.hRes) drawEndX = mode.hRes - 1;

      // Loop through every vertical stripe of the sprite on screen
      for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
        if (transformY < ZBuffer[stripe]) {
          // Calculate the proportion of the stripe within the sprite width
          float texX = float(stripe - drawStartX) / float(drawEndX - drawStartX);

          // Determine color based on texX (simple shading)
          uint16_t color;
          if (texX < 0.25f || texX > 0.75f) {
            color = 0xF81F; // Pink side walls
          } else {
            color = 0x001F; // Blue front face
          }

          // Draw vertical line for the sprite
          gfx.drawFastVLine(stripe, drawStartY, drawEndY - drawStartY, color);

          // Optionally draw top
          if (stripe == (drawStartX + drawEndX) / 2) {
            gfx.drawPixel(stripe, drawStartY, 0xF81F); // Pink top pixel
          }
        }
      }
    }
  }
}

// Function to Render Bullets
void renderBullets() {
  for (int i = 0; i < maxBullets; i++) {
    if (bullets[i].active) {
      // Translate bullet position to relative to camera
      float bulletX = bullets[i].x - posX;
      float bulletY = bullets[i].y - posY;

      // Transform to camera space
      float invDet = 1.0f / (planeX * dirY - dirX * planeY);
      float transformX = invDet * (dirY * bulletX - dirX * bulletY);
      float transformY = invDet * (-planeY * bulletX + planeX * bulletY);

      if (transformY <= 0.1f) continue; // Bullet behind the player or too close

      int bulletScreenX = int((mode.hRes / 2) * (1 + transformX / transformY));

      // Bullet dimensions
      int bulletHeight = abs(int(mode.vRes / transformY)) / 4; // Smaller than monsters
      int drawStartY = -bulletHeight / 2 + mode.vRes / 2;
      if (drawStartY < 0) drawStartY = 0;
      int drawEndY = bulletHeight / 2 + mode.vRes / 2;
      if (drawEndY >= mode.vRes) drawEndY = mode.vRes - 1;

      int bulletWidth = bulletHeight;
      int drawStartX = -bulletWidth / 2 + bulletScreenX;
      if (drawStartX < 0) drawStartX = 0;
      int drawEndX = bulletWidth / 2 + bulletScreenX;
      if (drawEndX >= mode.hRes) drawEndX = mode.hRes - 1;

      // Draw the bullet
      for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
        if (stripe > 0 && stripe < mode.hRes) {
          // Draw vertical line for the bullet
          gfx.drawFastVLine(stripe, drawStartY, drawEndY - drawStartY, 0xFFE0); // Yellow color
        }
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
