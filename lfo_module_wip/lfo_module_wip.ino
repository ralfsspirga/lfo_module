#include <math.h>
#include <SPI.h>

enum LFOMode {
  mode_sine,
  mode_triangle,
  mode_square,
  mode_saw
};

// Global configuration
const float SAMPLE_RATE = 100;
const int PPQ = 24; // Minibrute 2S default setting

// Global variables
const int CS_PIN = 9;
const int CLOCK_PIN = 2; // Interrupt PIN for clock sync
volatile unsigned long g_last_time = 0;
volatile float g_bpm = 120.0f;
typedef float (*WaveformFunc)(float phase);

class LFO {
  public:
    // Class variables
    int outPin;                // Output pin for the waveform
    int ratePin;               // Analog input pin to control frequency rate
    int offsetPin;             // Analog input pin to control offset
    int ledPin;                // Pin for an LED indicator
    int resetPin;              // Pin for CV reset signal
    uint8_t channel;           // DAC output channel
    float minFreq;             // Minimum frequency (Hz)
    float maxFreq;             // Maximum frequency (Hz)
    float exponentialQ;        // Curve shaping factor for exponential response
    volatile float phase = 0;  // Current phase of the LFO (volatile because it may update in interrupts)
    int clockCounter = 0;      // Counts external clock ticks

    // Constructor with initializer list
    LFO(int o, int r, int off, int l, int rst, uint8_t chnl, float minF, float maxF, float q)
      : outPin(o), ratePin(r), offsetPin(off), ledPin(l), resetPin(rst), channel(chnl),
        minFreq(minF), maxFreq(maxF),
        exponentialQ(q) {}

    void update() {
      this->outputValues();
      this->updatePhase();
    }

    // Methods to return values based on digital pin states and switches
    bool getIsSynced() {
      return false;
      // TODO - add sync switch variables and implement; Based on on/off switch
    }

    WaveformFunc getLfoModeFunction() {
      // TODO - implement, based on 4-way switch;
      return squareValue;
    }

    float getOffsetCv() {
      return 0.0;
      // TODO - Map -1 to +1 based on voltage on the offsetPin
    }

    float getNoteDivision() {
      return 2.0f / 3; // Placeholder

      // int divisionsLength = sizeof(noteDivisions) / sizeof(noteDivisions[0]);

      // return noteDivisions[analogRead(this->ratePin) / (1023 / divisionsLength)];
    }

    float getFreeFrequency() {
        float potVal = 80; // Todo: read
        float normalized = potVal / 1023.0;
        float curved = pow(normalized, this->exponentialQ);

        return this->minFreq + curved * (this->maxFreq - this->minFreq);
    }

    float getSyncedFrequency() {
      return 0.5;
      // TODO - Calcuate freq value based on ratePin reading and map to division length + BPM
    }

    float updatePhase() {
      float freq = this->getIsSynced() ? this->getSyncedFrequency() : this->getFreeFrequency();
      float increment = 2 * PI * freq / SAMPLE_RATE;

      this->phase += increment;

      if (this->phase >= 2 * PI) {
        this->phase -= 2 * PI;
      }
    }

    void clockPulse() {
      if (this->getIsSynced()) {
        long cycle_length = this->getNoteDivision() * PPQ; 
        float pulse_phase_length = 2 * PI / cycle_length;
        // phase = pulsePhaseLength * clockCounter;

        if (this->clockCounter >= cycle_length) {
          // phase = 0;
          this->clockCounter = 0;
        }
      }

      this->clockCounter++;
    }

    void outputValues() {
      WaveformFunc lfo_function = this->getLfoModeFunction();
      float offset_function_val = constrain(lfo_function(this->phase) + this->getOffsetCv(), -1, 1);

      // DAC
      this->writeDAC((2047 + 2047 * offset_function_val));
      Serial.println((int)(2047 + 2047 * offset_function_val));

      // LEDs
      uint8_t pwm_value = (uint8_t)(127 + 127 * offset_function_val);
      uint8_t led_adjusted_pwm = (uint8_t)pow(pwm_value / 255.0, 2) * 255.0; // Possible upgrade: integer math?
      // analogWrite(ledPin1, led_adjusted_pwm); // TODO: WRITE LED ADJUSTED VALUE TO LFO OUTPUT


    }

