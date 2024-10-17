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
#include <vector>
#include <algorithm>
#include "driver/ledc.h"  // For PWM functions
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

SemaphoreHandle_t xSemaphore;


// Pin definition for audio output
const int pwmPin = 1;  // GPIO pin 1 for audio output (ensure this pin is available)

// Define PWM properties
const int pwmChannel = 0;  // PWM channel 0
const int resolution = 8;   // 8-bit resolution for PWM

int previousButton3State1 = HIGH;
int previousButton3State2 = HIGH;

struct Sprite {
    float x, y;      // Position in the game world
    int texture;     // Identifier for the sprite type (e.g., 0 for bullets, 1 for player)
    float distance;  // Distance from the player (to be calculated)
    int shooter;     // For bullets, to know who shot it (1 or 2); not used for players
};

// Add this at the top of your file
std::vector<Sprite> sprites;

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
  NAME_ENTRY,
  PAUSE_MENU
};
AppState appState = STARTUP;

// =========================
// Controller Mapping
// =========================

std::map<String, int> controllerMap;  // Key: MAC address string, Value: Controller number

// =========================
// Menu Items
// =========================

const char* mainMenuItems[] = {"Pong", "Snake", "Flappy Bird", "Maze Hunter"};
const int totalMainMenuItems = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char* playerCountMenuItems[] = {"Single Player", "Two Player", "Leaderboard"};
const int totalPlayerCountMenuItems = sizeof(playerCountMenuItems) / sizeof(playerCountMenuItems[0]);

int currentMainMenuItem = 0;       // Current selection in the main menu
int currentPlayerCountItem = 0;    // Current selection in the player count menu
const char* selectedGame = nullptr; // Pointer to the selected game

// Add this near the top where you define your menu items
const char* pauseMenuItems[] = {"Resume Game", "Exit Game", "Volume Up", "Volume Down"};
const int totalPauseMenuItems = sizeof(pauseMenuItems) / sizeof(pauseMenuItems[0]);

int currentPauseMenuItem = 0; // Current selection in the pause menu




// =========================
// Timing Variables
// =========================

unsigned long startupStartTime = 0;
const long startupDuration = 5000; // Display "Scorpio" for 5 seconds
// Timing variables for debouncing
unsigned long lastNavInputTime = 0;
unsigned long lastSelectInputTime = 0;
const unsigned long debounceDelay = 20; // Debounce interval in milliseconds
const unsigned long debounceInterval = 200; // Debounce interval in milliseconds
const unsigned long debouncePause = 500; // Debounce interval in milliseconds

// Debounce timing variables
unsigned long lastButton1PressTime = 0;
unsigned long lastButton2PressTime = 0;
unsigned long lastButton3PressTime = 0;


unsigned long lastButton3PressTime1 = 0; // For Player 1
unsigned long lastButton3PressTime2 = 0; // For Player 2

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
void displayPauseMenu();

// ESP-NOW Callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);

// Utility Functions for Text Dimensions
int calculateTextWidth(const char* text, int textSize);
int calculateTextHeight(int textSize);

// Game Functions
void runSelectedGame();
void runPong(bool isSinglePlayer);
void handlePlayerInputs(bool isSinglePlayer);
void checkPaddleCollisions();
void drawGame(bool isSinglePlayer);
void resetBall();
void handlePaddleCollision(int paddleX, int paddleY, bool isPlayerPaddle);
void drawCenterLine();
void displayScores(bool isSinglePlayer);
void checkWinCondition();
void displayWinMessage(const char* winner);
void setupServe(bool isSinglePlayer);
bool lineIntersectsRect(float x1, float y1, float x2, float y2, float rectLeft, float rectTop, float rectRight, float rectBottom);
bool lineIntersectsLine(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);


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
void updateBullets();
void renderBullets(float playerX, float playerY, float dirX, float dirY, float planeX, float planeY, int screenOffset, float ZBuffer[]);
void handle3DGameInputs();
void display3DGameOverScreen();
void drawMinimap(float playerX, float playerY, uint16_t playerColor, int offsetX, int offsetY);
void run3DGameTwoPlayer();
void handle3DGameInputsPlayer2();
void drawFrame3DTwoPlayer();
void renderOtherPlayerOnScreen(float posX, float posY, float posX2, float posY2, float ZBufferP1[], float ZBufferP2[]);
void generateRandomMaze();
void resetPlayerPositions();
void drawHeart(int x, int y, uint16_t fillColor, uint16_t outlineColor);


// Leaderboard Functions
void displayLeaderboard(const char* gameName);
void updateLeaderboard(const char* gameName, int score);
void enterPlayerName(char* playerName);
void displayKeyboard(char* playerName, int maxLength);
void audioTask(void *parameter);
void stopSong();
void adjustVolume(int change);

// Snake Game
void runSnake(bool);


int volumeLevel = 128; // Initial volume level (0 - 255)
const int maxVolume = 255;
const int minVolume = 0;
bool songFinished = false;  // Global flag to indicate song completion

// =========================
// Game Variables (Pong)
// =========================

// Ball Properties
// Previous Ball Position
float prevBallX;
float prevBallY;
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

// Define the line segment of the ball's movement
float ballDeltaX;
float ballDeltaY;

// Player Paddle Rectangle
float paddleLeft;
float paddleRight;
float paddleTop;
float paddleBottom;

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
const int mapWidth3D = 10;
const int mapHeight3D = 10;

char worldMap3D[mapWidth3D * mapHeight3D];

// Timing for 3D Game
unsigned long previousMillis3D = 0;
const long interval3D = 16;  // Frame update interval (~60 FPS)

struct Bullet {
    float x;
    float y;
    float dirX;
    float dirY;
    bool active;
    int shooter; // 1 for Player 1, 2 for Player 2
};

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
float posX = 1.5f, posY = 1.5f;   // Player 1 position
float dirX = -1.0f, dirY = 0.0f;  // Player 1 direction vector
float planeX = 0.0f, planeY = 0.66f; // Player 1 camera plane

// Player 2 Properties
float posX2 = mapWidth3D - 2.5f, posY2 = mapHeight3D - 2.5f;   // Player 2 position
float dirX2 = -1.0f, dirY2 = 0.0f;  // Player 2 direction vector
float planeX2 = 0.0f, planeY2 = 0.66f; // Player 2 camera plane
bool twoPlayerMode = false;

// Player Health and Lives
int player1Health = 8;
int player2Health = 8;
int player1Lives = 3;
int player2Lives = 3;

const unsigned long shootCooldown = 350; // Cooldown duration in milliseconds (adjust as needed)

//  Frequencies
// Octave 3
const int C3  = 131;
const int Cs3 = 139;  // C#3 / D♭3
const int D3  = 147;
const int Ds3 = 156;  // D#3 / E♭3
const int E3  = 165;
const int F3  = 175;
const int Fs3 = 185;  // F#3 / G♭3
const int G3  = 196;
const int Gs3 = 208;  // G#3 / A♭3
const int A3  = 220;
const int As3 = 233;  // A#3 / B♭3
const int B3  = 247;

// Octave 4
const int C4  = 262;
const int Cs4 = 277;  // C#4 / D♭4
const int D4  = 294;
const int Ds4 = 311;  // D#4 / E♭4
const int E4  = 330;
const int F4  = 349;
const int Fs4 = 370;  // F#4 / G♭4
const int G4  = 392;
const int Gs4 = 415;  // G#4 / A♭4
const int A4  = 440;  // A4 is standard tuning
const int As4 = 466;  // A#4 / B♭4
const int B4  = 494;

// Octave 5
const int C5  = 523;
const int Cs5 = 554;  // C#5 / D♭5
const int D5  = 587;
const int Ds5 = 622;  // D#5 / E♭5
const int E5  = 659;
const int F5  = 698;
const int Fs5 = 740;  // F#5 / G♭5
const int G5  = 784;
const int Gs5 = 831;  // G#5 / A♭5
const int A5  = 880;
const int As5 = 932;  // A#5 / B♭5
const int B5  = 988;

// Octave 6
const int C6  = 1047;
const int Cs6 = 1109;  // C#6 / D♭6
const int D6  = 1175;
const int Ds6 = 1245;  // D#6 / E♭6
const int E6  = 1319;
const int F6  = 1397;
const int Fs6 = 1480;  // F#6 / G♭6
const int G6  = 1568;
const int Gs6 = 1661;  // G#6 / A♭6
const int A6  = 1760;
const int As6 = 1865;  // A#6 / B♭6
const int B6  = 1976;

// Song Structure
struct Song {
    int bpm;               // Beats per minute
    float (*notes)[2];     // Pointer to a 2D array where [Note, Duration_in_beats]
    int length;            // Number of notes in the song
    bool loop;             // Whether the song should loop after finishing
};

void startSong(Song &song);

Song currentSong;

// Define the Doom song with notes and durations (in beats)

float doomNotes[][2] = {
    {E3, 0.5},  // E3 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E4, 1},    // E4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {D4, 1},    // D4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {C4, 1},    // C4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 1},   // A#4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 0.5}, // B4 for 1 beat
    {B4, 0.5},  // C4 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E4, 1},    // E4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {D4, 1},    // D4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {C4, 1},    // C4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 3},  // A#4 for 1.5 beats
    {E3, 0.5},  // E3 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E4, 1},    // E4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {D4, 1},    // D4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {C4, 1},    // C4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 1},   // A#4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 0.5}, // B4 for 1 beat
    {B4, 0.5},  // C4 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E3, 0.5},  // E3 for half a beat
    {E4, 1},    // E4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {D4, 1},    // D4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {C4, 1},    // C4 for 1 beat
    {E3, 0.5},  // E3 for half a beat
    {As3, 3},  // A#4 for 1.5 beats
    {A4, 0.5},  // E3 for half a beat
    {A4, 0.5},  // E3 for half a beat
    {A5, 1},    // E4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {G4, 1},    // D4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {F4, 1},    // C4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {Ds4, 1},   // A#4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {E4, 0.5}, // B4 for 1 beat
    {F4, 0.5},  // C4 for half a beat
    {A4, 0.5},  // E3 for half a beat
    {A4, 0.5},  // E3 for half a beat
    {A5, 1},    // E4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {G4, 1},    // D4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {F4, 1},    // C4 for 1 beat
    {A4, 0.5},  // E3 for half a beat
    {Ds4, 3},   // A#4 for 1 beat
};

// Create the song struct
Song DoomSong = {
  250,  // BPM
  doomNotes,  // Pointer to the 2D array of notes
  sizeof(doomNotes) / sizeof(doomNotes[0]),  // Calculate the length of the array
  true
};

