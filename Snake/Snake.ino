#include <TFT_eSPI.h>
#include "back.h"
#include "gameOver.h"
#include "newGame.h"
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

#define left 0
#define right 14
///////////////////////////////////////////////JOYSTICK////////////////////////////////////////////////////////////////////////
const int buttonPin = 11;

#define VRX_PIN  13 // Arduino pin connected to VRX pin
#define VRY_PIN  12 // Arduino pin connected to VRY pin
#define BUT_PIN 10
#define VIBRATION_PIN 3

// #define LEFT_THRESHOLD  250
// #define RIGHT_THRESHOLD 412
// #define UP_THRESHOLD    412
// #define DOWN_THRESHOLD  250
#define LEFT_THRESHOLD  270
#define RIGHT_THRESHOLD 370
#define UP_THRESHOLD    370
#define DOWN_THRESHOLD  270

#define COMMAND_NO     0x00
#define COMMAND_LEFT   0x01
#define COMMAND_RIGHT  0x02
#define COMMAND_UP     0x04
#define COMMAND_DOWN   0x08

float xValue = 0.0 ; // To store value of the X axis
float yValue = 0.0 ; // To store value of the Y axis
int bValue = 0;
int command = COMMAND_NO; // This is the default command (none)
float ratio = 0.183;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int size=1;
int y[120]={0};
int x[120]={0};

unsigned long currentTime=0;
int period=200;
int deb,deb2=0;
int dirX=1;
int dirY=0;
bool taken=0;
unsigned short colors[2]={0x48ED,0x590F}; //terain colors
unsigned short snakeColor[2]={0x9FD3,0x38C9};
bool chosen=0;
bool gOver=0;
int moves=0;
int playTime=0;
int foodX=0;
int foodY=0;
int howHard=0;
String diff[3]={"EASY","NORMAL","HARD"};
bool ready=1;
long readyTime=0;

void setup() {  //.......................setup
    /////////////////////////////////////////////////////////////////////JOYSTICK/////////////////////////////////////////////////////////
    pinMode(buttonPin, INPUT);
    pinMode(VRX_PIN, INPUT);
    pinMode(VRY_PIN, INPUT);
    pinMode(BUT_PIN, INPUT);
    pinMode(VIBRATION_PIN, OUTPUT);
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    pinMode(left,INPUT_PULLUP);
    pinMode(right,INPUT_PULLUP);
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    tft.pushImage(0,0,170,320,back);
    tft.pushImage(0,30,170,170,newGame);
    
    
  
  
    tft.setTextColor(TFT_PURPLE,0x7DFD);
    tft.fillSmoothCircle(28,102+(howHard*24),5,TFT_RED,TFT_BLACK); 
    tft.drawString("DIFFICULTY:   "+ diff[howHard]+"   ",26,267); 
   
    sprite.createSprite(170,170);
    sprite.setSwapBytes(true);

    // while(digitalRead(right)==1)
    //      {
    //        if(digitalRead(left)==0)
    //        {
    //          if(deb2==0)
    //            {
    //            deb2=1; 
    //            tft.fillCircle(28,102+(howHard*24),6,TFT_BLACK);   
    //            howHard++;   if(howHard==3) howHard=0;  
    //            tft.fillSmoothCircle(28,102+(howHard*24),5,TFT_RED,TFT_BLACK);
    //            tft.drawString("DIFFICULTY:   "+ diff[howHard]+"   ",26,267);  
    //            period=200-howHard*20;  
    //            delay(200); 
    //            }             

    //        }else deb2=0;
    //      }

      /////////////////////////////////WHILE LOOP NEW//////////////////////////////////////////////////////////////////////////////////////
      while(!digitalRead(buttonPin))
         {
           if(digitalRead(left)==0)
           {
             if(deb2==0)
               {
               deb2=1; 
               tft.fillCircle(28,102+(howHard*24),6,TFT_BLACK);   
               howHard++;   if(howHard==3) howHard=0;  
               tft.fillSmoothCircle(28,102+(howHard*24),5,TFT_RED,TFT_BLACK);
               tft.drawString("DIFFICULTY:   "+ diff[howHard]+"   ",26,267);  
               period=200-howHard*20;  
               delay(200); 
               }             

           }else deb2=0;
         }
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    y[0]=random(5,15);
    getFood();
    tft.setTextSize(3);
    tft.setTextDatum(4);
    tft.drawString(String(size),44,250); 
    tft.drawString(String(500-period),124,250);
    delay(400);
    dirX=1;
    dirY=0;
}

void getFood()//.....................getFood -get new position of food
{
    foodX=random(0,17);
    foodY=random(0,17);
    taken=0;
    for(int i=0;i<size;i++)
    if(foodX==x[i] && foodY==y[i])
    
    taken=1;
    if(taken==1)
    getFood();
}

