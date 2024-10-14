#include "ESP32S3VGA.h"
#include "GfxWrapper.h"
#include <esp_now.h>
#include <WiFi.h>
#include <map>
#include <string.h> // For strcmp
#include <math.h>   // For math functions
#include <SD.h>     // Include the SD library
#include <SPI.h>    // Include the SPI library
#include <vector>
#include <stack>
#include <cstdlib>  // For random generation
#include <ctime>    // For seeding random generation


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
  MENU_LEADERBOARD,
  GAME_RUNNING,
  DISPLAY_LEADERBOARD,
  NAME_ENTRY
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

const char* playerCountMenuItems[] = {"Single Player", "Two Player", "Leaderboard"};
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
// SD Card Configuration
// =========================

#define SD_CS_PIN 10  // Chip Select pin for SD card

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
void drawMinimap(float playerX, float playerY, uint16_t playerColor, int offsetX, int offsetY);
void run3DGameTwoPlayer();
void handle3DGameInputsPlayer2();
void drawFrame3DTwoPlayer();
void renderOtherPlayerOnScreen(float posX, float posY, float posX2, float posY2, float ZBufferP1[], float ZBufferP2[]);



// Leaderboard Functions
void displayLeaderboard(const char* gameName);
void updateLeaderboard(const char* gameName, int score);
void enterPlayerName(char* playerName);
void displayKeyboard(char* playerName, int maxLength);

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

// Flag to check if high score was achieved
bool newHighScore = false;

// Player name
char playerName[4];  // 3 characters + null terminator

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

char worldMap3D[mapWidth3D * mapHeight3D];




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

// Flag to check if high score was achieved in Flappy Bird
bool fbNewHighScore = false;

// Minimap dimensions
const int minimapSize = 50; // Size of the minimap in pixels
const int minimapTileSize = minimapSize / mapWidth3D; // Size of each tile in the minimap

// Player 1 Properties
float posX = 8.0f, posY = 8.0f;   // Player 1 position
float dirX = -1.0f, dirY = 0.0f;  // Player 1 direction vector
float planeX = 0.0f, planeY = 0.66f; // Player 1 camera plane

// Player 2 Properties
float posX2 = 10.0f, posY2 = 10.0f;   // Player 2 position
float dirX2 = -1.0f, dirY2 = 0.0f;  // Player 2 direction vector
float planeX2 = 0.0f, planeY2 = 0.66f; // Player 2 camera plane

int player2Lives = 3;
int player2Health = 3;
bool twoPlayerMode = false;

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

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setCursor(10, 60);
    gfx.println("SD Card Init Failed");
    vga.show();
    while (1) delay(1); // Halt if SD card fails to initialize
  }

  // Display startup screen
  startupStartTime = millis();
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

    case DISPLAY_LEADERBOARD:
      displayLeaderboard(selectedGame);
      break;

    case NAME_ENTRY:
      // Handle name entry for high score
      enterPlayerName(playerName);
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

      // Transition to player count selection
      appState = MENU_PLAYER_COUNT;
      currentPlayerCountItem = 0; // Reset player count selection
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
      Serial.print("Selected Option: ");
      Serial.println(playerCount);

      if (strcmp(playerCount, "Leaderboard") == 0) {
        appState = DISPLAY_LEADERBOARD;
      } else {
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
          newHighScore = false;
        }
        else if (strcmp(selectedGame, "Flappy Bird") == 0) {
          // Reset Flappy Bird game
          resetFlappyBirdGame(currentPlayerCountItem == 0);
          fbNewHighScore = false;
        }
        else if (strcmp(selectedGame, "Doom") == 0) {
          // Initialize 3D game variables
          posX = 8.0f;
          posY = 8.0f;
          dirX = -1.0f;
          dirY = 0.0f;
          planeX = 0.0f;
          planeY = 0.66f;
          generateRandomMaze();
          initializeMonsters();
          gameState3D = GAME_PLAYING_3D;
        }
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
  else if (appState == DISPLAY_LEADERBOARD) {
    if (data.d) { // Press select to return
      appState = MENU_PLAYER_COUNT;
    }
  }
  else if (appState == NAME_ENTRY) {
    // Handle name entry inputs
    // Implementation will be in enterPlayerName function
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
  gfx.print("Options for ");
  gfx.println(selectedGame);

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
  else if (strcmp(selectedGame, "Snake") == 0) {
    bool isSinglePlayer = (currentPlayerCountItem == 0);
    runSnake(isSinglePlayer);
  }
  else if (strcmp(selectedGame, "Flappy Bird") == 0) {
    bool isSinglePlayer = (currentPlayerCountItem == 0);
    runFlappyBird(isSinglePlayer);
  }
  else if (strcmp(selectedGame, "Doom") == 0) {
    if (currentPlayerCountItem == 1) { // Two-player mode selected
      twoPlayerMode = true;
      run3DGameTwoPlayer();
    } else {
      twoPlayerMode = false;
      run3DGame();
    }
  }
}