float menuNotes[][2] = {
    //LINE 1
    {Fs4, 1},   // B4 for half a beat
    {A4, 0.5},  // B4 for half a beat
    {Cs5, 0.5}, // B4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {A4, 0.5},  // A4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {D4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // E4 for half a beat
    {D4, 0.5},  // E4 for half a beat
    {-1, 2},   // for a quarter beat
    {Cs4, 0.5}, // D4 for half a beat
    {D4, 0.5},  // E4 for half a beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {A4, 0.5},  // E4 for half a beat
    {Cs5, 0.5}, // Fs4 for half a beat
    {-1, 0.5},  // for a quarter beat
    {A4, 0.5},  // Fs4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Fs4, 0.5}, // D4 for half a beat
    {E5, 1.5},  // E4 for half a beat
    {Ds5, 0.5}, // Fs4 for three-quarters of a beat
    {D5, 1},    // D4 for one beat
    {-1, 0.5},   // for a quarter beat

    //LINE 2
    {Gs4, 1},   // E4 for three-quarters of a beat
    {Cs5, 0.5}, // Fs4 for three-quarters of a beat    
    {Fs4, 0.5}, // D4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Cs5, 0.5}, // E4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Gs4, 0.5}, // Fs4 for two beats    
    {-1, 0.5},   // for a quarter beat
    {Cs5, 0.5}, // Fs4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {G4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5}, // D4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {E4, 0.5},  // E4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {E4, 0.5},  // Fs4 for three-quarters of a beat
    {E4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {-1.5, 0.5}, // for a quarter beat
    {E4, 0.5},  // Fs4 for three-quarters of a beat
    {E4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {-1, 1.5}, // for a quarter beat
    {Ds4, 1},   // Fs4 for two beats
    {D4, 1},    // Fs4 for half a beat

    //LINE 3
    {Cs4, 1},   // Fs4 for half a beat
    {A4, 0.5},  // D4 for half a beat
    {Cs5, 0.5},  // E4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {A4, 0.5},  // Fs4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // Fs4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {E5, 0.5},  // D4 for half a beat
    {E5, 0.5},  // D4 for half a beat
    {E5, 0.5},  // B3 for three beats
    {-1, 0.5},   // for a quarter beat
    {Fs4, 0.5}, // B4 for half a beat
    {A4, 0.5},  // B4 for half a beat
    {Cs5, 0.5}, // B4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {A4, 0.5},  // A4 for half a beat
    {-1, 0.5},   // for a quarter beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {Cs5, 2},   // B4 for two beats
    {B4, 1},    // B4 for one beat
    {-1, 0.5},   // for a quarter beat

    //LINE 4
    {B4, 0.5},  // B4 for half a beat
    {G4, 0.5},  // A4 for half a beat
    {D4, 0.5},  // Fs4 for half a beat
    {Cs4, 1},   // B4 for half a beat
    {B4, 0.5},  // A4 for half a beat
    {G4, 0.5},  // Fs4 for half a beat
    {Cs4, 0.5},  // B4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {C4, 0.5},  // B4 for half a beat
    {B3, 1},    // A4 for half a beat
    {F4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // B4 for half a beat
    {B3, 0.5},  // A4 for half a beat
    {E4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},  // B4 for half a beat
    {E4, 0.5},  // A4 for half a beat
    {-1, 2},   // for a quarter beat
    {As4, 0.5},  // Fs4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {C5, 0.5},  // A4 for half a beat
    {D5, 0.5},  // Fs4 for half a beat
    {Fs5, 0.5},  // B4 for half a beat
    {A5, 1},    // A4 for one beat
    {-1, 3},   // for a quarter beat
    {A3, 1},    // Fs4 for half a beat
    {As3, 1},   // B4 for half a beat

    //LINE 5
    {B3, 1.5},  // A4 for half a beat
    {As3, 0.5},  // Fs4 for half a beat
    {B3, 3},    // B4 for three beats
    {A3, 0.5},  // A4 for half a beat
    {As3, 0.5},  // Fs4 for half a beat
    {B3, 0.5},  // B4 for half a beat
    {Fs4, 1},   // A4 for half a beat
    {Cs4, 0.5},  // Fs4 for half a beat
    {B3, 1.5},  // B4 for half a beat
    {As3, 0.5},  // A4 for half a beat
    {B3, 4},    // Fs4 for half a beat
    {B3, 1},    // B4 for half a beat
    {B3, 1},    // B4 for half a beat

    //LINE 6
    {Cs4, 1.5},  // A4 for half a beat
    {C4, 0.5},  // Fs4 for half a beat
    {Cs4, 3},    // B4 for three beats
    {Cs4, 0.5},  // A4 for half a beat
    {C4, 0.5},  // Fs4 for half a beat
    {Cs4, 0.5},  // B4 for half a beat
    {Gs4, 1},    // A4 for half a beat
    {Ds4, 0.5},  // Fs4 for half a beat
    {Cs4, 1.5},  // B4 for half a beat
    {Ds4, 0.5},  // A4 for half a beat
    {B3, 1.5},   // Fs4 for half a beat
    {Cs4, 0.5},  // B4 for half a beat
    {D4, 0.5},   // A4 for half a beat
    {A4, 1},     // Fs4 for half a beat
    {D4, 0.5},   // B4 for half a beat
    {Gs4, 0.5},  // A4 for half a beat
    {Gs4, 0.5},  // Fs4 for half a beat
    {Gs4, 0.5},  // B4 for half a beat
    {-1, 0.5},    // for a quarter beat
};

// Create the song struct
Song menuSong = {
  114,  // BPM
  menuNotes,  // Pointer to the 2D array of notes
  sizeof(menuNotes) / sizeof(menuNotes[0]),  // Calculate the length of the array
  true
};

float flappyBirdNotes[][2] {
    /*I'm in the thick of it, everybody knows
    They know me where it snows, I skied in and they froze
    I don't know no nothin' 'bout no ice, I'm just cold
    Forty somethin' milli' subs or so, I've been told*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 1},  // Fs4 for one beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 2},  // Fs4 for two beats
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat
    {D4, 0.75},  // D4 for three-quarters of a beat
    {E4, 0.75},  // E4 for three-quarters of a beat
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat    
    {D4, 0.75},  // D4 for three-quarters of a beat
    {E4, 0.75},  // E4 for three-quarters of a beat
    {Fs4, 2},  // Fs4 for two beats    
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 2},  // Fs4 for two beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat
    {D4, 0.75},  // D4 for three-quarters of a beat
    {D4, 0.75},  // D4 for three-quarters of a beat
    {B3, 3},  // B3 for three beats

    /*I'm in my prime, and this ain't even final form
    They knocked me down, but still, my feet, they find the floor
    I went from living rooms straight out to sold-out tours
    Life's a fight, but trust, I'm ready for the war*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.25},  // B4 for a quarter beat
    {A4, 0.75},  // A4 for three-quarters of a beat
    {A4, 0.5},  // A4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.25},  // E4 for a quarter beat
    {Fs4, 3},  // Fs4 for three beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.25},  // Fs4 for a quarter beat
    {Fs4, 1},  // Fs4 for one beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {B3, 3},  // B3 for three beats
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {D5, 1.5},  // D5 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 3},  // Fs4 for three beats
    {Fs4, 0.75},  // Fs4 for three-quarters of a beat
    {E4, 0.75},  // E4 for three-quarters of a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},  // E4 for half a beat
    {D4, 0.5},  // D4 for half a beat
    {B3, 3},   // B3 for three beats

    /*Woah-oh-oh
    This is how the story goes
    Woah-oh-oh
    I guess this is how the story goes*/

    {B4, 0.5},  // B4 for half a beat
    {A4, 1},    // A4 for one beat
    {B4, 0.5},  // C5 for half a beat
    {A4, 1},    // A4 for one beat
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B3, 0.5},  // B3 for half a beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 1.5}, // Fs4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {B4, 3},    // B4 for three beats

    /*I'm in the thick of it, everybody knows
    They know me where it snows, I skied in and they froze
    I don't know no nothin' 'bout no ice, I'm just cold
    Forty somethin' milli' subs or so, I've been told*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B5 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 1},   // Fs5 for one beat
    {E4, 0.5},  // E5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {D4, 0.5},  // D#5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // B5 for two beats
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {B3, 1.5},   // C5 for one and a half beats


    /*From the screen to the ring, to the pÐµn, to the king
    Where's my crown? That's my bling, always drama when I ring
    See, I believe that if I see it in my heart
    Smash through the ceiling 'cause I'm reaching for the stars*/

    {B4, 0.25},  // B4 for a quarter beat
    {B4, 0.5},   // B4 for half a beat
    {A4, 1},     // A4 for one beat
    {A4, 0.25},  // A4 for a quarter beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 1},     // A4 for one beat
    {D5, 0.25},  // D5 for a quarter beat
    {D5, 0.5},   // D5 for half a beat
    {D5, 1},     // D5 for one beat
    {D4, 0.25},  // D4 for a quarter beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 1},    // Fs4 for one beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 1},    // Fs4 for one beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1},     // E4 for one beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.25},  // E4 for a quarter beat
    {D4, 0.5},   // D4 for half a beat
    {B3, 2.5},   // B3 for two and a half beats
    {B4, 0.5},   // B4 for half a beat
    {B4, 0.5},  // Bs4 for half a beat
    {B4, 0.25},  // B4 for a quarter beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {D5, 0.5},   // D5 for half a beat
    {D5, 0.5},   // D5 for half a beat
    {D5, 0.5},   // D5 for half a beat
    {D4, 0.5},   // D4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 2},    // Fs4 for two beats
    {G4, 0.5},   // G4 for half a beat
    {G4, 0.5},   // G4 for half a beat
    {Fs4, 0.25}, // Fs4 for a quarter beat
    {Fs4, 0.75}, // Fs4 for three-quarters of a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.55},  // E4 for half a beat
    {D4, 0.25},  // D4 for a quarter beat
    {B3, 3},      // B3 for three beats


    /*Woah-oh-oh
    This is how the story goes
    Woah-oh-oh
    I guess this is how the story goes*/

    {B4, 0.5},  // B4 for half a beat
    {A4, 1},    // A4 for one beat
    {B4, 0.5},  // C5 for half a beat
    {A4, 1},    // A4 for one beat
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B3, 0.5},  // B3 for half a beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 1.5}, // Fs4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {B4, 3},    // B4 for three beats

    /*I'm in the thick of it, everybody knows
    They know me where it snows, I skied in and they froze
    I don't know no nothin' 'bout no ice, I'm just cold
    Forty somethin' milli' subs or so, I've been told*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B5 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 1},   // Fs5 for one beat
    {E4, 0.5},  // E5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {D4, 0.5},  // D#5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // B5 for two beats
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {B3, 1.5},   // C5 for one and a half beats

    /*Highway to heaven, I'm just cruisin' by my lone'
    They cast me out, left me for dead, them people cold
    My faith in God, mind in the sun, I'm 'bout to sow (Yeah)
    My life is hard, I took the wheel, I cracked the code (Yeah-yeah, woah-oh-oh)
    Ain't nobody gon' save you, man, this life will break you (Yeah, woah-oh-oh)
    In the thick of it, this is how the story goes*/

    {B4, 0.5},   // B4 for half a beat
    {B4, 0.5},   // B4 for half a beat
    {B4, 0.25},  // B4 for a quarter beat
    {A4, 0.75},  // A4 for three-quarters of a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 3},    // Fs4 for three beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.25}, // Fs4 for a quarter beat
    {G4, 0.75},  // G4 for three-quarters of a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {D4, 0.25},  // D4 for a quarter beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {B3, 3},      // B3 for three beats

    {B4, 0.5},   // B4 for half a beat
    {B4, 0.5},   // B4 for half a beat
    {B4, 0.25},  // B4 for a quarter beat
    {A4, 0.75},  // A4 for three-quarters of a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {A4, 0.25},  // A4 for a quarter beat
    {A4, 0.5},   // A4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 3},    // Fs4 for three beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {D4, 0.25},  // D4 for a quarter beat
    {E4, 0.75},  // E4 for three-quarters of a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {D4, 0.25},  // D4 for a quarter beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {D4, 0.5},   // D4 for half a beat
    {D4, 3},     // D4 for three beats

    {B4, 0.5},   // B4 for half a beat
    {A4, 1},     // A4 for one beat
    {B4, 0.5},   // B4 for half a beat
    {A4, 1},     // A4 for one beat
    {G4, 0.5},   // G4 for half a beat
    {Fs4, 2},    // Fs4 for two beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {Fs4, 1},    // Fs4 for one beat
    {A4, 1},     // A4 for one beat
    {Fs4, 1},    // Fs4 for one beat
    {E4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 0.5},   // E4 for half a beat
    {D4, 0.5},   // D4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {G4, 0.25},  // G4 for a quarter beat
    {Fs4, 2},    // Fs4 for two beats
    {B4, 0.5},   // B4 for half a beat
    {A4, 1},     // A4 for one beat
    {B4, 0.5},   // B4 for half a beat
    {A4, 1},     // A4 for one beat
    {G4, 0.5},   // G4 for half a beat
    {Fs4, 2},    // Fs4 for two beats
    {B4, 0.5},   // B4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1.5},   // E4 for one and a half beats
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1.5},   // E4 for one and a half beats
    {D4, 0.5},   // D4 for half a beat
    {B4, 3},     // B4 for three beats


    /*I'm in the thick of it, everybody knows
    They know me where it snows, I skied in and they froze
    I don't know no nothin' 'bout no ice, I'm just cold
    Forty somethin' milli' subs or so, I've been told*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B5 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 1},   // Fs5 for one beat
    {E4, 0.5},  // E5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {D4, 0.5},  // D#5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // B5 for two beats
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {B3, 1.5},   // C5 for one and a half beats

    /*I'm in the thick of it, everybody knows
    They know me where it snows, I skied in and they froze
    I don't know no nothin' 'bout no ice, I'm just cold
    Forty somethin' milli' subs or so, I've been told*/

    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B5 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 1},   // Fs5 for one beat
    {E4, 0.5},  // E5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {D4, 0.5},  // D#5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // B5 for two beats
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {E4, 0.75},  // E5 for three-quarters of a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 2},   // Fs5 for two beats
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {Fs4, 0.5},  // Fs5 for half a beat
    {D4, 0.5},  // D5 for half a beat
    {E4, 0.5},  // E5 for half a beat
    {Fs4, 0.75},  // Fs5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {D4, 0.75},  // D5 for three-quarters of a beat
    {B3, 1.5},   // C5 for one and a half beats

    /*Woah-oh-oh
    This is how the story goes
    Woah-oh-oh
    I guess this is how the story goes*/

    {B4, 0.5},  // B4 for half a beat
    {A4, 1},    // A4 for one beat
    {B4, 0.5},  // C5 for half a beat
    {A4, 1},    // A4 for one beat
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B3, 0.5},  // B3 for half a beat
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {Fs4, 3},   // Fs4 for three beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {A4, 1.5},  // A4 for one and a half beats
    {G4, 0.5},  // G4 for half a beat
    {Fs4, 1.5}, // Fs4 for one and a half beats
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {B4, 0.5},  // B4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {Fs4, 0.5}, // Fs4 for half a beat
    {E4, 1.5},  // E4 for one and a half beats
    {D4, 0.5},  // D4 for half a beat
    {B4, 3},    // B4 for three beats
};

