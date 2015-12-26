//Adafruit LCD Libraries
#include <Wire.h>
#include <utility/Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
//WaveShield Libraries
#include <FatReader.h>
#include <FatStructs.h>
#include <SdInfo.h>
#include <SdReader.h>
#include <WaveHC.h>
#include <Wavemainpage.h>
#include <WaveUtil.h>

// Adafruit LCD Constants
// Adapted from Adafruit rgb lcd shield website
// The rgb lcd shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

//WaveShield Objects
SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're play
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time
#define DEBOUNCE 100  // button debouncer, may not be needed

/*>> Pulse Sensor Amped 1.2 <<
 This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
 <a href="http://www.pulsesensor.com">  www.pulsesensor.com  </a> 
 >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
 Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
 PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
 The following variables are automatically updated:
 Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
 IBI  :      int that holds the time interval between beats. 2mS resolution.
 BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
 QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
 Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.
*/
//Pulse Sensor Variables
int pulsePin = 0;       // Pulse Sensor purple wire connected to analog pin 0 (rgb lcd shield)
int blinkPin = 6;       // Pin to blink led at each beat
int fadeRate = 0;
int heartvals[4];
int h = 0;      
int randNumber = 0;    // Used to hold the random number generated                 
int tempNum = 0;       // Used to compare newly randomly generated number.
bool firstFlag = false;// Indicator to get tempNum initialized once only.

//Volatile because they are used during the interrupt service routine!
volatile int BPM;                   // Pulse rate
volatile int Signal;                // Incoming raw data
volatile int IBI = 600;             // Time between beats, must be seeded! 
volatile boolean Pulse = false;     // True when pulse wave is high, false when it's low
volatile boolean QS = true;        // Becomes true when Arduoino finds a beat.

/////////////////////////////////////////////////////////////////////
// Component: Pulse Sensor 
// Function:  setup()
// Summary:   Checks the SD card. Pauses program if error is found.         
// Params:    None         
// Returns:   Nothing
// Uses:      Nothing 
///////////////////////////////////////////////////////////////////// 
void setup() {
  randomSeed(analogRead(0));
  // Set the output pins for the DAC control. This pins are defined in the library  
  //pinMode( blinkPin, OUTPUT );      // pin that will blink to your heartbeat!  
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);               // enable pull-up resistors on switch pins (analog inputs)
  digitalWrite(14, HIGH);
  digitalWrite(15, HIGH);
  digitalWrite(16, HIGH);
  digitalWrite(17, HIGH);
  digitalWrite(18, HIGH);
  digitalWrite(19, HIGH);
  Serial.begin( 9600 );
  
  if (!card.init()) {     //play with 8 MHz spi (default faster!)  
    putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
    sdErrorCheck();
    while(1);                            // then 'halt' - do nothing!
  }                                      // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);           // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {     // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                             // we found one, lets bail
  }  if (part == 5) {                    // if we ended up not finding one  :(
    putstring_nl("No valid FAT partition!");
    sdErrorCheck();                      // Something went wrong, lets print out why
    while(1);                            // then 'halt' - do nothing!
  }  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?  // Try to open the root directory
  if (!root.openRoot(vol)) {
    putstring_nl("Can't open root dir!"); // Something went wrong,
    while(1);                             // then 'halt' - do nothing!
  }  
  
  // secondary file with code for the Pulse Sensor
  interruptSetup();                       // sets up to read Pulse Sensor signal every 2mS 
  // UN-COMMENT THE NEXT LINE IF YOU ARE POWERING The Pulse Sensor AT LOW VOLTAGE, 
  // AND APPLY THAT VOLTAGE TO THE A-REF PIN
  //analogReference(EXTERNAL);            // set up for LCD
  lcd.begin( 16, 2 );
  lcd.setBacklight( RED );  
  lcd.setCursor( 0, 0 );
  lcd.print( "Initializing" );
  lcd.setCursor( 0, 1 );
  lcd.print( "Arduino HRM." );
  playComplete("Welcome.wav");  
  } 
  // END OF VOID 
  //SETUP uint8_t i=0;
//}

/////////////////////////////////////////////////////////////////////
// Component: Adafruit LCD 
// Function:  loop()
// Summary:   Main loop to watch for button presses.           
// Params:    None         
// Returns:   Nothing
// Uses:      playComplete(), averageH(), BUTTON (all) 
/////////////////////////////////////////////////////////////////////
void loop() { 
  uint8_t buttons = lcd.readButtons();
  if ( buttons ) {
    lcd.clear();
    lcd.setCursor( 0, 0 );    
    
    //UP Button
    if ( buttons & BUTTON_UP ) {
      heartRate();
      //playRandomBit();
    }
    
    //LEFT Button
    if ( buttons & BUTTON_LEFT ) {
      lcd.setCursor( 0, 0 );
      lcd.print( "Last 4 readings: " );
      lcd.setCursor( 0, 1 );
      int i;
      for ( i=0; i<4; i++ ) {
        lcd.print( heartvals[i]);
        lcd.print(" ");
      }
    }

    //RIGHT Button
    if ( buttons & BUTTON_RIGHT ) {

      playRandomBit();
      
    }//RIGHT Button

    //DOWN Button
    if ( buttons & BUTTON_DOWN ) {
      averageH();
    }

    //SELECT Button
    if ( buttons & BUTTON_SELECT ) {
      lcd.print( "Directions" );
      playComplete( "Direct.wav" );
    }
  }
}//Setup