// Pong Game Logic
void runPong(bool isSinglePlayer) {
  unsigned long currentMillis = millis();

  if (gameState == GAME_OVER) {
    displayWinMessage(playerScore >= maxScore ? "Player 1 Wins!" : isSinglePlayer ? "AI Wins!" : "Player 2 Wins!");
    if (!newHighScore && playerScore >= maxScore) {
      // Check for high score
      updateLeaderboard("Pong", playerScore);
      newHighScore = true;
    }
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

  if (playerScore >= maxScore && !newHighScore && currentPlayerCountItem == 0) {
    gfx.setTextSize(1);
    gfx.setCursor(mode.hRes / 2 - 50, mode.vRes / 2 + 10);
    gfx.print("New High Score!");
    gfx.setCursor(mode.hRes / 2 - 70, mode.vRes / 2 + 20);
    gfx.print("Press Select to Save");
    if (controllerData[1].d) {
      appState = NAME_ENTRY;
    }
  } else {
    gfx.setTextSize(1);
    gfx.setCursor(mode.hRes / 2 - 40, mode.vRes / 2 + 10);
    gfx.print("Press Select");
    gfx.setCursor(mode.hRes / 2 - 20, mode.vRes / 2 + 20);
    gfx.print("to return");
  }
  vga.show();
}

// =========================
// Game Variables (Snake)
// =========================

// Snake properties
struct SnakeSegment {
  int x;
  int y;
};
std::vector<SnakeSegment> snake;
int snakeDirection = COMMAND_RIGHT; // Initial direction
int snakeSpeed = 100; // Delay between moves in milliseconds
unsigned long lastMoveTime = 0;

// Food properties
int foodX = 0;
int foodY = 0;

// Game state
bool snakeGameInitialized = false;
bool snakeGameOver = false;
int snakeScore = 0;
bool newSnakeHighScore = false;

// Grid size
const int gridSize = 10; // Each grid cell is 10x10 pixels

// Colors for Snake game
uint16_t snakeColor = 0x07E0; // Green
uint16_t foodColor = 0xF800;  // Red

// =========================
// Snake Game Functions
// =========================

void runSnake(bool isSinglePlayer) {
  unsigned long currentMillis = millis();

  if (!snakeGameInitialized) {
    resetSnakeGame(isSinglePlayer);
  }

  if (snakeGameOver) {
    // Check for high score
    if (!newSnakeHighScore) {
      updateLeaderboard("Snake", snakeScore);
      newSnakeHighScore = true;
    }

    // Display Game Over Screen
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(2);
    gfx.setCursor(screenWidth / 2 - 60, screenHeight / 2 - 20);
    gfx.print("Game Over!");
    gfx.setTextSize(1);
    gfx.setCursor(screenWidth / 2 - 50, screenHeight / 2 + 10);
    gfx.print("Score: ");
    gfx.print(snakeScore);

    if (newSnakeHighScore) {
      gfx.setCursor(screenWidth / 2 - 70, screenHeight / 2 + 20);
      gfx.print("New High Score!");
      gfx.setCursor(screenWidth / 2 - 80, screenHeight / 2 + 30);
      gfx.print("Press Select to Save");
      if (controllerData[1].d) {
        appState = NAME_ENTRY;
      }
    } else {
      gfx.setCursor(screenWidth / 2 - 70, screenHeight / 2 + 20);
      gfx.print("Press Select to return");
      if (controllerData[1].d) {
        appState = MENU_MAIN;
        displayMenuMain();
        snakeGameInitialized = false;
      }
    }
    return;
  }

  if (currentMillis - lastMoveTime >= snakeSpeed) {
    lastMoveTime = currentMillis;
    handleSnakeInput();
    updateSnake();
    checkSnakeCollision();
    drawSnakeGame();
  }
}

void resetSnakeGame(bool isSinglePlayer) {
  snake.clear();
  SnakeSegment head;
  head.x = (screenWidth / 2 / gridSize) * gridSize;
  head.y = (screenHeight / 2 / gridSize) * gridSize;
  snake.push_back(head);

  snakeDirection = COMMAND_RIGHT;
  snakeGameOver = false;
  snakeGameInitialized = true;
  snakeScore = 0;
  newSnakeHighScore = false;

  generateFood();
}

void handleSnakeInput() {
  int newDirection = snakeDirection;
  if (controllerData[1].command & COMMAND_UP && snakeDirection != COMMAND_DOWN) {
    newDirection = COMMAND_UP;
  }
  else if (controllerData[1].command & COMMAND_DOWN && snakeDirection != COMMAND_UP) {
    newDirection = COMMAND_DOWN;
  }
  else if (controllerData[1].command & COMMAND_LEFT && snakeDirection != COMMAND_RIGHT) {
    newDirection = COMMAND_LEFT;
  }
  else if (controllerData[1].command & COMMAND_RIGHT && snakeDirection != COMMAND_LEFT) {
    newDirection = COMMAND_RIGHT;
  }
  snakeDirection = newDirection;
}

void updateSnake() {
  // Move the snake
  SnakeSegment newHead = snake.front();
  switch (snakeDirection) {
    case COMMAND_UP:
      newHead.y -= gridSize;
      break;
    case COMMAND_DOWN:
      newHead.y += gridSize;
      break;
    case COMMAND_LEFT:
      newHead.x -= gridSize;
      break;
    case COMMAND_RIGHT:
      newHead.x += gridSize;
      break;
  }

  // Add new head
  snake.insert(snake.begin(), newHead);

  // Check if food is eaten
  if (newHead.x == foodX && newHead.y == foodY) {
    snakeScore++;
    generateFood();
  } else {
    // Remove tail
    snake.pop_back();
  }
}

void checkSnakeCollision() {
  SnakeSegment head = snake.front();

  // Check wall collision
  if (head.x < 0 || head.x >= screenWidth || head.y < 0 || head.y >= screenHeight) {
    snakeGameOver = true;
    return;
  }

  // Check self collision
  for (size_t i = 1; i < snake.size(); i++) {
    if (head.x == snake[i].x && head.y == snake[i].y) {
      snakeGameOver = true;
      return;
    }
  }
}

void drawSnakeGame() {
  gfx.fillScreen(blackColor);

  // Draw food
  gfx.fillRect(foodX, foodY, gridSize, gridSize, foodColor);

  // Draw snake
  for (const auto& segment : snake) {
    gfx.fillRect(segment.x, segment.y, gridSize, gridSize, snakeColor);
  }

  // Draw score
  gfx.setTextColor(whiteColor);
  gfx.setCursor(10, 10);
  gfx.print("Score: ");
  gfx.print(snakeScore);
}

void generateFood() {
  foodX = random(screenWidth / gridSize) * gridSize;
  foodY = random(screenHeight / gridSize) * gridSize;

  // Ensure food does not appear on the snake
  for (const auto& segment : snake) {
    if (foodX == segment.x && foodY == segment.y) {
      generateFood();
      break;
    }
  }
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
  fbJumpStrength = -4.0f;
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
  if (controllerData[1].d) { // Use Select button to jump
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
    // Check for high score
    if (!fbNewHighScore) {
      updateLeaderboard("Flappy Bird", fbScore);
      fbNewHighScore = true;
    }

    // Game over
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(2);
    gfx.setCursor(fbScreenWidth / 2 - 60, fbScreenHeight / 2 - 20);
    gfx.print("Game Over!");
    if (fbNewHighScore) {
      gfx.setTextSize(1);
      gfx.setCursor(fbScreenWidth / 2 - 70, fbScreenHeight / 2 + 10);
      gfx.print("New High Score!");
      gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 + 20);
      gfx.print("Press Select to Save");
      if (controllerData[1].d) {
        appState = NAME_ENTRY;
      }
    } else {
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



// =========================
// Random Maze Generation
// =========================

void generateRandomMaze() {
  // Initialize the map with walls
  for (int y = 0; y < mapHeight3D; y++) {
    for (int x = 0; x < mapWidth3D; x++) {
      worldMap3D[y * mapWidth3D + x] = '#';
    }
  }

  // Use a depth-first search (DFS) approach to carve out a maze
  std::stack<std::pair<int, int>> stack;
  stack.push({1, 1});
  worldMap3D[1 * mapWidth3D + 1] = '.'; // Start position

  // Directions for movement: up, down, left, right
  const int dx[] = {0, 0, -2, 2};
  const int dy[] = {-2, 2, 0, 0};

  // Seed the random generator
  srand(time(0));

  while (!stack.empty()) {
    int x, y;
    std::tie(x, y) = stack.top();

    // Create a list of possible directions to move
    std::vector<int> directions = {0, 1, 2, 3};
    std::random_shuffle(directions.begin(), directions.end());

    bool moved = false;
    for (int i : directions) {
      int nx = x + dx[i];
      int ny = y + dy[i];

      if (nx > 0 && nx < mapWidth3D - 1 && ny > 0 && ny < mapHeight3D - 1 && worldMap3D[ny * mapWidth3D + nx] == '#') {
        // Carve a path
        worldMap3D[(y + ny) / 2 * mapWidth3D + (x + nx) / 2] = '.';
        worldMap3D[ny * mapWidth3D + nx] = '.';

        // Move to the next cell
        stack.push({nx, ny});
        moved = true;
        break;
      }
    }

    // If no move was possible, backtrack
    if (!moved) {
      stack.pop();
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

  // Rotation with b
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

  // Shooting with d
  static bool canShoot = true;
  if (controllerData[1].d && canShoot) { // Use d (select button) for shooting
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

// Function to draw the minimap
void drawMinimap(float playerX, float playerY, uint16_t playerColor, int offsetX, int offsetY) {
  // Position the minimap
  int minimapStartX = offsetX;
  int minimapStartY = offsetY;

  // Draw the map representation
  for (int y = 0; y < mapHeight3D; y++) {
    for (int x = 0; x < mapWidth3D; x++) {
      // Determine the color based on the map content
      uint16_t color = (worldMap3D[y * mapWidth3D + x] == '#') ? whiteColor : blackColor;
      gfx.fillRect(minimapStartX + x * minimapTileSize, minimapStartY + y * minimapTileSize, minimapTileSize, minimapTileSize, color);
    }
  }

  // Draw the player position
  int playerMinimapX = minimapStartX + (int)playerX * minimapTileSize;
  int playerMinimapY = minimapStartY + (int)playerY * minimapTileSize;
  gfx.fillCircle(playerMinimapX, playerMinimapY, minimapTileSize / 2, playerColor); // Draw the player as a small circle
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

  // Draw the minimap in the top right corner for player 1
  drawMinimap(posX, posY, redColor, mode.hRes - minimapSize - 10, 10);
}

void drawFrame3DTwoPlayer() {
  // Clear the screen
  gfx.fillScreen(0);

  // Draw Player 1 view
  float ZBuffer1[mode.hRes / 2]; // Z-buffer for Player 1
  for (int x = 0; x < mode.hRes / 2; x++) {
    float cameraX = 2 * x / float(mode.hRes / 2) - 1;
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    int mapX = int(posX);
    int mapY = int(posY);

    float sideDistX;
    float sideDistY;

    float deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1 / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1 / rayDirY);
    float perpWallDist;

    int stepX;
    int stepY;

    int hit = 0;
    int side;

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

    while (hit == 0) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      if (worldMap3D[mapY * mapWidth3D + mapX] == '#') hit = 1;
    }

    if (side == 0)
      perpWallDist = (sideDistX - deltaDistX);
    else
      perpWallDist = (sideDistY - deltaDistY);

    int lineHeight = (int)(mode.vRes / perpWallDist);
    int drawStart = -lineHeight / 2 + mode.vRes / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + mode.vRes / 2;
    if (drawEnd >= mode.vRes) drawEnd = mode.vRes - 1;

    uint16_t color = 0xF800;
    if (side == 1) {
      color = color >> 1;
    }

    gfx.drawFastVLine(x, drawStart, drawEnd - drawStart, color);
    ZBuffer1[x] = perpWallDist;
  }

  // Draw Player 2 view
  float ZBuffer2[mode.hRes / 2]; // Z-buffer for Player 2
  for (int x = 0; x < mode.hRes / 2; x++) {
    float cameraX = 2 * x / float(mode.hRes / 2) - 1;
    float rayDirX = dirX2 + planeX2 * cameraX;
    float rayDirY = dirY2 + planeY2 * cameraX;

    int mapX = int(posX2);
    int mapY = int(posY2);

    float sideDistX;
    float sideDistY;

    float deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1 / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1 / rayDirY);
    float perpWallDist;

    int stepX;
    int stepY;

    int hit = 0;
    int side;

    if (rayDirX < 0) {
      stepX = -1;
      sideDistX = (posX2 - mapX) * deltaDistX;
    } else {
      stepX = 1;
      sideDistX = (mapX + 1.0 - posX2) * deltaDistX;
    }
    if (rayDirY < 0) {
      stepY = -1;
      sideDistY = (posY2 - mapY) * deltaDistY;
    } else {
      stepY = 1;
      sideDistY = (mapY + 1.0 - posY2) * deltaDistY;
    }

    while (hit == 0) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = 0;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = 1;
      }
      if (worldMap3D[mapY * mapWidth3D + mapX] == '#') hit = 1;
    }

    if (side == 0)
      perpWallDist = (sideDistX - deltaDistX);
    else
      perpWallDist = (sideDistY - deltaDistY);

    int lineHeight = (int)(mode.vRes / perpWallDist);
    int drawStart = -lineHeight / 2 + mode.vRes / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + mode.vRes / 2;
    if (drawEnd >= mode.vRes) drawEnd = mode.vRes - 1;

    uint16_t color = 0x001F;
    if (side == 1) {
      color = 0xF81F; // Pink walls for Player 2 (side walls)
    }

    gfx.drawFastVLine(x + mode.hRes / 2, drawStart, drawEnd - drawStart, color);
    ZBuffer2[x] = perpWallDist;
  }

  // Render other player on each screen
  renderOtherPlayerOnScreen(posX, posY, posX2, posY2, ZBuffer1, ZBuffer2);

  // Draw minimaps for both players
  drawMinimap(posX, posY, redColor, mode.hRes / 2 - minimapSize - 10, 10); // Player 1 minimap
  drawMinimap(posX2, posY2, blueColor, mode.hRes - minimapSize - 10, 10); // Player 2 minimap
}

void renderOtherPlayerOnScreen(float posX1, float posY1, float posX2, float posY2, float ZBufferP1[], float ZBufferP2[]) {
  // Render Player 2 on Player 1's screen
  float otherPlayerX = posX2 - posX1;
  float otherPlayerY = posY2 - posY1;
  float invDet = 1.0f / (planeX * dirY - dirX * planeY);
  float transformX = invDet * (dirY * otherPlayerX - dirX * otherPlayerY);
  float transformY = invDet * (-planeY * otherPlayerX + planeX * otherPlayerY);

  if (transformY > 0) {
    int spriteScreenX = int((mode.hRes / 4) * (1 + transformX / transformY));
    int spriteHeight = abs(int(mode.vRes / transformY)) / 2;
    int drawStartY = -spriteHeight / 2 + mode.vRes / 2;
    int drawEndY = spriteHeight / 2 + mode.vRes / 2;
    int spriteWidth = spriteHeight;
    int drawStartX = -spriteWidth / 2 + spriteScreenX;
    int drawEndX = spriteWidth / 2 + spriteScreenX;

    for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
      if (stripe > 0 && stripe < mode.hRes / 2 && transformY < ZBufferP1[stripe]) {
        gfx.drawFastVLine(stripe, drawStartY, drawEndY - drawStartY, 0x001F); // Blue color for Player 2
      }
    }
  }

  // Render Player 1 on Player 2's screen
  otherPlayerX = posX1 - posX2;
  otherPlayerY = posY1 - posY2;
  invDet = 1.0f / (planeX2 * dirY2 - dirX2 * planeY2);
  transformX = invDet * (dirY2 * otherPlayerX - dirX2 * otherPlayerY);
  transformY = invDet * (-planeY2 * otherPlayerX + planeX2 * otherPlayerY);

  if (transformY > 0) {
    int spriteScreenX = int((mode.hRes / 4) * (1 + transformX / transformY));
    int spriteHeight = abs(int(mode.vRes / transformY)) / 2;
    int drawStartY = -spriteHeight / 2 + mode.vRes / 2;
    int drawEndY = spriteHeight / 2 + mode.vRes / 2;
    int spriteWidth = spriteHeight;
    int drawStartX = -spriteWidth / 2 + spriteScreenX;
    int drawEndX = spriteWidth / 2 + spriteScreenX;

    for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
      if (stripe > 0 && stripe < mode.hRes / 2 && transformY < ZBufferP2[stripe]) {
        gfx.drawFastVLine((mode.hRes / 2) + stripe, drawStartY, drawEndY - drawStartY, 0xF800); // Red color for Player 1
      }
    }
  }
}