Song flappyBirdSong {
  146,
  flappyBirdNotes,
  sizeof(flappyBirdNotes) / sizeof(flappyBirdNotes[0]),  // Length of the song
  true
};

// Define the Snake song with notes and durations (in beats) 
// F major
// 115 BPM
float snakeNotes[][2] = {
    //LINE 1
    {F4, 0.5},  // B4 for half a beat
    {G4, 0.5},  // B4 for half a beat
    {A5, 1.5},  // B4 for half a beat
    {G4, 0.5},  // A4 for half a beat
    {F4, 1},    // Fs4 for half a beat
    {C5, 0.5},  // Fs4 for one beat
    {F4, 0.5},  // E4 for half a beat
    {G4, 2},    // E4 for two beats
    {G4, 0.5},  // D4 for half a beat
    {A4, 0.5},  // E4 for half a beat
    {B4, 1.5},  // Fs4 for one and a half beats
    {A4, 1.5},  // E4 for one and a half beats
    {G4, 1},    // Fs4 for one beat

    //LINE 2
    {C4, 0.5},  // B4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {G4, 1.5},  // Fs4 for one and a half beats
    {E4, 0.5},  // Fs4 for half a beat
    {F4, 0.5},  // E4 for half a beat
    {G4, 0.5},  // E4 for half a beat
    {F4, 1.5},  // D4 for one and a half beats
    {A4, 1.5},  // E4 for one and a half beats
    {F4, 1},    // Fs4 for one beat
    {G4, 0.5},  // E4 for half a beat
    {A5, 0.5},  // Fs4 for half a beat
    {G4, 1.5},  // E4 for one and a half beats
    {-1, 0.5},    // for a quarter beat
    {C5, 1},    // E4 for one beat
    {D5, 1.5},  // D4 for one and a half beats
    {C5, 1.5},  // E4 for one and a half beats
    {A4, 1},    // Fs4 for one beat

    //LINE 3
    {G4, 3},    // E4 for three beats
    {-1, 1},      // for a quarter beat

    //REPEAT
    //LINE 1
    {F4, 0.5},  // B4 for half a beat
    {G4, 0.5},  // B4 for half a beat
    {A5, 1.5},  // B4 for half a beat
    {G4, 0.5},  // A4 for half a beat
    {F4, 1},    // Fs4 for half a beat
    {C5, 0.5},  // Fs4 for half a beat
    {F4, 0.5},  // E4 for half a beat
    {G4, 2},    // E4 for two beats
    {G4, 0.5},  // D4 for half a beat
    {A4, 0.5},  // E4 for half a beat
    {B4, 1.5},  // Fs4 for one and a half beats
    {A4, 1.5},  // E4 for one and a half beats
    {G4, 1},    // Fs4 for one beat

    //LINE 2
    {C4, 0.5},  // B4 for half a beat
    {A4, 0.5},  // A4 for half a beat
    {G4, 1.5},  // Fs4 for one and a half beats
    {E4, 0.5},  // Fs4 for half a beat
    {F4, 0.5},  // E4 for half a beat
    {G4, 0.5},  // E4 for half a beat
    {F4, 1.5},  // D4 for one and a half beats
    {A4, 1.5},  // E4 for one and a half beats
    {F4, 1},    // Fs4 for one beat
    {G4, 0.5},  // E4 for half a beat
    {A5, 0.5},  // Fs4 for half a beat
    {G4, 1.5},  // E4 for one and a half beats
    {-1, 0.5},    // for a quarter beat
    {C5, 1},    // E4 for one beat
    {D5, 1.5},  // D4 for one and a half beats
    {C5, 1.5},  // E4 for one and a half beats
    {A4, 1},    // Fs4 for one beat

    //LINE 3
    {G4, 3},    // E4 for three beats
    {-1, 1},      // for a quarter beat


    {Cs5, 0.5},  // Fs4 for half a beat  
    {Cs5, 0.5},  // B4 for half a beat
    {A4, 0.5},   // A4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {D4, 0.5},   // Fs4 for half a beat
    {D4, 0.5},   // E4 for half a beat
    {D4, 0.5},   // E4 for half a beat
    {Cs4, 0.5},  // D4 for half a beat
    {D4, 0.5},   // E4 for half a beat
    {Fs4, 0.5},  // Fs4 for half a beat
    {A4, 0.5},   // E4 for half a beat
    {Cs5, 0.5},  // Fs4 for half a beat

    //LINE 4
    {-1, 2.5},    // for two and a half beats
    {C4, 0.5},  // Fs4 for half a beat  
    {D4, 0.5},  // B4 for half a beat
    {E4, 0.5},  // A4 for half a beat
};

Song snakeSong {
  149,
  snakeNotes,
  sizeof(snakeNotes) / sizeof(snakeNotes[0]),  // Length of the song
  true
};

float shutdownNotes [][2] {
  {Gs5, 0.67},  // B4 for half a beat
    {Ds5, 0.67},  // B4 for half a beat
    {Gs4, 0.67},  // B4 for half a beat
    {As4, 2},  // A4 for half a beat
};

Song shutDownSong {
  115,
  shutdownNotes,
  sizeof(shutdownNotes) / sizeof(shutdownNotes[0]),
  false
};

float startNotes[][2] = {
    {-1, 1},
    {Ds6, 0.583},  // B4 for half a beat
    {Ds5, 0.167},  // B4 for half a beat
    {As5, 1},  // B4 for half a beat
    {Gs5, 1.33},  // A4 for half a beat
    {Ds6, 0.67},  // Fs4 for one beat
    {As5, 2},  // E4 for half a beat 
};

