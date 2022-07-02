/*
 Displaying instantaneous speed from a LIDAR on two large 7-segment displays
 By: Nathan Seidle
 SparkFun Electronics
 Date: January 5th, 2015
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 The new LIDAR-Lite from PulsedLight is pretty nice. It outputs readings very quickly. From multiple distance
 readings we can calculate speed (velocity is the derivative of position).

 Here's how to hook up the Arduino pins to the Large Digit Driver backpack: 
 Arduino pin 5 -> LAT
 6 -> CLK
 7 -> SER
 GND -> GND
 5V -> 5V
 VIN/Barrel Jack -> External 12V supply (this should power the LDD as well)
 
 You'll also need to connect the LIDAR to the Arduino:
 Arduino 5V -> LIDAR 5V
 GND -> GND
 A5 -> SCL
 A4 -> SDA
A0 -> Enable

*/

#include <Wire.h> //Used for I2C

#include <avr/wdt.h> //We need watch dog for this program

#define    LIDARLite_ADDRESS   0x62          // Default I2C Address of LIDAR-Lite.
#define    RegisterMeasure     0x00          // Register to write to initiate ranging.
#define    MeasureValue        0x04          // Value to initiate ranging.
#define    RegisterHighLowB    0x8F          // Register to get both High and Low bytes in 1 call.

//GPIO declarations
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

byte statLED = 13; //On board status LED
byte en_LIDAR = A0; //Low makes LIDAR go to sleep, high is normal operation

byte segmentLatch = 5; //Display data when this pin goes high
byte segmentClock = 6; //Clock one bit on each rising/falling edge
byte segmentSerial = 7; //Serial data in

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

long lastTime = 0;
long lastReading = 0;
int lastDistance = 265;
float newDistance;

const byte numberOfDeltas = 8;
float deltas[numberOfDeltas];
byte deltaSpot = 0; //Keeps track of where we are within the deltas array

//This controls how quickly the display updates
//Too quickly and it gets twitchy. Too slow and it doesn't seem like it's responding.
#define LOOPTIME 50

int maxMPH = 0; //Keeps track of what the latest fastest speed is
long maxMPH_timeout = 0; //Forget the max speed after some length of time

#define maxMPH_remember 3000 //After this number of ms the system will forget the max speed

void setup()
{
  wdt_reset(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  Serial.begin(115200);
  Serial.println("Speed Trap");

  Wire.begin();

  pinMode(en_LIDAR, OUTPUT);
  
  pinMode(segmentClock, OUTPUT);
  pinMode(segmentLatch, OUTPUT);
  pinMode(segmentSerial, OUTPUT);

  digitalWrite(segmentClock, LOW);
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentSerial, LOW);

  pinMode(statLED, OUTPUT);

  Serial.println("Coming online");

  enableLIDAR();
  while(readLIDAR() == 0)
  {
    Serial.println("Failed LIDAR read");
    delay(100);
  }

  showSpeed(42); //Test pattern

  delay(500);

  /*postNumber('c', false);
  postNumber(' ', false);
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentLatch, HIGH); //Register moves storage register on the rising edge of RCK
  delay(2000);*/

  wdt_reset(); //Pet the dog
  wdt_enable(WDTO_250MS); //Unleash the beast
}

void loop()
{
  wdt_reset(); //Pet the dog

  //Each second blink the status LED
  if (millis() - lastTime > 1000)
  {
    lastTime = millis();

    if (digitalRead(statLED) == LOW)
      digitalWrite(statLED, HIGH);
    else
      digitalWrite(statLED, LOW);
  }

  //Take a reading every 50ms
  if (millis() - lastReading > (LOOPTIME-1)) // 49)
  {
    lastReading = millis();

    //Every loop let's get a reading
    newDistance = readLIDAR(); //Go get distance in cm
    
    //Error checking
    if(newDistance > 1200) newDistance = 0;
    
    int deltaDistance = lastDistance - newDistance;
    lastDistance = newDistance;

    //Scan delta array to see if this new delta is sane or not
    boolean safeDelta = true;
    for(int x = 0 ; x < numberOfDeltas ; x++)
    {
      //We don't want to register jumps greater than 30cm in 50ms
      //But if we're less than 1000cm then maybe
      //30 works well
      if( abs(deltaDistance - deltas[x]) > 40) safeDelta = false; 
    }  
    
    //Insert this new delta into the array
    if(safeDelta)
    {
      deltas[deltaSpot++] = deltaDistance;
      if (deltaSpot >= numberOfDeltas) deltaSpot = 0; //Wrap this variable
    }

    //Get average of the current deltas array
    float avgDeltas = 0.0;
    for (byte x = 0 ; x < numberOfDeltas ; x++)
      avgDeltas += (float)deltas[x];
    avgDeltas /= numberOfDeltas;

    //22.36936 comes from a big coversion from cm per 50ms to mile per hour
    float instantMPH = 22.36936 * (float)avgDeltas / (float)LOOPTIME;
    
    instantMPH = abs(instantMPH); //We want to measure as you walk away

    ceil(instantMPH); //Round up to the next number. This is helpful if we're not displaying decimals.

    if(instantMPH > maxMPH)
    {
      showSpeed(instantMPH);

      maxMPH = instantMPH;
      maxMPH_timeout = millis();
    }
    else //maxMPH is king
    {
      showSpeed(maxMPH);
    }
    
    if(millis() - maxMPH_timeout > maxMPH_remember)
    {
      maxMPH = 0;
      showSpeed(0);
    }

    Serial.print("raw: ");
    Serial.print(newDistance);
    Serial.print(" delta: ");
    Serial.print(deltaDistance);
    Serial.print(" cm distance: ");
    Serial.print(newDistance * 0.0328084, 2); //Convert to ft
    Serial.print(" ft delta:");
    Serial.print(abs(avgDeltas));
    Serial.print(" speed:");
    Serial.print(abs(instantMPH), 2);
    Serial.print(" mph");
    Serial.println();
  }

}