// Function to run the 3D game in two-player mode
void run3DGameTwoPlayer() {
  unsigned long currentMillis = millis();

  if (gameState3D == GAME_OVER_3D) {
    display3DGameOverScreen();
    if (controllerData[1].d || controllerData[2].d) {  // Use Select button to return to menu
      delay(200);
      appState = MENU_MAIN;
      displayMenuMain();
      return;
    }
  } else {
    // Handle inputs for both players
    handle3DGameInputs();
    handle3DGameInputsPlayer2();

    // Update and draw frame at regular intervals
    if (currentMillis - previousMillis3D >= interval3D) {
      previousMillis3D = currentMillis;
      drawFrame3DTwoPlayer();
    }
  }
}

// Function to handle inputs for Player 2
void handle3DGameInputsPlayer2() {
  float moveSpeed = 0.05f; // Movement speed
  float rotSpeed = 0.05f;  // Rotation speed

  // Movement Forward/Backward for Player 2
  if (controllerData[2].command & COMMAND_UP) { // Move forward
    float newPosX = posX2 + dirX2 * moveSpeed;
    float newPosY = posY2 + dirY2 * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
      posX2 = newPosX;
      posY2 = newPosY;
    }
  }
  if (controllerData[2].command & COMMAND_DOWN) { // Move backward
    float newPosX = posX2 - dirX2 * moveSpeed;
    float newPosY = posY2 - dirY2 * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
      posX2 = newPosX;
      posY2 = newPosY;
    }
  }

  // Strafing Left/Right for Player 2
  if (controllerData[2].command & COMMAND_LEFT) { // Strafe left
    float newPosX = posX2 - dirY2 * moveSpeed;
    float newPosY = posY2 + dirX2 * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
      posX2 = newPosX;
      posY2 = newPosY;
    }
  }
  if (controllerData[2].command & COMMAND_RIGHT) { // Strafe right
    float newPosX = posX2 + dirY2 * moveSpeed;
    float newPosY = posY2 - dirX2 * moveSpeed;
    if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
      posX2 = newPosX;
      posY2 = newPosY;
    }
  }

  // Rotation for Player 2 with b
  if (controllerData[2].b == -1) { // Rotate left
    float oldDirX = dirX2;
    dirX2 = dirX2 * cos(rotSpeed) - dirY2 * sin(rotSpeed);
    dirY2 = oldDirX * sin(rotSpeed) + dirY2 * cos(rotSpeed);
    float oldPlaneX = planeX2;
    planeX2 = planeX2 * cos(rotSpeed) - planeY2 * sin(rotSpeed);
    planeY2 = oldPlaneX * sin(rotSpeed) + planeY2 * cos(rotSpeed);
  }
  if (controllerData[2].b == 1) { // Rotate right
    float oldDirX = dirX2;
    dirX2 = dirX2 * cos(-rotSpeed) - dirY2 * sin(-rotSpeed);
    dirY2 = oldDirX * sin(-rotSpeed) + dirY2 * cos(-rotSpeed);
    float oldPlaneX = planeX2;
    planeX2 = planeX2 * cos(-rotSpeed) - planeY2 * sin(-rotSpeed);
    planeY2 = oldPlaneX * sin(-rotSpeed) + planeY2 * cos(-rotSpeed);
  }
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
// Leaderboard Functions
// =========================