// Create the song struct
Song startupSong = {
  115,  // BPM
  startNotes,  // Pointer to the 2D array of notes
  sizeof(startNotes) / sizeof(startNotes[0]),  // Calculate the length of the array
  false
};






// Variables for song playback
bool isSongPlaying = false;
unsigned long noteStartTime = 0;
int currentNoteIndex = 0;
int beatDuration = 0;

// =========================
// Setup Function
// =========================

void setup() {

    // Create the mutex
  xSemaphore = xSemaphoreCreateMutex();

  // Create the audio task
  xTaskCreatePinnedToCore(
      audioTask,    // Function to implement the task
      "Audio Task", // Name of the task
      2048,         // Stack size in words
      NULL,         // Task input parameter
      1,            // Priority of the task
      NULL,         // Task handle
      0             // Core where the task should run (0 or 1)
  );

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

  // Start the startup song
  startSong(startupSong);

    // Set up the PWM channel
  ledcSetup(pwmChannel, 2000, resolution);  // Initialize with an arbitrary frequency (e.g., 2000 Hz)

  // Attach the PWM channel to the pin
  ledcAttachPin(pwmPin, pwmChannel);
}

// =========================
// Loop Function
// =========================

void loop() {
    unsigned long currentMillis = millis();  // Current time for debounce and timing

    static AppState previousAppState = STARTUP;  // Initialize to STARTUP or an invalid state
    // Check for appState change
    if (appState != previousAppState) {
        // State has changed
        // Stop any playing song
        stopSong();

        // Start new song if needed
        switch (appState) {
            case STARTUP:
                startSong(startupSong);
                break;
            case MENU_MAIN:
                startSong(menuSong);
                break;
            case GAME_RUNNING:
                // Start game-specific music
                if (strcmp(selectedGame, "Flappy Bird") == 0) {
                    startSong(flappyBirdSong);
                } else if (strcmp(selectedGame, "Maze Hunter") == 0) {
                    startSong(DoomSong);
                } else if (strcmp(selectedGame, "Snake") == 0) {
                    startSong(snakeSong);
                }
                // Add additional games and their songs as needed
                break;
            // Handle other states as needed
            default:
                // For other states, we might not play any song
                break;
        }

        previousAppState = appState;
    }

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
            back(appState, currentMillis);  // Call the back function and pass currentMillis
            break;

        case MENU_PLAYER_COUNT:
            // Continuously display the player count menu to prevent flickering
            displayMenuPlayerCount();
            back(appState, currentMillis);  // Call the back function and pass currentMillis
            break;

        case DISPLAY_LEADERBOARD:
            displayLeaderboard(selectedGame);
            back(appState, currentMillis);  // Call the back function and pass currentMillis
            break;

        case NAME_ENTRY:
            // Handle name entry for high score
            enterPlayerName(playerName);
            back(appState, currentMillis);  // Call the back function and pass currentMillis
            break;

        case GAME_RUNNING:
            runSelectedGame();
            checkPause(); // Add this line to continuously check for pause input
            break;

        case PAUSE_MENU:
            displayPauseMenu();
            break;
    }

    // Refresh the VGA display
    vga.show();
}

void back(AppState state, unsigned long currentMillis) {
  // Check if button 1 is pressed and debounce is respected
  if (controllerData[1].button1 == LOW && (currentMillis - lastSelectInputTime >= debounceInterval)) {
    lastSelectInputTime = currentMillis;
    
    // Navigate back depending on the current appState
    if (state == MENU_MAIN) {
      appState = STARTUP;  // Go back to STARTUP screen from MAIN MENU
    }
    else if (state == MENU_PLAYER_COUNT) {
      appState = MENU_MAIN;  // Go back to MAIN MENU from PLAYER COUNT MENU
    }
    else if (state == DISPLAY_LEADERBOARD) {
      appState = MENU_PLAYER_COUNT;  // Go back to PLAYER COUNT MENU from LEADERBOARD
    }
    else if (state == NAME_ENTRY) {
      appState = MENU_PLAYER_COUNT;  // Go back to PLAYER COUNT MENU from NAME ENTRY
    }
  }
}

// =========================
// ESP-NOW Callback Function
// =========================


