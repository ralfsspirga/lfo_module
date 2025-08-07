#include <math.h>

enum LFOMode {
  SIN,
  TRI,
  SQUARE,
  SAW,
  RAND,
};

int noteDivisions[] = {
  0.125f,
  0.25f,
  0.25f * 1.5,
  0.5f / 3,
  0.5f,
  0.5f * 1.5,
  1.0f / 3,
  1.0f, 
  1.0f * 1.5,
  2.0f / 3,
  2.0f,
  3.0f,
  4.0f,
  5.0f,
  8.0f,
  16.0f,
  32.0f,
};

// Pin setup
const int pinOut1 = 10; // Private
const int pinOut2 = 11;
const int potPin1 = A0;
const int potPin2 = A1;
const int clockPin = 2;
const int resetPin = 3;

const int ledPin1 = 9;

// LFO configuration
const float sampleRate = 100;
const float minFreq = 0.02;
const float maxFreq = 10;

// Sync config
const int ppq = 24; // Minibrute 2S default setting
bool lfoSynced = false;
LFOMode currentLfoMode = SQUARE;

// Potentiometer config
const float exponentialQ = 2.0f;

// Init variables
volatile float phase = 0.0f;
volatile float phaseIncrement = 0.0f;

float funcVal;

float divisionLength = 1.0f;

float freq, increment;

int clockCounter = 0;
int phaseResetCounter = 0;

volatile unsigned long lastTime = 0;
volatile unsigned long lastTimeClock = 0;
volatile float bpm = 120.0f;

int resetCycleLenghts[] = {
  1 * ppq, // 0.125f,
  1 * ppq, // 0.25f,
  3 * ppq, // 0.25f * 1.5, = 0.375
  // 0.5f / 3,
  1 * ppq, // 0.5f,
  3 * ppq, // 0.5f * 1.5,
  // 1.0f / 3,
  1 * ppq, // 1.0f, 
  3 * ppq, // 1.0f * 1.5,
  // 2.0f / 3,
  3 * ppq, // 3.0f,
  // 4.0f,
  // 5.0f,
  6 * ppq, // 6.0f,
  8 * ppq, // 8.0f,
  16 * ppq, // 16.0f,
  32 * ppq,// 32.0f,
};

void setup() {
  pinMode(pinOut1, OUTPUT);
  pinMode(ledPin1, OUTPUT);

  pinMode(clockPin, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  // pinMode(clockPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(clockPin), clockISR, RISING);
  attachInterrupt(digitalPinToInterrupt(resetPin), resetISR, RISING);


  // updatePhaseIncrement();

  Serial.begin(9600);
}

float calculateLfoFrequency(float potVal) {
  float normalized = potVal / 1023.0;
  float curved = pow(normalized, exponentialQ);

  return minFreq + curved * (maxFreq - minFreq);
}

float calculateSyncedFrequency(float potVal) {
  freq = minFreq + potVal * (maxFreq - minFreq);

  divisionLength =   2.0f / 3; // SET DIVISION LENGTH HERE!

  return bpm / (60 * divisionLength);
}

void loop() {
  switch (currentLfoMode) {
    case SIN:
      funcVal = sin(phase);
      break;
    case TRI:
      funcVal = triangle(phase);
      break;
    case SQUARE:
      funcVal = square(phase);
      break;
    case SAW:
      funcVal = saw(phase);
      break;
    case RAND:
      if (!lfoSynced) {
        funcVal = constrain(randomValue(funcVal), -1, 1);
      }
      break;
  }

  Serial.println(funcVal);

  int pwmVal = (int)(127 + 127 * funcVal);
  analogWrite(pinOut1, pwmVal);

  int ledAdjustedPWM = pow(pwmVal / 255.0, 2) * 255.0;
  analogWrite(ledPin1, ledAdjustedPWM);
  
  // updateFrequency();
  // updateOffset();

  if (lfoSynced) {
    freq = calculateSyncedFrequency(analogRead(potPin1));
    increment = 2 * PI * freq / sampleRate; // Interpolate function value based on BPM between clock pulses
  } else {
    freq = calculateLfoFrequency(analogRead(potPin1));
    increment = 2 * PI * freq / sampleRate;
  }

  phase += increment;

  if (phase >= 2 * PI) {
    phase -= 2 * PI;

    if (lfoSynced && currentLfoMode == RAND) {
      // For synced LFO get a random value and hold it for whole cycle
      funcVal = random (0, 100001) / 100000.0;
    }

  }

  // Serial.print("BPM: ");
  // Serial.println(bpm);

  // Serial.print("freq: ");
  // Serial.println(freq);

  // Serial.print("freq value:");
  // Serial.println(freq);
  

  delay(1000 / sampleRate);
}

void clockISR() {
  unsigned long now = millis();
  unsigned long interval = now - lastTime;

  if (interval > 0) {
    // Calculate BPM
    lastTime = now;
    float reading = (60000.0 / interval) / ppq;
    bpm = reading;

    // Adjust phase to clock pulses
    if (lfoSynced && currentLfoMode != RAND) {
      long cycleLength = divisionLength * ppq; //8 * 24 
      float pulsePhaseLength = 2 * PI / cycleLength;
      phase = pulsePhaseLength * clockCounter;

      if (clockCounter >= cycleLength) {
        // phase = 0;
        clockCounter = 0;
      }
    }

    // Increment clock counter
    clockCounter++;
    phaseResetCounter++;
  }
}

void resetISR() {
  clockCounter = 0;
  phaseResetCounter = 0;
  phase = 0;
}

// updateOffset();

float triangle(float phase) {
  float normPhase = fmod(phase, 2 * PI) / (2 * PI);
  return 2.0 * (1.0 - fabs(2.0 * normPhase - 1.0)) - 1.0;
}

float saw(float phase) {
  return (phase / (2 * PI)) * 2.0 - 1.0;
}

float square(float phase) {
  if (phase > PI) {
    return -1;
  } else {
    return 1;
  }
}

float randomValue(float funcVal) {
  return random (2) ? funcVal + 0.01 : funcVal - 0.01;
}