void displayLeaderboard(const char* gameName) {
  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(2);
  gfx.setCursor(10, 10);
  gfx.print(gameName);
  gfx.print(" Leaderboard");

  // Construct filename
  String filename = "/";
  if (strcmp(gameName, "Pong") == 0) {
    filename += "pongLeaderboard.txt";
  } else if (strcmp(gameName, "Snake") == 0) {
    filename += "snakeLeaderboard.txt";
  } else if (strcmp(gameName, "Flappy Bird") == 0) {
    filename += "flappyBirdLeaderboard.txt";
  } else if (strcmp(gameName, "Doom") == 0) {
    filename += "doomLeaderboard.txt";
  } else {
    filename += "leaderboard.txt"; // Default filename
  }

  // Open file
  File file = SD.open(filename);
  if (!file) {
    gfx.setCursor(10, 50);
    gfx.print("No leaderboard data.");
  } else {
    int y = 50;
    int rank = 1;
    gfx.setTextSize(1);
    while (file.available() && rank <= 10) {
      String line = file.readStringUntil('\n');
      gfx.setCursor(10, y);
      gfx.print(rank);
      gfx.print(". ");
      gfx.print(line);
      y += 15;
      rank++;
    }
    file.close();
  }

  gfx.setCursor(10, screenHeight - 20);
  gfx.print("Press Select to return");
}

