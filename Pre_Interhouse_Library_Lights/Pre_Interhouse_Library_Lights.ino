#include <Adafruit_NeoPixel.h>
#include <RF24.h>
#include "averageStack.h"
#define PIN_1 2
#define PIN_2 3
#define PIN_3 4
#define PIN_4 5
#define buttonPin  6
#define NUM_PIXELS 72
#define WHITE_OUT false

Adafruit_NeoPixel pixels_1(NUM_PIXELS, PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels_2(NUM_PIXELS, PIN_2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels_3(NUM_PIXELS, PIN_3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels_4(NUM_PIXELS, PIN_4, NEO_GRB + NEO_KHZ800);

// CONSTANTS FOR MUSIC LIGHTS
const int N_VALUES = 3; // changes the sensitivity of the music strip (most sensitive: 0 < n < inf :least sensitive)
const double MINGAIN = 16.0; // Minimum upper threshold for sound to activate all lights
const int DELAYVAL = 0; // the smaller this is the more reactive and quick the music strip will be (loop frequency)
const int INITIAL_FILL_VALUE = 0; //initiall value for the list of volume that get averaged
const int THRESH = 15; // Lower threshold for volume to activate lights
const double GAIN_MULT = 1.5; // The multiplier from the average gain to activate all lights
const int NUM_GAIN_SAMPLES = 10; // Number of Gain Samples used calculating the upper threshold
const double GAIN_SAMPLE_DURATION = .5; //Duration in seconds over which Gain samples are taken
uint32_t WHITE = pixels_1.Color(250, 250, 250);
const float UPDATE_HZ = 60;
uint32_t currentcolor;
const int numLevels = 5;
//--------------------------------

int h = 1; // hue
int s = 255; // saturation
int sceil = 0; //default saturation (saturation increases when volume peaks then returns to this value)
int v = 255; //value, brightness. Increases when volume peaks then returns to 200
int j = 0;
double volume; // the averaged mic volumes for the last N_VALUES points
double sound; // the most recent mic volume
int maxled; // the highest led that will light up for a particular volume
short highled = 72; //the index of the "falling" LED, when maxled > highled, highled gets set to maxled
double GAIN = MINGAIN; // The calculated maximum gain from the average of the values
int gain_readings = 0; // Used to calculate the average over the sample duration
double gain_sample_sum = 0;
unsigned long SampleStart;
unsigned long SampleEnd;
unsigned long TimeStart;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 100;    // the debounce time; increase if the output flickers
int sample_millis;
double refactor;
boolean reading;
boolean lastSteadyState = LOW;
boolean lastButtonState = LOW;
boolean currentState = LOW;
int vScale = numLevels;
int update_duration = ceil(1000/UPDATE_HZ);
//int gainreset = 0;

RF24 radio(7, 8); // CE, CSN
const byte address[6] = "00001";

averageStack values = averageStack(N_VALUES, INITIAL_FILL_VALUE); // just helps average all the amplitude values together
averageStack gainVals = averageStack(NUM_GAIN_SAMPLES, MINGAIN);


void setup() {
  Serial.begin(115200);
  pixels_1.begin();
  pixels_2.begin();
  pixels_3.begin();
  pixels_4.begin();
  pixels_1.setBrightness(255);
  pixels_2.setBrightness(255);
  pixels_3.setBrightness(255);
  pixels_4.setBrightness(255);
  randomSeed(analogRead(A1));
  SampleStart = millis();
  TimeStart = millis();
  sample_millis = floor(GAIN_SAMPLE_DURATION * 1000);
  //  char sbuffer[100];
  //  sprintf(sbuffer, "stack: %p", &values);
  //  Serial.println(sbuffer);
  //  sprintf(sbuffer, "volume: %p", &volume);
  //  Serial.println(sbuffer);
  //  sprintf(sbuffer, "volume + stack size: %p", &volume + 4*8);
  //  Serial.println(sbuffer);
  //  Serial.println("s, v, maxled");
  pinMode(buttonPin, INPUT);
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setRetries(0,0);
  radio.setAutoAck(false);
  radio.stopListening();
}

void loop() {
//  TimeStart = millis();
  reading = digitalRead(buttonPin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch/button changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed from LOW to HIGH (Button Pressed)
    if(lastSteadyState == LOW && reading == HIGH) {
      vScale = (vScale + 1) % (numLevels +1);
    }

    // save the the last steady state
    lastSteadyState = reading;
  }
    
  lastButtonState = reading;
  
  if (millis() - SampleStart > sample_millis) {
    gainVals.enqueue(gain_sample_sum / gain_readings);
    gain_sample_sum = 0;
    gain_readings = 0;

    GAIN = (gainVals._avg + THRESH) * GAIN_MULT;
    if (GAIN < MINGAIN) {
      GAIN = MINGAIN;
    }
    SampleStart = millis();
  }
  
  if(radio.isChipConnected()){
  short datapacket[] = {h, s, v, maxled, highled};
  radio.write(&datapacket, sizeof(datapacket));
  }

  sound = analogRead(A0);
  
  values.enqueue(sound);

//  Serial.println(sound);

  //  if (sound >= 2*volume){
  //    values.flush(sound);
  //  }
  //

  volume = values._avg - THRESH;
  gain_sample_sum += volume;
  gain_readings += 1;

  pixels_1.clear(); // Set all pixel colors to 'off'
  pixels_2.clear();
  pixels_3.clear(); // Set all pixel colors to 'off'
  pixels_4.clear();



  if ((highled <= 0) && (volume <= 0)) {
    maxled = 0;
    h = random(65535);
    sceil = random(50, 100);
    s = sceil;
  }

  else if (volume > GAIN) {
    maxled = NUM_PIXELS;
    v = (255/numLevels) * vScale;
    if (WHITE_OUT) {
      s = 0;
    } else {
      s = 255;
    }
  }
  
  else {
    maxled = round(refactorFun(volume / GAIN) * NUM_PIXELS);
  }
  
  if (maxled > highled) {
    highled = maxled;
  }
  else {
    highled -= 1 ;
  }
  if(radio.isChipConnected()){
  short datapacket[] = {h, s, v, maxled, highled};
  radio.write(&datapacket, sizeof(datapacket));
  }
//  Serial.print(highled);
//  Serial.print(",");
//  Serial.println(maxled);
//  Serial.print(s);
//  Serial.print(",");
//  Serial.print(v);
//  Serial.print(",");
//  Serial.print(volume);
//  Serial.print(",");
//  Serial.print(GAIN);
//  Serial.print(",");
//  Serial.print(vScale*10);
//
//  Serial.print(s);
//  Serial.print(",");
//  Serial.print(v);
//  Serial.print(",");
//  Serial.print(volume);
//  Serial.print(",");
//  Serial.println(GAIN);
//  Serial.print(",");
//  Serial.print(vScale*10);
  //currentcolor = pixels_1.gamma32(pixels_1.ColorHSV(h, s, v));
  for (int i = 0; i < maxled; i++) { // For each pixel...
    // pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels_1.setPixelColor(i, pixels_1.gamma32(pixels_1.ColorHSV(h+150*i, s, v)));
    pixels_2.setPixelColor(i, pixels_1.gamma32(pixels_1.ColorHSV(h+150*i, s, v))); // periodically oscillates the rgb out of phase with each other
    pixels_3.setPixelColor(i, pixels_1.gamma32(pixels_1.ColorHSV(h+150*i, s, v)));
    pixels_4.setPixelColor(i, pixels_1.gamma32(pixels_1.ColorHSV(h+150*i, s, v)));
  }

  if (highled > 0) {
    pixels_1.setPixelColor(highled, pixels_1.gamma32(pixels_1.ColorHSV(h+150*highled, 255, (255/numLevels) * vScale)));
    pixels_2.setPixelColor(highled, pixels_1.gamma32(pixels_1.ColorHSV(h+150*highled, 255, (255/numLevels) * vScale)));
    pixels_3.setPixelColor(highled, pixels_1.gamma32(pixels_1.ColorHSV(h+150*highled, 255, (255/numLevels) * vScale)));
    pixels_4.setPixelColor(highled, pixels_1.gamma32(pixels_1.ColorHSV(h+150*highled, 255, (255/numLevels) * vScale)));
  }


//
//  if (v >= (200/numLevels) * vScale) {
//    v -= (2.5/numLevels) * vScale;
//  }
  if (WHITE_OUT) {
    if (s <= sceil - 4) {
      s += 5;
    }
  } else {
    if (s >= sceil + 4) {
      s -= 2.5;
    }
  }
  pixels_1.show();
  pixels_2.show();
  pixels_3.show();
  pixels_4.show();

  if(radio.isChipConnected()){
  short datapacket[] = {h, s, v, maxled, highled};
  radio.write(&datapacket, sizeof(datapacket));
  }
//  while (millis()-TimeStart < update_duration){
//    delay(1);
//    if(radio.isChipConnected()){
//      short datapacket[] = {h, s, v, maxled, highled};
//      radio.write(&datapacket, sizeof(datapacket));
//     }
//  }
//  delay(DELAYVAL);
//  Serial.print(",");
//  Serial.print((millis()-TimeStart));
//  Serial.print(",");
//  Serial.println(h);
//  TimeStart = millis();
}

double refactorFun(double normvol) {
  // Takes in volume/GAIN, will output the function's action on it

  // Current function is x^3

  return normvol * normvol * normvol;
}