//A watch dog friendly delay
void petFriendlyDelay(int timeMS)
{
  long current = millis();
  
  while(millis() - current < timeMS)
  {
    delay(1);
    wdt_reset(); //Pet the dog
  }
}

//Get a new reading from the distance sensor
int readLIDAR(void)
{
  int distance = 0;

  Wire.beginTransmission((int)LIDARLite_ADDRESS); // transmit to LIDAR-Lite
  Wire.write((int)RegisterMeasure); // sets register pointer to  (0x00)
  Wire.write((int)MeasureValue); // sets register pointer to  (0x00)
  Wire.endTransmission(); // stop transmitting

  delay(20); // Wait 20ms for transmit
  wdt_reset(); //Pet the dog

  Wire.beginTransmission((int)LIDARLite_ADDRESS); // transmit to LIDAR-Lite
  Wire.write((int)RegisterHighLowB); // sets register pointer to (0x8f)
  Wire.endTransmission(); // stop transmitting

  delay(20); // Wait 20ms for transmit
  wdt_reset(); //Pet the dog

  Wire.requestFrom((int)LIDARLite_ADDRESS, 2); // request 2 bytes from LIDAR-Lite

  if (Wire.available() >= 2) // if two bytes were received
  {
    distance = Wire.read(); // receive high byte (overwrites previous reading)
    distance = distance << 8; // shift high byte to be high 8 bits
    distance |= Wire.read(); // receive low byte as lower 8 bits
    return (distance);
  }
  else
  {
    Serial.println("Read fail");
    disableLIDAR();
    delay(100);
    enableLIDAR();

    return(0);
  }
}

//Takes a speed and displays 2 numbers. Displays absolute value (no negatives)
void showSpeed(float speed)
{
  int number = abs(speed); //Remove negative signs and any decimals

  //Serial.print("number: ");
  //Serial.println(number);

  for (byte x = 0 ; x < 2 ; x++)
  {
    int remainder = number % 10;

    postNumber(remainder, false);

    number /= 10;
  }

  //Latch the current segment data
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given a number, or '-', shifts it out to the display
void postNumber(byte number, boolean decimal)
{
  //    -  A
  //   / / F/B
  //    -  G
  //   / / E/C
  //    -. D/DP

#define a  1<<0
#define b  1<<6
#define c  1<<5
#define d  1<<4
#define e  1<<3
#define f  1<<1
#define g  1<<2
#define dp 1<<7

  byte segments;

  //This method uses 7946 bytes
  switch (number)
  {
    case 1: segments = b | c; break;
    case 2: segments = a | b | d | e | g; break;
    case 3: segments = a | b | c | d | g; break;
    case 4: segments = f | g | b | c; break;
    case 5: segments = a | f | g | c | d; break;
    case 6: segments = a | f | g | e | c | d; break;
    case 7: segments = a | b | c; break;
    case 8: segments = a | b | c | d | e | f | g; break;
    case 9: segments = a | b | c | d | f | g; break;
    case 0: segments = a | b | c | d | e | f; break;
    case ' ': segments = 0; break;
    case 'c': segments = g | e | d; break;
    case '-': segments = g; break;
  }

  //The method uses 7954 bytes
  /*if(number == 1) segments = b|c;
  if(number == 2) segments = a|b|d|e|g;
  if(number == 3) segments = a|b|c|d|g;
  if(number == 4) segments = f|g|b|c;
  if(number == 5) segments = a|f|g|c|d;
  if(number == 6) segments = a|f|g|e|c|d;
  if(number == 7) segments = a|b|c;
  if(number == 8) segments = a|b|c|d|e|f|g;
  if(number == 9) segments = a|b|c|d|f|g;
  if(number == 0) segments = a|b|c|d|e|f;
  if(number == ' ') segments = 0;
  if(number == 'c') segments = g | e | d;
  if(number == '-') segments = g;*/

  if (decimal) segments |= dp;

  for (byte x = 0 ; x < 8 ; x++)
  {
    digitalWrite(segmentClock, LOW);
    digitalWrite(segmentSerial, segments & 1 << (7 - x));
    digitalWrite(segmentClock, HIGH); //Data transfers to the register on the rising edge of SRCK
  }
}

//Sometimes the LIDAR stops responding. This causes it to reset
void disableLIDAR()
{
  digitalWrite(en_LIDAR, LOW);
}

void enableLIDAR()
{
  digitalWrite(en_LIDAR, HIGH);  
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(byte pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return (runningValue);
}