void updateLeaderboard(const char* gameName, int score) {
  // Construct filename
  String filename = "/";
  if (strcmp(gameName, "Pong") == 0) {
    filename += "pongLeaderboard.txt";
  } else if (strcmp(gameName, "Snake") == 0) {
    filename += "snakeLeaderboard.txt";
  } else if (strcmp(gameName, "Flappy Bird") == 0) {
    filename += "flappyBirdLeaderboard.txt";
  } else if (strcmp(gameName, "Doom") == 0) {
    filename += "doomLeaderboard.txt";
  } else {
    filename += "leaderboard.txt"; // Default filename
  }

  // Read existing leaderboard
  struct ScoreEntry {
    String name;
    int score;
  };

  ScoreEntry entries[10];
  int entryCount = 0;

  File file = SD.open(filename);
  if (file) {
    while (file.available() && entryCount < 10) {
      String line = file.readStringUntil('\n');
      int separatorIndex = line.indexOf(' ');
      if (separatorIndex > 0) {
        entries[entryCount].name = line.substring(0, separatorIndex);
        entries[entryCount].score = line.substring(separatorIndex + 1).toInt();
        entryCount++;
      }
    }
    file.close();
  }

  // Check if current score qualifies for top 10
  bool qualifies = false;
  if (entryCount < 10) {
    qualifies = true;
  } else {
    for (int i = 0; i < entryCount; i++) {
      if (score > entries[i].score) {
        qualifies = true;
        break;
      }
    }
  }

  if (qualifies) {
    // Set flag to enter player name
    appState = NAME_ENTRY;
  }
}

