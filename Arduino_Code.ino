#include <Adafruit_BusIO_Register.h>
#include <Adafruit_GenericDevice.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_AS7341.h>
#include <math.h>

Adafruit_AS7341 as7341;

#define CHANNELS 10
const unsigned long readDuration = 5000; // 5 seconds 
const unsigned long gapDuration  = 30000; // 30-second gap

String sampleName = "";

const char* labels[CHANNELS] = {
  "415nm", "445nm", "480nm", "515nm", "555nm",
  "590nm", "630nm", "680nm", "Clear", "NIR"
};

// State machine
enum State {GET_SAMPLE_NAME, REF_READING, GAP, SAMPLE_READING, PRINT_RESULTS, DONE};
State state = GET_SAMPLE_NAME;

unsigned long startTime;

// Reference running average
float refMean[CHANNELS] = {0};
float refM2[CHANNELS]   = {0};
uint16_t refCount = 0;

// Sample running average
float sampMean[CHANNELS] = {0};
float sampM2[CHANNELS]   = {0};
uint16_t sampCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);

  if (!as7341.begin()) {
    Serial.println("Could not find AS7341");
    while (1) delay(10);
  }

  as7341.setATIME(10);
  as7341.setASTEP(200);
  as7341.setGain(AS7341_GAIN_4X);

  Serial.println("Enter sample name and press ENTER:");
}

void loop() {
  switch(state) {

    case GET_SAMPLE_NAME:
      if (Serial.available()) {
        sampleName = Serial.readStringUntil('\n');
        sampleName.trim();
        Serial.print("Sample name set to: ");
        Serial.println(sampleName);
        Serial.println("\nStarting 5s reference reading...");
        startTime = millis();
        state = REF_READING;
      }
      break;

    case REF_READING:
      if (millis() - startTime <= readDuration) {
        readReference();
        delay(50);
      } else {
        Serial.print("Reference done. Total readings: ");
        Serial.println(refCount);
        Serial.println("Load Sample....");
        startTime = millis();
        state = GAP;
      }
      break;

    case GAP:
      if (millis() - startTime >= gapDuration) {
        Serial.println("Starting 5s sample readings...");
        startTime = millis();
        state = SAMPLE_READING;
      }
      break;

    case SAMPLE_READING:
      if (millis() - startTime <= readDuration) {
        readSample();
        delay(50);
      } else {
        Serial.print("Sample reading done. Total readings: ");
        Serial.println(sampCount);
        state = PRINT_RESULTS;
      }
      break;

    case PRINT_RESULTS:
      printResults();
      state = DONE;
      break;

    case DONE:
      Serial.println("\nFinished. Press RESET to run again.");
      while(true){delay(100);}
      break;
  }
}

// Read Reference 
void readReference() {
  if (!as7341.readAllChannels()) return;

  for(int i=0;i<CHANNELS;i++){
    float x = as7341.getChannel(i);
    refCount++;
    float delta = x - refMean[i];
    refMean[i] += delta/refCount;
    float delta2 = x - refMean[i];
    refM2[i] += delta * delta2;
  }
}

//Read Sample 
void readSample() {
  if (!as7341.readAllChannels()) return;

  for(int i=0;i<CHANNELS;i++){
    float x = as7341.getChannel(i);
    sampCount++;
    float delta = x - sampMean[i];
    sampMean[i] += delta/sampCount;
    float delta2 = x - sampMean[i];
    sampM2[i] += delta * delta2;
  }
}

//Print Final Results (Single Table)
void printResults() {
  Serial.println("\nResults for Sample: " + sampleName);
  Serial.println("Wavelength | Ref Avg Int | Ref SD | Samp Avg Int | Samp SD | Absorbance");
  Serial.println("-----------------------------------------------------------------------");




  for (int i = 0; i < CHANNELS; i++) {

    float refSD  = (refCount  > 1) ? sqrt(refM2[i]  / (refCount  - 1)) : 0;
    float sampSD = (sampCount > 1) ? sqrt(sampM2[i] / (sampCount - 1)) : 0;

    float absorb = (sampMean[i] > 0 && refMean[i] > 0)
                     ? -log10(sampMean[i] / refMean[i])
                     : 0;

    Serial.print(labels[i]); Serial.print(" , ");

    Serial.print(refMean[i], 1); Serial.print(" , ");
    Serial.print(refSD, 2); Serial.print(" , ");

    Serial.print(sampMean[i], 1); Serial.print(" | ");
    Serial.print(sampSD, 2); Serial.print(" , ");

    Serial.println(absorb, 4);
  }

  Serial.print("\nReference readings taken: ");
  Serial.println(refCount);
  Serial.print("Sample readings taken: ");
  Serial.println(sampCount);
}