/////////////////////////////////////////////////////////////////////
// Component: WaveShield
// Function:  playComplete()
// Summary:   Plays a full file from beginning to end with no pause.            
// Params:    *name (char)         
// Returns:   Nothing
// Uses:      playFile()
/////////////////////////////////////////////////////////////////////
void playComplete(char *name) {
  playFile(name);
  while (wave.isplaying) {
    // do nothing while its playing
  }
}

/////////////////////////////////////////////////////////////////////
// Component: Uno
// Function:  playRandomBit()
// Summary:   Finds the .wav file and plays it otherwise return error.            
// Params:    None         
// Returns:   Nothing
// Uses:      Nothing
/////////////////////////////////////////////////////////////////////
void playRandomBit(){
  if(firstFlag == false){
        randNumber = random( 1,9);
        tempNum = randNumber;
  }
  else if (firstFlag == true){
    randNumber = random( 1,9 );
    while(randNumber == tempNum){
        randNumber = random( 1,9 );  
      }
    tempNum = randNumber;   
  }
      if ( randNumber == 1 ) {
        lcd.setCursor( 0, 0 );
        lcd.print( "Leg day all day." );
        playComplete("Squats.wav");
      }
      if ( randNumber == 2 ) {
        lcd.setCursor( 0, 0 );
        lcd.print( "Ur HR b lyk" );
        playComplete("untz.wav");
      }
      if ( randNumber == 3 ) {
        playComplete("Alive.wav");
        lcd.setCursor( 0, 0 );
        lcd.print( ":)" );
      }
      if ( randNumber == 4 ) {
        lcd.setCursor( 0, 0 );
        lcd.print( "Hold still." );
        playComplete("Append.wav");
      }
      if ( randNumber == 5 ) {
        playComplete("Dayum.wav");
        lcd.setCursor( 0, 0 );
        lcd.print( "Swag." );
      }
      if ( randNumber == 6 ) {
        playComplete("Fingers.wav");
        lcd.setCursor( 0, 0 );
        lcd.print( "....." );
      }
      if ( randNumber == 7 ) {
        playComplete("Life.wav");
        lcd.setCursor( 0, 0 );
        lcd.print( "<3" );
      }
      if ( randNumber == 8 ) {
        playComplete("Netflix.wav");
        lcd.setCursor( 0, 0 );
        lcd.print( ";)" );
      }
}//playRandomBit

/////////////////////////////////////////////////////////////////////
// Component: WaveShield
// Function:  playFile()
// Summary:   Finds the .wav file and plays it otherwise return error.            
// Params:    char *name         
// Returns:   Nothing
// Uses:      Nothing
/////////////////////////////////////////////////////////////////////
void playFile(char *name) {
  if (wave.isplaying) {   
    wave.stop(); 
  }
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); 
    Serial.print(name); 
    return;
  }
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); 
    return;
  }  
  wave.play();
}

/////////////////////////////////////////////////////////////////////
// Component: Uno
// Function:  heartRate()
// Summary:   Read and display BPM on lcd on UP button press            
// Params:    None         
// Returns:   Nothing
// Uses:      Nothing
/////////////////////////////////////////////////////////////////////
void heartRate() {
  int x = 0;
  while ( x < 4 ) {
    if ( QS == true ){                 // Quantified Self flag is true when arduino finds a heartbeat
      //fadeRate = 255;                  // Set 'fadeRate' Variable to 255 to fade LED with pulse
      lcd.setCursor( 0, 0 );
      lcd.print( "Heart rate: " );
      lcd.setCursor( 12, 0 );
      lcd.print( BPM );
      Serial.println( BPM );
      lcd.setCursor( 0, 1 );
      lcd.print( "Reading..." );
      
      heartvals[x] = BPM;
      delay( 500 );
      QS = false;                      // reset the Quantified Self flag for next time    
      x++;
    }
  }     
    lcd.setCursor(0,1);
    lcd.print("Done!     ");
}//heartRate

/////////////////////////////////////////////////////////////////////
// Component: Uno
// Function:  averageH()
// Summary:   Calculate the average of the 4 values in heartvals            
// Params:    None         
// Returns:   Nothing
// Uses:      heartVals[], mean()
/////////////////////////////////////////////////////////////////////
void averageH() {
  int average = 0;
  if ( heartvals[3] == 0 ) {
    lcd.setCursor( 4, 0 );
    lcd.print( "Error..." );
    lcd.setCursor( 0, 1 );
    lcd.print( "4 values needed" );
  } 
  else {
    average = mean( heartvals, 4 );
    lcd.setCursor( 0, 0 );
    lcd.print( "Average: " );
    lcd.setCursor( 9, 0 );
    lcd.print( average );
  }
}

/////////////////////////////////////////////////////////////////////
// Component: Uno 
// Function:  mean()
// Summary:   Calculate the average of the 4 values in heartvals            
// Params:    int array[], int numAvg         
// Returns:   int
// Uses:      heartVals[], mean()
/////////////////////////////////////////////////////////////////////
int mean(int array[], int numAvg)
{
  int sum = 0;
  for (int x = 0; x < numAvg;x++)
  {
    sum += array[x];
  }
  return (sum/numAvg);
}


/////////////////////////////////////////////////////////////////////
// Component: WaveShield 
// Function:  sdErrorCheck()
// Summary:   Checks the SD card. Pauses program if error is found.         
// Params:    None         
// Returns:   Nothing
// Uses:      Nothing 
///////////////////////////////////////////////////////////////////// 
void sdErrorCheck(void){ //Does void need to be in here?
  if (!card.errorCode()) 
  {
    return;
  }
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1); //Pause device if there is an error opening the card
}