void enterPlayerName(char* playerName) {
  // Simple keyboard input simulation
  static int charIndex = 0;
  static char inputName[4] = {'_', '_', '_', '\0'};
  static bool nameEntered = false;

  gfx.fillScreen(blackColor);
  gfx.setTextColor(whiteColor);
  gfx.setTextSize(2);
  gfx.setCursor(10, 20);
  gfx.print("Enter Your Name:");

  gfx.setTextSize(3);
  gfx.setCursor(screenWidth / 2 - 30, screenHeight / 2 - 20);
  gfx.print(inputName);

  gfx.setTextSize(1);
  gfx.setCursor(10, screenHeight - 20);
  gfx.print("Use Left/Right to change");
  gfx.setCursor(10, screenHeight - 10);
  gfx.print("Select to confirm");

  // Handle input
  if (controllerData[1].b == -1) {
    // Decrement character
    if (inputName[charIndex] == '_') {
      inputName[charIndex] = 'Z';
    } else if (inputName[charIndex] == 'A') {
      inputName[charIndex] = '_';
    } else {
      inputName[charIndex]--;
    }
    delay(200);
  }
  if (controllerData[1].b == 1) {
    // Increment character
    if (inputName[charIndex] == '_') {
      inputName[charIndex] = 'A';
    } else if (inputName[charIndex] == 'Z') {
      inputName[charIndex] = '_';
    } else {
      inputName[charIndex]++;
    }
    delay(200);
  }
  if (controllerData[1].d) {
    // Move to next character
    if (charIndex < 2) {
      charIndex++;
    } else {
      // Name entry complete
      strcpy(playerName, inputName);
      nameEntered = true;
      charIndex = 0;
      memset(inputName, '_', 3);
    }
    delay(200);
  }

  if (nameEntered) {
    // Validate and save name
    saveHighScore(selectedGame, playerName);
    nameEntered = false;
    appState = MENU_MAIN;
  }
}