    void writeDAC(uint16_t value) {

      // value must be 0–4095 (12-bit)
      uint16_t packet = 0;

      if (this->channel == 1) packet |= (1 << 15);

      packet |= (2 << 13);  // Gain
      packet |= (1 << 12);  // Output active (SHDN = 1)
      packet |= (value & 0x0FFF); // 12-bit data in bits 11–0

      digitalWrite(CS_PIN, LOW);
      SPI.transfer16(packet);
      digitalWrite(CS_PIN, HIGH);
    }

    void reset () {
      this->clockCounter = 0;
      this->phase = 0;
    }

    // Methods for calculating function values
    static float sineValue (float phase) {
      return sin(phase);
    }

    static float triangleValue(float phase) {
      float norm_phase = fmod(phase, 2 * PI) / (2 * PI);
      return 2.0 * (1.0 - fabs(2.0 * norm_phase - 1.0)) - 1.0;
    }

    static float sawValue(float phase) {
      return (phase / (2 * PI)) * 2.0 - 1.0;
    }

    static float squareValue(float phase) {
      if (phase > PI) {
        return -1;
      } else {
        return 1;
      }
    }
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

int resetCycleLenghts[] = {
  1 * PPQ, // 0.125f,
  1 * PPQ, // 0.25f,
  3 * PPQ, // 0.25f * 1.5, = 0.375
  // 0.5f / 3,
  1 * PPQ, // 0.5f,
  3 * PPQ, // 0.5f * 1.5,
  // 1.0f / 3,
  1 * PPQ, // 1.0f, 
  3 * PPQ, // 1.0f * 1.5,
  // 2.0f / 3,
  3 * PPQ, // 3.0f,
  // 4.0f,
  // 5.0f,
  6 * PPQ, // 6.0f,
  8 * PPQ, // 8.0f,
  16 * PPQ, // 16.0f,
  32 * PPQ,// 32.0f,
};

// INIT LFOS GLOBALLY
LFO lfo1(
  9,        // outPin: PWM output on digital pin 9
  A0,       // ratePin: frequency control from analog pin A0
  A1,       // offsetPin: offset control from analog pin A1
  13,       // ledPin: onboard LED for blinking with LFO
  3,        // resetPin: pin for reset interrupt
  0,        // DAC channel
  0.02f,     // minFreq: minimum frequency 0.02 Hz
  10.0f,    // maxFreq: maximum frequency 10 Hz
  2.0f     // exponentialQ: neutral curve shaping (1.0 = linear)
);

void setup() {
  pinMode(CLOCK_PIN, INPUT_PULLUP);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockISR, RISING);
  attachInterrupt(digitalPinToInterrupt(lfo1.resetPin), lfo1ResetISR, RISING);
  // attachInterrupt(digitalPinToInterrupt(lfo2.resetPin), lfo2ResetISR, RISING);

  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));


  Serial.begin(9600);
}

// float calculateSyncedFrequency(float potVal) {
//   // freq = minFreq + potVal * (maxFreq - minFreq);

//   divisionLength =   2.0f / 3; // SET DIVISION LENGTH HERE!

//   return bpm / (60 * divisionLength);
// }

void loop() {
  lfo1.update();
  // lfo2.update();

  delay(1000 / SAMPLE_RATE);
}

void clockISR() {
  unsigned long now = millis();
  unsigned long interval = now - g_last_time;

  if (interval > 0) {
    g_last_time = now;
    g_bpm = (60000.0 / interval) / PPQ;
  }
}

void lfo1ResetISR() {
  lfo1.reset();
}

void lfo2ResetISR() {
  // lfo2.reset();
}

// float triangle(float phase) {
//   float normPhase = fmod(phase, 2 * PI) / (2 * PI);
//   return 2.0 * (1.0 - fabs(2.0 * normPhase - 1.0)) - 1.0;
// }

// float saw(float phase) {
//   return (phase / (2 * PI)) * 2.0 - 1.0;
// }

// float square(float phase) {
//   if (phase > PI) {
//     return -1;
//   } else {
//     return 1;
//   }
// }