// Add this function to generate player count menu items based on the selected game
// Function to generate player count menu items based on the selected game
std::vector<const char*> getPlayerCountMenuItems() {
  std::vector<const char*> items;
  if (strcmp(selectedGame, "Maze Hunter") == 0) {
    items.push_back("Two Player");
  } else if (strcmp(selectedGame, "Pong") == 0) {
    items.push_back("Single Player");
    items.push_back("Two Player");
  } else if (strcmp(selectedGame, "Snake") == 0) {
    items.push_back("Single Player");
    items.push_back("Leaderboard");
  } else if (strcmp(selectedGame, "Flappy Bird") == 0) {
    items.push_back("Single Player");
    items.push_back("Two Player");
    items.push_back("Leaderboard");
  }
  return items;
}


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

  unsigned long currentMillis = millis();

  if(appState == MENU_MAIN) {
    // Handle navigation in main menu
    if (data.button2 == LOW && (currentMillis - lastSelectInputTime >= debounceInterval)) { // Select button pressed
      lastSelectInputTime = currentMillis; // Debounce select button
      // Handle selection based on currentMainMenuItem
      selectedGame = mainMenuItems[currentMainMenuItem];
      // Transition to player count selection
      appState = MENU_PLAYER_COUNT;
      currentPlayerCountItem = 0; // Reset player count selection
    } else {
      // Handle navigation with joystick 1
      if ((data.command1 & COMMAND_DOWN) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move down
        currentMainMenuItem = (currentMainMenuItem + 1) % totalMainMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
      } else if ((data.command1 & COMMAND_UP) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move up
        currentMainMenuItem = (currentMainMenuItem - 1 + totalMainMenuItems) % totalMainMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
      }
    }
  }
  else if(appState == MENU_PLAYER_COUNT) {
    // Handle navigation in player count menu
    if (data.button2 == LOW && (currentMillis - lastSelectInputTime >= debounceInterval)) { // Select button pressed
      lastSelectInputTime = currentMillis; // Debounce select button
      // Handle selection based on currentPlayerCountItem
      const char* playerCount = playerCountMenuItems[currentPlayerCountItem];
      std::vector<const char*> menuItems = getPlayerCountMenuItems();
      int totalItems = menuItems.size();
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
          bool isSinglePlayer = (currentPlayerCountItem == 0);
          runFlappyBird(isSinglePlayer);
          startSong(flappyBirdSong);  // Start the Flappy Bird song
        }
        else if (strcmp(selectedGame, "Maze Hunter") == 0) {
          if (currentPlayerCountItem == 0) { // Two-player mode selected
            twoPlayerMode = true;
            // Initialize 3D game variables
            generateRandomMaze();
            resetPlayerPositions();   // Reset player positions
            gameState3D = GAME_PLAYING_3D;
            player1Health = 8;        // Reset health
            player2Health = 8;
            player1Lives = 3;         // Reset lives
            player2Lives = 3;
            startSong(DoomSong);
          } else if (currentPlayerCountItem == 1) { // Leaderboard selected
            appState = DISPLAY_LEADERBOARD;
          }
        }
      }
    } else {
      // Handle navigation with joystick 1
      if ((data.command1 & COMMAND_DOWN) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move down
        currentPlayerCountItem = (currentPlayerCountItem + 1) % totalPlayerCountMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
      } else if ((data.command1 & COMMAND_UP) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move up
        currentPlayerCountItem = (currentPlayerCountItem - 1 + totalPlayerCountMenuItems) % totalPlayerCountMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
      }
    }
  }
  else if (appState == DISPLAY_LEADERBOARD) {
      if (data.button2 == LOW && (currentMillis - lastSelectInputTime >= debounceInterval)) { // Press select to return
        lastSelectInputTime = currentMillis; // Debounce select button
        appState = MENU_PLAYER_COUNT;
      }
  }
  else if (appState == NAME_ENTRY) {
    // Handle name entry for high score
    // Implementation will be in enterPlayerName function
  }
  else if (appState == GAME_RUNNING) {
    // Game is running, inputs are handled within the game functions
  }
  else if (appState == PAUSE_MENU) {
    unsigned long currentMillis = millis();

    // Handle navigation in pause menu
    if ((data.command1 & COMMAND_DOWN) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move down
        currentPauseMenuItem = (currentPauseMenuItem + 1) % totalPauseMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
    } else if ((data.command1 & COMMAND_UP) && (currentMillis - lastNavInputTime >= debounceInterval)) { // Move up
        currentPauseMenuItem = (currentPauseMenuItem - 1 + totalPauseMenuItems) % totalPauseMenuItems;
        lastNavInputTime = currentMillis; // Debounce navigation input
    }

    // Handle selection
    if (data.button2 == LOW && (currentMillis - lastSelectInputTime >= debounceInterval)) { // Select button pressed
        lastSelectInputTime = currentMillis; // Debounce select button

        // Perform action based on the selected menu item
    if (strcmp(pauseMenuItems[currentPauseMenuItem], "Resume Game") == 0) {
        appState = GAME_RUNNING; // Resume the game
        previousButton3State1 = controllerData[1].button3;
        previousButton3State2 = controllerData[2].button3;
        // Reset debounce timers if necessary
        lastButton3PressTime1 = millis();
        lastButton3PressTime2 = millis();
    } else if (strcmp(pauseMenuItems[currentPauseMenuItem], "Exit Game") == 0) {
            appState = MENU_MAIN; // Exit to main menu
            displayMenuMain();
        } else if (strcmp(pauseMenuItems[currentPauseMenuItem], "Volume Up") == 0) {
            adjustVolume(10); // Increase volume
        } else if (strcmp(pauseMenuItems[currentPauseMenuItem], "Volume Down") == 0) {
            adjustVolume(-10); // Decrease volume
        }
    }
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

  // Get dynamic menu items
  std::vector<const char*> menuItems = getPlayerCountMenuItems();
  int totalItems = menuItems.size();

  // Ensure the current selection index is within bounds
  if (currentPlayerCountItem >= totalItems) {
    currentPlayerCountItem = 0;
  }

  // Menu Items
  for (int i = 0; i < totalItems; i++) {
    if (i == currentPlayerCountItem) {
      gfx.setTextColor(highlightColor);
    } else {
      gfx.setTextColor(whiteColor);
    }

    // Calculate cursor position
    int cursorX = 20;
    int cursorY = 50 + i * 30;

    gfx.setCursor(cursorX, cursorY);
    gfx.println(menuItems[i]);
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
  else if (strcmp(selectedGame, "Maze Hunter") == 0) {
    if (currentPlayerCountItem == 0) { // Two-player mode selected
      twoPlayerMode = true;
      run3DGameTwoPlayer();
    }
  }
}

void checkPause() {
    unsigned long currentMillis = millis();

    // Check if controllerData[1] exists
    if (controllerData.find(1) != controllerData.end()) {
        int currentButton3State1 = controllerData[1].button3;

        // Check for transition from HIGH to LOW
        if (previousButton3State1 == HIGH && currentButton3State1 == LOW) {
            if (currentMillis - lastButton3PressTime1 >= debouncePause) {
                appState = PAUSE_MENU;
                lastButton3PressTime1 = currentMillis; // Update debounce time
                // Update previous state after handling the event
            }
        }
        // Update previous state
        previousButton3State1 = currentButton3State1;
    }

    // Check if controllerData[2] exists
    if (controllerData.find(2) != controllerData.end()) {
        int currentButton3State2 = controllerData[2].button3;
        // Check for transition from HIGH to LOW
        if (previousButton3State2 == HIGH && currentButton3State2 == LOW) {
            if (currentMillis - lastButton3PressTime2 >= debouncePause) {
                appState = PAUSE_MENU;
                lastButton3PressTime2 = currentMillis; // Update debounce time
                // Update previous state after handling the event
                Serial.println("triggered");
            }
        }
        // Update previous state
        previousButton3State2 = currentButton3State2;
    }
}

// Pong Game Logic
void runPong(bool isSinglePlayer) {
    unsigned long currentMillis = millis();
    checkPause();
    if (gameState == GAME_OVER) {
        displayWinMessage(playerScore >= maxScore ? "Player 1 Wins!" : isSinglePlayer ? "AI Wins!" : "Player 2 Wins!");
        if (controllerData[1].button2 == LOW) {  // Button press to return to menu
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

    // Update the ball's position
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Update ball's previous position
        prevBallX = ballX;
        prevBallY = ballY;

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
        drawGame(isSinglePlayer);
    }
}

// Handle player inputs for both single-player and two-player mode
void handlePlayerInputs(bool isSinglePlayer) {
  // Controller 1: Player 1 Paddle
  if (controllerData[1].command1 & COMMAND_UP) {
    playerPaddleY -= playerSpeed;
    if (playerPaddleY < 0) playerPaddleY = 0;
  }
  if (controllerData[1].command1 & COMMAND_DOWN) {
    playerPaddleY += playerSpeed;
    if (playerPaddleY + paddleHeight > gfx.height()) playerPaddleY = gfx.height() - paddleHeight;
  }

  if (isSinglePlayer) {
      // AI Logic with Proportional Control
      float targetY = ballY - paddleHeight / 2;
      float deltaY = targetY - aiPaddleY;

      // Limit the AI paddle's speed
      if (deltaY > aiSpeed) deltaY = aiSpeed;
      if (deltaY < -aiSpeed) deltaY = -aiSpeed;

      aiPaddleY += deltaY;

      // Boundary checks
      if (aiPaddleY < 0) aiPaddleY = 0;
      if (aiPaddleY + paddleHeight > gfx.height()) aiPaddleY = gfx.height() - paddleHeight;
  } else {
    // Controller 2: Player 2 Paddle
    if (controllerMap.size() >= 2) {
      if ((controllerData[2].button1 == -1) && controllerData.find(2) != controllerData.end()) {
        aiPaddleY -= playerSpeed;
        if (aiPaddleY < 0) aiPaddleY = 0;
      }
      if ((controllerData[2].button1 == 1) && controllerData.find(2) != controllerData.end()) {
        aiPaddleY += playerSpeed;
        if (aiPaddleY + paddleHeight > gfx.height()) aiPaddleY = gfx.height() - paddleHeight;
      }
    }
  }
}

// Function to setup serve
void setupServe(bool isSinglePlayer) {
    // Always draw the serve screen
    gfx.fillScreen(blackColor);
    drawCenterLine();
    displayScores(isSinglePlayer);

    // Draw paddles
    gfx.fillRect(playerPaddleX, playerPaddleY, paddleWidth, paddleHeight, paddleColor);
    gfx.fillRect(aiPaddleX, aiPaddleY, paddleWidth, paddleHeight, paddleColor);

    // Draw ball
    gfx.fillCircle(ballX, ballY, ballRadius, ballColor);

    // Display "Press Select to Serve"
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(1);
    int textWidth = calculateTextWidth("Press Select to Serve", 1);
    gfx.setCursor((screenWidth - textWidth) / 2, screenHeight / 2 - 10);
    gfx.println("Press Select to Serve");

    // Wait for player input to start the serve
    if (controllerData[1].button2 == LOW) {
        serve = false;
        float angle = (random(-30, 30)) * PI / 180.0;
        ballSpeedY = ballSpeed * sin(angle);
        ballSpeedX = playerServe ? ballSpeed * cos(angle) : -ballSpeed * cos(angle);
    }
}


// Check for paddle collisions
void checkPaddleCollisions() {
    // AI Paddle Rectangle
    paddleLeft = aiPaddleX;
    paddleRight = aiPaddleX + paddleWidth;
    paddleTop = aiPaddleY;
    paddleBottom = aiPaddleY + paddleHeight;

    // Check for collision with AI paddle
    if (lineIntersectsRect(prevBallX, prevBallY, ballX, ballY, paddleLeft, paddleTop, paddleRight, paddleBottom)) {
        // Handle collision
        handlePaddleCollision(aiPaddleX, aiPaddleY, false);
        collisionOccurred = true;
        collisionCooldown = millis();
    }


    // Define the line segment of the ball's movement
    ballDeltaX = ballX - prevBallX;
    ballDeltaY = ballY - prevBallY;

    // Player Paddle Rectangle
    paddleLeft = playerPaddleX;
    paddleRight = playerPaddleX + paddleWidth;
    paddleTop = playerPaddleY;
    paddleBottom = playerPaddleY + paddleHeight;

    // Check for collision with player paddle
    if (lineIntersectsRect(prevBallX, prevBallY, ballX, ballY, paddleLeft, paddleTop, paddleRight, paddleBottom)) {
        // Handle collision
        handlePaddleCollision(playerPaddleX, playerPaddleY, true);
        collisionOccurred = true;
        collisionCooldown = millis();
    }
  }

// Draw the game elements
void drawGame(bool isSinglePlayer) {
  // Clear the previous frame
  gfx.fillScreen(blackColor);

  // Draw center line and scores
  drawCenterLine();
  displayScores(isSinglePlayer);

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
void displayScores(bool isSinglePlayer) {
  gfx.setTextSize(1);
  gfx.setTextColor(whiteColor);

  // Player Score
  gfx.setCursor(mode.hRes / 2 - 60, 10);
  gfx.print("P1: ");
  gfx.print(playerScore);

  // AI or Player 2 Score
  gfx.setCursor(mode.hRes / 2 + 10, 10);
  if (!isSinglePlayer) {
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
    if (controllerData[1].button2  == LOW) {
      appState = NAME_ENTRY;
    }
  } else {
    gfx.setTextSize(1);
    gfx.setCursor(mode.hRes / 2 - 40, mode.vRes / 2 + 10);
    gfx.print("Press Select");
    gfx.setCursor(mode.hRes / 2 - 20, mode.vRes / 2 + 20);
    gfx.print("to return");
  }
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
  playSong(snakeSong);
  unsigned long currentMillis = millis();
  checkPause();

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
      if (controllerData[1].button2 == LOW) {
        appState = NAME_ENTRY;
      }
    } else {
      gfx.setCursor(screenWidth / 2 - 70, screenHeight / 2 + 20);
      gfx.print("Press Select to return");
      if (controllerData[1].button2 == LOW) {
        appState = MENU_MAIN;
        displayMenuMain();
        snakeGameInitialized = false;
      }
    }
    return;
  }

  // Always handle input to update direction
  handleSnakeInput();

  // Update game logic at intervals
  if (currentMillis - lastMoveTime >= snakeSpeed) {
    lastMoveTime = currentMillis;
    updateSnake();
    checkSnakeCollision();
  }

  // Always draw the game every frame
  drawSnakeGame();

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
  if (controllerData[1].command1 & COMMAND_UP && snakeDirection != COMMAND_DOWN) {
    newDirection = COMMAND_UP;
  }
  else if (controllerData[1].command1 & COMMAND_DOWN && snakeDirection != COMMAND_UP) {
    newDirection = COMMAND_DOWN;
  }
  else if (controllerData[1].command1 & COMMAND_LEFT && snakeDirection != COMMAND_RIGHT) {
    newDirection = COMMAND_LEFT;
  }
  else if (controllerData[1].command1 & COMMAND_RIGHT && snakeDirection != COMMAND_LEFT) {
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
    // Clear the play area
    gfx.fillRect(0, 20, screenWidth, screenHeight - 20, blackColor); // Clear play area

    // Draw the entire snake
    for (const auto& segment : snake) {
        gfx.fillRect(segment.x, segment.y, gridSize, gridSize, snakeColor);
    }

    // Draw food
    gfx.fillRect(foodX, foodY, gridSize, gridSize, foodColor);

    // Draw score (update as needed)
    gfx.fillRect(0, 0, screenWidth, 20, blackColor); // Clear score area
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
  fbPipeGapSize = 70;

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
  unsigned long currentMillis = millis();
  checkPause();

  if (!fbGameInitialized) {
    resetFlappyBirdGame(isSinglePlayer);
  }

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

  static float previousKalmanAnglePitch = 0.0f;
  const float pitchThreshold = 120.0f; // Adjust this threshold as needed

  // Process input
  if (controllerData[1].button2 == LOW) { // Use Select button to jump
    fbBirdSpeedY = fbJumpStrength;
  }

  Serial.println(controllerData[1].mykalmanAnglePitch);

  // Calculate the change in pitch
  float deltaPitch = controllerData[1].mykalmanAnglePitch - previousKalmanAnglePitch;

  // Check if the change exceeds the threshold
  if (fabs(deltaPitch) > pitchThreshold) {
      Serial.println("gyro active");
      fbBirdSpeedY = fbJumpStrength;
  }

  // Update the previous pitch value
  previousKalmanAnglePitch = controllerData[1].mykalmanAnglePitch;

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
      if (controllerData[1].button2 == LOW) {
        appState = NAME_ENTRY;
      }
    } else {
      gfx.setTextSize(1);
      gfx.setCursor(fbScreenWidth / 2 - 80, fbScreenHeight / 2 + 10);
      gfx.print("Press Select to return");
      if (controllerData[1].button2 == LOW) {
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
  if (fbPlayer1Alive && controllerData[1].button2 == LOW) {
    fbBirdSpeedY1 = fbJumpStrength;
  }
  // Process input for Player 2
  if (controllerData.find(2) != controllerData.end()) {
    if (fbPlayer2Alive && controllerData[2].button2 == LOW) {
      fbBirdSpeedY2 = fbJumpStrength;
    }
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
    if (controllerData[1].button2 == LOW || (controllerData[2].button2 == LOW && controllerData.find(2) != controllerData.end())) {
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
  float rotSpeed = 0.1f;  // Rotation speed
  float playerRadius = 0.2f;

  float moveX = 0;
  float moveY = 0;

  // Movement Forward/Backward
  if (controllerData[1].command1 & COMMAND_UP) { // Move forward
    moveX += dirX * moveSpeed;
    moveY += dirY * moveSpeed;
  }
  if (controllerData[1].command1 & COMMAND_DOWN) { // Move backward
    moveX -= dirX * moveSpeed;
    moveY -= dirY * moveSpeed;
  }

  // Strafing Left/Right
  if (controllerData[1].command1 & COMMAND_LEFT) { // Strafe left
    moveX += dirY * moveSpeed;
    moveY -= dirX * moveSpeed;
  }
  if (controllerData[1].command1 & COMMAND_RIGHT) { // Strafe right
    moveX -= dirY * moveSpeed;
    moveY += dirX * moveSpeed;
  }

  // Attempt to move in X direction
  float newPosX = posX + moveX;
  if (isPositionFree(newPosX, posY, playerRadius)) {
    posX = newPosX;
  } else {
    moveX = 0;
  }

  // Attempt to move in Y direction
  float newPosY = posY + moveY;
  if (isPositionFree(posX, newPosY, playerRadius)) {
    posY = newPosY;
  } else {
    moveY = 0;
  }

  // Rotation with command2 remains the same
  if (controllerData[1].command2 & COMMAND_RIGHT) { // Rotate left
    float oldDirX = dirX;
    dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
    dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
    float oldPlaneX = planeX;
    planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
    planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
  }
  if (controllerData[1].command2 & COMMAND_LEFT) { // Rotate right
    float oldDirX = dirX;
    dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
    dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
    float oldPlaneX = planeX;
    planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
    planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
  }

    // Shooting logic for Player 1
  static unsigned long lastShootTime = 0;
  unsigned long currentMillis = millis();
  if ((controllerData[1].button2 == LOW) && (currentMillis - lastShootTime >= shootCooldown)) { // Player 1 shooting
      for (int i = 0; i < maxBullets; i++) {
          if (!bullets[i].active) {
              bullets[i].x = posX;
              bullets[i].y = posY;
              bullets[i].dirX = dirX;
              bullets[i].dirY = dirY;
              bullets[i].active = true;
              bullets[i].shooter = 1;  // Assign Player 2 as the shooter
              lastShootTime = currentMillis; // Update last shoot time
              break;
          }
      }
  }
}

// Collision detection function accounting for player radius
bool isPositionFree(float x, float y, float radius) {
  // Check the four corners of the player's bounding box
  if (worldMap3D[int(y + radius) * mapWidth3D + int(x + radius)] != '.') return false;
  if (worldMap3D[int(y + radius) * mapWidth3D + int(x - radius)] != '.') return false;
  if (worldMap3D[int(y - radius) * mapWidth3D + int(x + radius)] != '.') return false;
  if (worldMap3D[int(y - radius) * mapWidth3D + int(x - radius)] != '.') return false;

  return true;
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

// Function to Draw Each Frame of the 3D Game

void renderSprites(float playerX, float playerY, float dirX, float dirY, float planeX, float planeY, int screenOffset, float ZBuffer[], const std::vector<Sprite>& sprites) {
    int screenWidth = mode.hRes / 2; // Each player's screen width

    for (const auto& sprite : sprites) {
        // Transform sprite position to camera space
        float spriteX = sprite.x - playerX;
        float spriteY = sprite.y - playerY;

        float invDet = 1.0f / (planeX * dirY - dirX * planeY); // Required for correct matrix multiplication

        float transformX = invDet * (dirY * spriteX - dirX * spriteY);
        float transformY = invDet * (-planeY * spriteX + planeX * spriteY);

        if (transformY <= 0.1f) continue; // Sprite is behind the camera

        int spriteScreenX = int((screenWidth / 2) * (1 + transformX / transformY));

        // Parameters for scaling the sprite
        int spriteHeight, spriteWidth;

        // Adjust size based on sprite type
        if (sprite.texture == 0) { // Bullet
            spriteHeight = abs(int(mode.vRes / transformY)) / 4;
            spriteWidth = spriteHeight;
        } else if (sprite.texture == 1) { // Player
            spriteHeight = abs(int(mode.vRes / transformY));
            spriteWidth = spriteHeight / 2;
        }

        int drawStartY = -spriteHeight / 2 + mode.vRes / 2;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = spriteHeight / 2 + mode.vRes / 2;
        if (drawEndY >= mode.vRes) drawEndY = mode.vRes - 1;

        int drawStartX = spriteScreenX - spriteWidth / 2;
        int drawEndX = spriteScreenX + spriteWidth / 2;

        // Ensure drawing coordinates are within the screen boundaries
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= screenWidth) drawEndX = screenWidth - 1;

        // Draw the sprite if it's closer than the wall
        for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
            int bufferIndex = stripe;
            if (bufferIndex >= 0 && bufferIndex < screenWidth) {
                if (transformY < ZBuffer[bufferIndex]) {
                    uint16_t color;

                    if (sprite.texture == 0) { // Bullet
                        color = (sprite.shooter == 1) ? 0x07FF : 0xFFE0;
                    } else if (sprite.texture == 1) { // Player
                        // Determine color based on which screen
                        color = (screenOffset == 0) ? 0x001F : 0xF800; // Blue or Red

                        // Calculate the current pixel's position relative to the sprite
                        float spriteColumn = (float)(stripe - drawStartX) / (drawEndX - drawStartX);

                        // Calculate the height of the sprite at this stripe
                        int spriteHeightCurrent = drawEndY - drawStartY;

                        // Loop over each pixel in the column
                        for (int y = drawStartY; y < drawEndY; y++) {
                            // Calculate the current pixel's position relative to the sprite
                            float spriteRow = (float)(y - drawStartY) / spriteHeightCurrent;

                            if (spriteRow < 0.5f) {
                                // Top half (Circle)
                                float dx = spriteColumn - 0.5f;
                                float dy = spriteRow - 0.25f; // Center the circle in the top half
                                if (dx * dx + dy * dy <= 0.0625f) { // Adjust radius as needed
                                    gfx.drawPixel(screenOffset + stripe, y, color);
                                }
                            } else {
                                // Bottom half (Upside-Down Triangle)
                                float triangleWidth = 1.0f - 2.0f * (spriteRow - 0.5f);
                                if (spriteColumn >= 0.5f - triangleWidth / 2 && spriteColumn <= 0.5f + triangleWidth / 2) {
                                    gfx.drawPixel(screenOffset + stripe, y, color);
                                }
                            }
                        }
                    } else {
                        color = 0xFFFF; // Default color (white)
                    }

                    // For bullets, we still draw vertical lines
                    if (sprite.texture == 0) {
                        gfx.drawFastVLine(screenOffset + stripe, drawStartY, drawEndY - drawStartY, color);
                    }
                }
            }
        }
    }
}

void displayPlayerLives(int lives, int playerHealth, int x, int y) {
    // Use white color for hearts and health bars
    uint16_t heartColor = 0xFFFF; // White color

    // Display hearts
    for (int i = 0; i < lives; i++) {
        int heartX = x + i * 15;  // Space the hearts 15 pixels apart
        drawHeart(heartX, y, heartColor, heartColor);  // Use white for both fill and outline
    }
    
    // Calculate health bar position
    int barWidth = 45;   // The health bar width should match the number of hearts
    int barHeight = 10;          // Height of the health bar
    int healthBarY = y + 10;     // Position the health bar directly under the hearts

    // Draw the white border for the health bar
    gfx.drawRect(x - 5, healthBarY, barWidth, barHeight, heartColor);  // White border

    // Calculate and fill the current health portion of the bar
    int healthWidth = (barWidth * playerHealth) / 8;  // Adjust based on the player's health
    gfx.fillRect(x -5, healthBarY, healthWidth, barHeight, heartColor);  // Fill health bar with white color
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


  // Collect sprites for Player 1
  sprites.clear();

  // Add bullets
  for (int i = 0; i < maxBullets; i++) {
      if (bullets[i].active) {
          Sprite sprite;
          sprite.x = bullets[i].x;
          sprite.y = bullets[i].y;
          sprite.texture = 0; // Bullet
          sprite.shooter = bullets[i].shooter;
          sprites.push_back(sprite);
      }
  }

  // Add other player
  Sprite otherPlayerSprite;
  otherPlayerSprite.x = posX2;
  otherPlayerSprite.y = posY2;
  otherPlayerSprite.texture = 1; // Player
  sprites.push_back(otherPlayerSprite);

  // Calculate distances and sort sprites for Player 1
  for (auto& sprite : sprites) {
      float dx = sprite.x - posX;
      float dy = sprite.y - posY;
      sprite.distance = dx * dx + dy * dy;
  }

  std::sort(sprites.begin(), sprites.end(), [](const Sprite& a, const Sprite& b) {
      return a.distance > b.distance;
  });

  // Render sprites for Player 1
  renderSprites(posX, posY, dirX, dirY, planeX, planeY, 0, ZBuffer1, sprites);


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

  // Collect sprites for Player 2
  sprites.clear();

  // Add bullets
  for (int i = 0; i < maxBullets; i++) {
      if (bullets[i].active) {
          Sprite sprite;
          sprite.x = bullets[i].x;
          sprite.y = bullets[i].y;
          sprite.texture = 0; // Bullet
          sprite.shooter = bullets[i].shooter;
          sprites.push_back(sprite);
      }
  }

  // Add other player
  otherPlayerSprite.x = posX;
  otherPlayerSprite.y = posY;
  otherPlayerSprite.texture = 1; // Player
  sprites.push_back(otherPlayerSprite);

  // Calculate distances and sort sprites for Player 2
  for (auto& sprite : sprites) {
      float dx = sprite.x - posX2;
      float dy = sprite.y - posY2;
      sprite.distance = dx * dx + dy * dy;
  }

  std::sort(sprites.begin(), sprites.end(), [](const Sprite& a, const Sprite& b) {
      return a.distance > b.distance;
  });

  // Render sprites for Player 2
  renderSprites(posX2, posY2, dirX2, dirY2, planeX2, planeY2, mode.hRes / 2, ZBuffer2, sprites);

  // Draw dividing line
  gfx.drawFastVLine(mode.hRes / 2, 0, mode.vRes, 0xFFFF);  // White line

  // Draw minimaps and other UI elements as before
  drawMinimap(posX, posY, redColor, mode.hRes / 2 - minimapSize - 10, 10); // Player 1 minimap
  drawMinimap(posX2, posY2, blueColor, mode.hRes - minimapSize - 10, 10); // Player 2 minimap

  displayPlayerLives(player1Lives, player1Health, mode.hRes / 2 - minimapSize, minimapSize + 20);  // Player 1
  displayPlayerLives(player2Lives, player2Health, mode.hRes - minimapSize, minimapSize + 20);  // Player 2


  gfx.drawFastVLine(mode.hRes / 2, 0, mode.vRes, 0xFFFF);  // 0xFFFF is the color for white
}

void drawHeart(int x, int y, uint16_t fillColor, uint16_t outlineColor) {
    // Simple heart shape using triangles and circles
    int size = 10;  // Adjust the size as needed

    // Left half-circle
    gfx.fillCircle(x - size / 4, y, size / 4, fillColor);
    gfx.drawCircle(x - size / 4, y, size / 4, outlineColor);

    // Right half-circle
    gfx.fillCircle(x + size / 4, y, size / 4, fillColor);
    gfx.drawCircle(x + size / 4, y, size / 4, outlineColor);

    // Bottom triangle
    int x0 = x - size / 2;
    int y0 = y;
    int x1 = x + size / 2;
    int y1 = y;
    int x2 = x;
    int y2 = y + size / 2;

    gfx.fillTriangle(x0, y0, x1, y1, x2, y2, fillColor);
    gfx.drawLine(x0, y0, x2, y2, outlineColor);
    gfx.drawLine(x1, y1, x2, y2, outlineColor);
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

        // Adjust the sprite's width and height for a 1x2 ratio
    int spriteHeight = abs(int(mode.vRes / transformY));  // Full height
    int spriteWidth = spriteHeight / 2;                   // Half the height

    int drawStartY = -spriteHeight / 2 + mode.vRes / 2;
    int drawEndY = spriteHeight / 2 + mode.vRes / 2;
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
  checkPause();

  if (gameState3D == GAME_OVER_3D) {
    display3DGameOverScreen();
    if ((controllerData[1].button2 == LOW) || ((controllerData[2].button2 == LOW) && controllerData.find(2) != controllerData.end())) {  // Use Select button to return to menu
      delay(200);
      appState = MENU_MAIN;
      displayMenuMain();
      return;
    }
  } else {
    // Handle inputs for both players
    handle3DGameInputs();
    handle3DGameInputsPlayer2();

    updateBullets();

    // Play the song
    // playSong(DoomSong);

    // Check if Player 1 lost a life
    if (player1Health <= 0) {
      player1Lives--;
      if (player1Lives > 0) {
        // Reset the maze, player positions, and health when a life is lost
        generateRandomMaze();          // Reset to a new maze
        resetPlayerPositions();        // Reset players' positions
        player1Health = 8;             // Reset player health to 8
        player2Health = 8;             // Also reset Player 2's health
      } else {
        gameState3D = GAME_OVER_3D;
      }
    }

    // Check if Player 2 lost a life
    if (player2Health <= 0) {
      player2Lives--;
      if (player2Lives > 0) {
        // Reset the maze, player positions, and health when a life is lost
        generateRandomMaze();          // Reset to a new maze
        resetPlayerPositions();        // Reset players' positions
        player1Health = 8;             // Also reset Player 1's health
        player2Health = 8;             // Reset player health to 8
      } else {
        gameState3D = GAME_OVER_3D;
      }
    }

    // Update and draw frame at regular intervals
    if (currentMillis - previousMillis3D >= interval3D) {
      previousMillis3D = currentMillis;
      drawFrame3DTwoPlayer();
    }
  }
}

void resetPlayerPositions() {
    // Reset Player 1 position to the top-left corner of the map
    posX = 1.5f;   // Player 1 X position near the top-left corner
    posY = 1.5f;   // Player 1 Y position near the top-left corner
    dirX = 1.0f;   // Player 1 facing right
    dirY = 0.0f;
    planeX = 0.0f;
    planeY = 0.66f;

    // Reset Player 2 position to the bottom-right corner of the map
    posX2 = mapWidth3D - 2.5f;   // Player 2 X position near the bottom-right corner
    posY2 = mapHeight3D - 2.5f;  // Player 2 Y position near the bottom-right corner
    dirX2 = -1.0f;   // Player 2 facing left
    dirY2 = 0.0f;
    planeX2 = 0.0f;
    planeY2 = 0.66f;
}

// Function to handle inputs for Player 2
void handle3DGameInputsPlayer2() {
  if (controllerData.find(2) != controllerData.end()) {
    float moveSpeed = 0.05f; // Movement speed
    float rotSpeed = 0.1f;  // Rotation speed

    // Movement Forward/Backward for Player 2
    if (controllerData[2].command1 & COMMAND_UP) { // Move forward
      float newPosX = posX2 + dirX2 * moveSpeed;
      float newPosY = posY2 + dirY2 * moveSpeed;
      if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
        posX2 = newPosX;
        posY2 = newPosY;
      }
    }
    if (controllerData[2].command1 & COMMAND_DOWN) { // Move backward
      float newPosX = posX2 - dirX2 * moveSpeed;
      float newPosY = posY2 - dirY2 * moveSpeed;
      if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
        posX2 = newPosX;
        posY2 = newPosY;
      }
    }

    // Strafing Left/Right for Player 2
    if (controllerData[2].command1 & COMMAND_LEFT) { // Strafe left
      float newPosX = posX2 - dirY2 * moveSpeed;
      float newPosY = posY2 + dirX2 * moveSpeed;
      if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
        posX2 = newPosX;
        posY2 = newPosY;
      }
    }
    if (controllerData[2].command1 & COMMAND_RIGHT) { // Strafe right
      float newPosX = posX2 + dirY2 * moveSpeed;
      float newPosY = posY2 - dirX2 * moveSpeed;
      if (worldMap3D[int(newPosY) * mapWidth3D + int(newPosX)] == '.') {
        posX2 = newPosX;
        posY2 = newPosY;
      }
    }

    // Rotation with command2
    if (controllerData[2].command2 & COMMAND_LEFT) { // Rotate left
      float oldDirX = dirX2;
      dirX2 = dirX2 * cos(rotSpeed) - dirY2 * sin(rotSpeed);
      dirY2 = oldDirX * sin(rotSpeed) + dirY2 * cos(rotSpeed);
      float oldPlaneX = planeX2;
      planeX2 = planeX2 * cos(rotSpeed) - planeY2 * sin(rotSpeed);
      planeY2 = oldPlaneX * sin(rotSpeed) + planeY2 * cos(rotSpeed);
    }
    if (controllerData[2].command2 & COMMAND_RIGHT) { // Rotate right
      float oldDirX = dirX2;
      dirX2 = dirX2 * cos(-rotSpeed) - dirY2 * sin(-rotSpeed);
      dirY2 = oldDirX * sin(-rotSpeed) + dirY2 * cos(-rotSpeed);
      float oldPlaneX = planeX2;
      planeX2 = planeX2 * cos(-rotSpeed) - planeY2 * sin(-rotSpeed);
      planeY2 = oldPlaneX * sin(-rotSpeed) + planeY2 * cos(-rotSpeed);
    }

    // Shooting logic for Player 2
    static unsigned long lastShootTimeP2 = 0;
    unsigned long currentMillis = millis();
    if ((controllerData[2].button2 == LOW) && (currentMillis - lastShootTimeP2 >= shootCooldown)) { // Player 2 shooting
        for (int i = 0; i < maxBullets; i++) {
            if (!bullets[i].active) {
                bullets[i].x = posX2;
                bullets[i].y = posY2;
                bullets[i].dirX = dirX2;
                bullets[i].dirY = dirY2;
                bullets[i].active = true;
                bullets[i].shooter = 2;  // Assign Player 2 as the shooter
                lastShootTimeP2 = currentMillis; // Update last shoot time
                break;
            }
        }
    }
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

  if (player1Lives <= 0 && player2Lives <= 0) {
      gfx.print("It's a Tie!");
  } else if (player1Lives <= 0) {
      gfx.print("Player 2 Wins!");
  } else if (player2Lives <= 0) {
      gfx.print("Player 1 Wins!");
  }

  gfx.setCursor(mode.hRes / 2 - 70, mode.vRes / 2 + 30);
  gfx.print("Press Select to return");

  // Reset health and lives when game ends
  player1Health = 8;
  player2Health = 8;
  player1Lives = 3;
  player2Lives = 3;
}

// Function to Render Bullets
void renderBullets(float playerX, float playerY, float dirX, float dirY, float planeX, float planeY, int screenOffset, float ZBuffer[]) {
    int screenWidth = mode.hRes / 2; // Each player's screen width
    for (int i = 0; i < maxBullets; i++) {
        if (bullets[i].active) {
            // Transform bullet position to camera space
            float bulletX = bullets[i].x - playerX;
            float bulletY = bullets[i].y - playerY;
            float invDet = 1.0f / (planeX * dirY - dirX * planeY);
            float transformX = invDet * (dirY * bulletX - dirX * bulletY);
            float transformY = invDet * (-planeY * bulletX + planeX * bulletY);

            if (transformY <= 0.1f) continue; // Bullet behind the player or too close

            int bulletScreenX = int((screenWidth / 2) * (1 + transformX / transformY));

            // Bullet dimensions
            int bulletHeight = abs(int(mode.vRes / transformY)) / 4; // Adjust size as needed
            int drawStartY = -bulletHeight / 2 + mode.vRes / 2;
            if (drawStartY < 0) drawStartY = 0;
            int drawEndY = bulletHeight / 2 + mode.vRes / 2;
            if (drawEndY >= mode.vRes) drawEndY = mode.vRes - 1;

            int bulletWidth = bulletHeight;
            int drawStartX = bulletScreenX - bulletWidth / 2;
            int drawEndX = bulletScreenX + bulletWidth / 2;

            // Ensure drawing coordinates are within the screen boundaries
            if (drawStartX < 0) drawStartX = 0;
            if (drawEndX >= screenWidth) drawEndX = screenWidth - 1;

            // Draw the bullet if it's closer than the wall
            for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
                int bufferIndex = stripe;
                if (bufferIndex >= 0 && bufferIndex < screenWidth) {
                    if (transformY < ZBuffer[bufferIndex]) {
                        uint16_t bulletColor = (bullets[i].shooter == 1) ? 0xFFE0 : 0xF81F; // Yellow for Player 1, Pink for Player 2
                        gfx.drawFastVLine(screenOffset + stripe, drawStartY, drawEndY - drawStartY, bulletColor);
                    }
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

    // Static variables to store previous button states
    static int previousButton1State = HIGH;
    static int previousButton2State = HIGH;
    static int previousButton3State = HIGH;

    unsigned long currentMillis = millis();

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
    gfx.print("Use Buttons to change");
    gfx.setCursor(10, screenHeight - 10);
    gfx.print("Select to confirm");

    // Get current button states
    int currentButton1State = controllerData[1].button1;
    int currentButton2State = controllerData[1].button2;
    int currentButton3State = controllerData[1].button3;

    // Handle character decrement (Button 1)
    if (currentButton1State == LOW && previousButton1State == HIGH && (currentMillis - lastButton1PressTime >= debounceDelay)) {
        // Button 1 was just pressed
        if (inputName[charIndex] == '_') {
            inputName[charIndex] = 'Z';
        } else if (inputName[charIndex] == 'A') {
            inputName[charIndex] = '_';
        } else {
            inputName[charIndex]--;
        }
        lastButton1PressTime = currentMillis;
    }

    // Handle character increment (Button 3)
    if (currentButton3State == LOW && previousButton3State == HIGH && (currentMillis - lastButton3PressTime >= debounceDelay)) {
        // Button 3 was just pressed
        if (inputName[charIndex] == '_') {
            inputName[charIndex] = 'A';
        } else if (inputName[charIndex] == 'Z') {
            inputName[charIndex] = '_';
        } else {
            inputName[charIndex]++;
        }
        lastButton3PressTime = currentMillis;
    }

    // Handle moving to next character (Button 2)
    if (currentButton2State == LOW && previousButton2State == HIGH && (currentMillis - lastButton2PressTime >= debounceDelay)) {
        // Button 2 was just pressed
        if (charIndex < 2) {
            charIndex++;
        } else {
            // Name entry complete
            strcpy(playerName, inputName);
            nameEntered = true;
            charIndex = 0;
            memset(inputName, '_', 3);
        }
        lastButton2PressTime = currentMillis;
    }

    // Update previous button states
    previousButton1State = currentButton1State;
    previousButton2State = currentButton2State;
    previousButton3State = currentButton3State;

    if (nameEntered) {
        // Validate and save name
        saveHighScore(selectedGame, playerName);
        nameEntered = false;
        appState = MENU_MAIN;
        // Reset game variables as needed
        snakeGameInitialized = false;
        snakeGameOver = false;
        snakeScore = 0;
        newSnakeHighScore = false;
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
  } else if (strcmp(gameName, "Maze Hunter") == 0) {
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

void startSong(Song &song) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        isSongPlaying = true;
        songFinished = false;         // Reset the song finished flag
        noteStartTime = millis();
        currentNoteIndex = 0;
        beatDuration = calculateBeatDuration(song.bpm);
        currentSong = song;            // Set the current song

        // Play the first note or handle rest
        int noteFrequency = song.notes[0][0];
        if (noteFrequency == -1) {
            ledcWriteTone(pwmChannel, 0);  // Mute audio
        } else {
            ledcWriteTone(pwmChannel, noteFrequency);
            ledcWrite(pwmChannel, volumeLevel); // Set initial volume
        }

        xSemaphoreGive(xSemaphore);
    }
}

void stopSong() {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        isSongPlaying = false;
        ledcWriteTone(pwmChannel, 0);  // Stop the tone
        ledcWrite(pwmChannel, 0);      // Set duty cycle to 0
        xSemaphoreGive(xSemaphore);
    }
}

// Function to play the song (non-blocking)
void playSong(Song &song) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        if (!isSongPlaying) {
            xSemaphoreGive(xSemaphore);
            vTaskDelay(10);
            return;
        }

        unsigned long currentTime = millis();
        float durationInBeats = song.notes[currentNoteIndex][1];
        unsigned long noteDuration = beatDuration * durationInBeats;

        if (currentTime - noteStartTime >= noteDuration) {
            // Move to the next note
            currentNoteIndex++;
            if (currentNoteIndex >= song.length) {
                if (song.loop) {
                    currentNoteIndex = 0;  // Restart the song
                } else {
                    isSongPlaying = false; // Stop playing
                    songFinished = true;   // Indicate the song has finished
                    ledcWriteTone(pwmChannel, 0); // Mute audio
                    xSemaphoreGive(xSemaphore);
                    return;
                }
            }

            // Play the next note or handle rest
            int noteFrequency = song.notes[currentNoteIndex][0];
            if (noteFrequency == -1) {
                // It's a rest, stop the tone for the rest duration
                ledcWriteTone(pwmChannel, 0); // Mute audio
            } else {
                // It's a note, play the note
                ledcWriteTone(pwmChannel, noteFrequency);
                ledcWrite(pwmChannel, volumeLevel); // Set volume
            }

            noteStartTime = currentTime;
        }
        xSemaphoreGive(xSemaphore);
    }
    vTaskDelay(1); // Yield to other tasks
}

// Function to calculate the duration of a beat in milliseconds based on BPM
int calculateBeatDuration(int bpm) {
  return 60000 / bpm;  // 60000 ms in a minute
}

void displayPauseMenu() {
    gfx.fillScreen(blackColor);
    gfx.setTextColor(whiteColor);
    int textSize = 2;
    gfx.setTextSize(textSize);

    // Header
    gfx.setCursor(10, 20);
    gfx.print("Game Paused");

    // Menu Items
    for (int i = 0; i < totalPauseMenuItems; i++) {
        if (i == currentPauseMenuItem) {
            gfx.setTextColor(highlightColor); // Highlight the selected item
        } else {
            gfx.setTextColor(whiteColor);
        }

        // Calculate cursor position
        int cursorX = 20;
        int cursorY = 60 + i * 30;

        gfx.setCursor(cursorX, cursorY);
        gfx.println(pauseMenuItems[i]);
    }

    // Display current volume level
    gfx.setTextColor(whiteColor);
    gfx.setTextSize(1);
    gfx.setCursor(10, screenHeight - 20);
    gfx.print("Volume: ");
    gfx.print(volumeLevel);
}

void audioTask(void *parameter) {
    while (1) {
        playSong(currentSong); // Replace with the song you're playing
        vTaskDelay(1); // Yield to other tasks
    }
}

void adjustVolume(int change) {
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        volumeLevel += change;
        if (volumeLevel > maxVolume) volumeLevel = maxVolume;
        if (volumeLevel < minVolume) volumeLevel = minVolume;
        ledcWrite(pwmChannel, volumeLevel); // Update volume
        xSemaphoreGive(xSemaphore);
    }
}

bool lineIntersectsRect(float x1, float y1, float x2, float y2, float rectLeft, float rectTop, float rectRight, float rectBottom) {
    // Check if either end is inside the rectangle
    if ((x1 >= rectLeft && x1 <= rectRight && y1 >= rectTop && y1 <= rectBottom) ||
        (x2 >= rectLeft && x2 <= rectRight && y2 >= rectTop && y2 <= rectBottom)) {
        return true;
    }

    // Check for intersection with each side of the rectangle
    if (lineIntersectsLine(x1, y1, x2, y2, rectLeft, rectTop, rectRight, rectTop)) return true;    // Top
    if (lineIntersectsLine(x1, y1, x2, y2, rectLeft, rectBottom, rectRight, rectBottom)) return true; // Bottom
    if (lineIntersectsLine(x1, y1, x2, y2, rectLeft, rectTop, rectLeft, rectBottom)) return true;   // Left
    if (lineIntersectsLine(x1, y1, x2, y2, rectRight, rectTop, rectRight, rectBottom)) return true;  // Right

    return false;
}

bool lineIntersectsLine(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
    float denom = (y4 - y3)*(x2 - x1) - (x4 - x3)*(y2 - y1);
    if (denom == 0.0f) return false; // Lines are parallel

    float ua = ((x4 - x3)*(y1 - y3) - (y4 - y3)*(x1 - x3)) / denom;
    float ub = ((x2 - x1)*(y1 - y3) - (y2 - y1)*(x1 - x3)) / denom;

    return (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f);
}

void updateBullets() {
    float bulletSpeed = 0.2f;
    for (int i = 0; i < maxBullets; i++) {
        if (bullets[i].active) {
            bullets[i].x += bullets[i].dirX * bulletSpeed;
            bullets[i].y += bullets[i].dirY * bulletSpeed;

            // Check if bullet hits a wall
            if (worldMap3D[int(bullets[i].y) * mapWidth3D + int(bullets[i].x)] == '#') {
                bullets[i].active = false;
            }

            // Check bullet collision with Player 1
            if (bullets[i].shooter == 2) {  // Only check if Player 2 shot the bullet
                float distanceToPlayer1 = sqrt(pow(bullets[i].x - posX, 2) + pow(bullets[i].y - posY, 2));
                if (distanceToPlayer1 < 0.5f && bullets[i].active) {
                    bullets[i].active = false;
                    player1Health--;
                    if (player1Health <= 0) {
                        player1Lives--;
                        if (player1Lives <= 0) gameState3D = GAME_OVER_3D;  // Player 1 loses
                        player1Health = 8;  // Reset health
                    }
                }
            }

            // Check bullet collision with Player 2
            if (bullets[i].shooter == 1) {  // Only check if Player 1 shot the bullet
                float distanceToPlayer2 = sqrt(pow(bullets[i].x - posX2, 2) + pow(bullets[i].y - posY2, 2));
                if (distanceToPlayer2 < 0.5f && bullets[i].active) {
                    bullets[i].active = false;
                    player2Health--;
                    if (player2Health <= 0) {
                        player2Lives--;
                        if (player2Lives <= 0) gameState3D = GAME_OVER_3D;  // Player 2 loses
                        player2Health = 8;  // Reset health
                    }
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