void run()//...............................run function
{

      //Updates the snakes body by updating its body segments to euqal the prior body segment, making the snake appear as if its moving.
      for(int i=size;i>0;i--)
       {
      x[i]=x[i-1];    
      y[i]=y[i-1];     
       }    
    
    //The first body segment (the head) is moved one position in the direction specified by dirx and dir y.
     x[0]=x[0]+dirX;
     y[0]=y[0]+dirY;

    //If snake head touches food, snake body increases by one (size++)
    if(x[0]==foodX && y[0]==foodY)
             {digitalWrite(VIBRATION_PIN, HIGH); delay(100); digitalWrite(VIBRATION_PIN, LOW); size++; getFood(); tft.drawString(String(size),44,250); period=period-1; tft.drawString(String(500-period),124,250);}
     sprite.fillSprite(TFT_BLACK);
     /*
    for(int i=0;i<17;i++)
      for(int j=0;j<17;j++)
        {
        sprite.fillRect(j*10,i*10,10,10,colors[chosen]);
        chosen=!chosen;
        }
     chosen=0;*/
     checkGameOver();
      if(gOver==0){
     sprite.drawRect(0,0,170,170,0x02F3);     
     for(int i=0;i<size;i++){
     sprite.fillRoundRect(x[i]*10,y[i]*10,10,10,2,snakeColor[0]); 
     sprite.fillRoundRect(2+x[i]*10,2+y[i]*10,6,6,2,snakeColor[1]); 
     }
     sprite.fillRoundRect(foodX*10+1,foodY*10+1,8,8,1,TFT_RED); 
     sprite.fillRoundRect(foodX*10+3,foodY*10+3,4,4,1,0xFE18); 
    }
else    

{sprite.pushImage(0,0,170,170,gameOver);}    
 sprite.pushSprite(0,30);
 
}     


int change=0;

void checkGameOver()//..,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,check game over
{
if(x[0]<0 || x[0]>=17 || y[0]<0 || y[0]>=17 )
gOver=true;
for(int i=1;i<size;i++)
if(x[i]==x[0] && y[i]==y[0])
gOver=true;
}

void loop() { //...............................................................loop
  
// if(millis()>currentTime+period) 
// {run(); currentTime=millis();} 

// //Set ready variable based on ready time as don't want commands to be too cloes in time together.
// if(millis()>readyTime+100 && ready==0) 
// {ready=1;} 

// if(ready==1){
// if(digitalRead(left)==0){

  //deb variable is for debounce. Basically once button has been pressed change the flag to 1 so that the condition block doesn't
  //occur anymore on subsequent button bounces.
  //I guess change variable is used to make sure only 1 change happens at a time (pretty sure the other flags do that too but idk)
//   if(deb==0)
//   {deb=1;
//   if(dirX==1 && change==0) {dirY=dirX*-1; dirX=0; change=1;}
//   if(dirX==-1 && change==0) {dirY=dirX*-1; dirX=0;change=1; }
//   if(dirY==1 && change==0) {dirX=dirY*1; dirY=0; change=1;}
//   if(dirY==-1 && change==0) {dirX=dirY*1; dirY=0; change=1;}
//   change=0;
//   ready=0;
//   readyTime=millis();
//   }
// }else{ deb=0;}}

// if(ready==1){
// if(digitalRead(right)==0)
// {
   
//   if(deb2==0)
//   {deb2=1;
//    if(dirX==1 && change==0) {dirY=dirX*1; dirX=0; change=1;}
//    if(dirX==-1 && change==0) {dirY=dirX*1; dirX=0;change=1; }
//    if(dirY==1 && change==0) {dirX=dirY*-1; dirY=0; change=1;}
//    if(dirY==-1 && change==0) {dirX=dirY*-1; dirY=0; change=1;}
//   change=0;
//   ready=0;
//   readyTime=millis();
//   }
// }else {deb2=0;}}

/////////////////////////////////////////////////JOYSTICK////////////////////////////////////////////////////////////////////////////

    if(millis()>currentTime+period) 
    {run(); currentTime=millis();} 

    //Set ready variable based on ready time as don't want commands to be too cloes in time together.
    if(millis()>readyTime+15 && ready==0) 
    {ready=1;} 

    // read analog X and Y analog values
    xValue = ratio*analogRead(VRX_PIN);
    yValue = ratio*analogRead(VRY_PIN);
    // tft.setCursor(0,40);
    // tft.print("xValue:" + String(xValue));
    // tft.setCursor(0,60);
    // tft.print("yValue" + String(yValue));

    // converts the analog value to commands
    // reset commands
    command = COMMAND_NO;

    // check left/right commands
    if (xValue < LEFT_THRESHOLD)
      //Bitwise OR
      command = command | COMMAND_LEFT;
    else if (xValue > RIGHT_THRESHOLD)
      command = command | COMMAND_RIGHT;

    // check up/down commands
    if (yValue > UP_THRESHOLD)
      command = command | COMMAND_UP;
    else if (yValue < DOWN_THRESHOLD)
      command = command | COMMAND_DOWN;

    // NOTE: AT A TIME, THERE MAY BE NO COMMAND, ONE COMMAND OR TWO COMMANDS

//////////////////////////////////////////////JOYSTICK//////////////////////////////////////////////////////////////////////////
if(ready==1){
  if (change == 0 && command & COMMAND_LEFT) {
    dirY=0;
    dirX=0;
    dirY = dirY;
    dirX = dirX-1;
    change = 1;
  }
  if (change == 0 && command & COMMAND_RIGHT) {
    dirY=0;
    dirX=0;
    dirY = dirY;
    dirX = dirX+1;
    change = 1;
  }
  if (change == 0 && command & COMMAND_UP) {
    dirY=0;
    dirX=0;
    dirY = dirY-1;
    dirX = 0;
    change = 1;
  }
  if (change == 0 && command & COMMAND_DOWN) {
    dirY=0;
    dirX=0;
    dirY = dirY+1;
    dirX = 0;
    change = 1;
  }
}

change=0;
ready=0;
readyTime=millis();

////////////////////////////////////////////////////////////JOYSTICK////////////////////////////////////////////////////////////////////

}