void saveHighScore(const char* gameName, const char* playerName) {
  // Construct filename
  String filename = "/";
  if (strcmp(gameName, "Pong") == 0) {
    filename += "pongLeaderboard.txt";
  } else if (strcmp(gameName, "Snake") == 0) {
    filename += "snakeLeaderboard.txt";
  } else if (strcmp(gameName, "Flappy Bird") == 0) {
    filename += "flappyBirdLeaderboard.txt";
  } else if (strcmp(gameName, "Doom") == 0) {
    filename += "doomLeaderboard.txt";
  } else {
    filename += "leaderboard.txt"; // Default filename
  }

  // Read existing leaderboard
  struct ScoreEntry {
    String name;
    int score;
  };

  ScoreEntry entries[10];
  int entryCount = 0;
  int newScore = 0;

  if (strcmp(gameName, "Pong") == 0) {
    newScore = playerScore;
  } else if (strcmp(gameName, "Flappy Bird") == 0) {
    newScore = fbScore;
  } else if (strcmp(gameName, "Snake") == 0) {
    newScore = snakeScore;
  }

  File file = SD.open(filename);
  if (file) {
    while (file.available() && entryCount < 10) {
      String line = file.readStringUntil('\n');
      int separatorIndex = line.indexOf(' ');
      if (separatorIndex > 0) {
        entries[entryCount].name = line.substring(0, separatorIndex);
        entries[entryCount].score = line.substring(separatorIndex + 1).toInt();
        entryCount++;
      }
    }
    file.close();
  }

  // Insert new score
  bool inserted = false;
  for (int i = 0; i < entryCount; i++) {
    if (newScore > entries[i].score) {
      // Shift scores down
      for (int j = entryCount; j > i; j--) {
        entries[j] = entries[j - 1];
      }
      entries[i].name = playerName;
      entries[i].score = newScore;
      inserted = true;
      if (entryCount < 10) entryCount++;
      break;
    }
  }
  if (!inserted && entryCount < 10) {
    entries[entryCount].name = playerName;
    entries[entryCount].score = newScore;
    entryCount++;
  }

  // Write back to file
  file = SD.open(filename, FILE_WRITE);
  if (file) {
    for (int i = 0; i < entryCount && i < 10; i++) {
      file.print(entries[i].name);
      file.print(" ");
      file.println(entries[i].score);
    }
    file.close();
  }
}

// =========================
// Utility Functions
// =========================

// Clear the screen (optional utility)
void clearScreen() {
  gfx.fillScreen(blackColor);
}
