/*Tuner Controller will be a state machine
   it is described in the readme.MD file.
*/

#include <Display_Setup.h>
#include <Display_Calc.h>

#define POWERON 0
#define PRETUNE 1 //measure power level for tuning
#define MEASURE 2 //measure reflection coefficient gain and phase + frequency
#define RFHIGH 3 //RF power is too high for tuning
#define RFLOW  4 //RF power is too low for tuning
#define TUNE 5 //clculate parallel C and series L
#define MONITOR 6
#define NOTUNE 7

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

//state variables for running the state machine
int state, power;
bool firstRound; //when true, it signifies the first time in a state
int roundCount; //for the measure function to collect three bits of data

//state of the load as measured by the reflection coefficient
float gammaMag, gammaPhase, frequency;

void readTuneButton();
void displayRFLow();
void measurePower();
void measureGamma();
void displayRFHigh();
void displayRFLow();
void tuneUp();
void monitorOp();
void seekHelp();
void scream();

button tuneButton = {TUNELEFT, TUNEDOWN, TUNEWIDTH, TUNEHEIGHT, "TUNE", TUNETEXTLEFT, TUNETEXTDOWN};
button bypassButton = {PASSLEFT, PASSDOWN, PASSWIDTH, PASSHEIGHT, "BYPASS", PASSTEXTLEFT, PASSTEXTDOWN};
button doneButton = {DONELEFT, DONEDOWN, DONEWIDTH, DONEHEIGHT, "DONE", DONETEXTLEFT, DONETEXTDOWN};
scale swr = {0, SWRDOWN, "SWR", SWRLEFT, SWRDOWN + SWRHEIGHT + SCALEOFFSET, SWRMAX,
  {SWR1P0, SWR1P1, SWR1P2, SWR1P4, SWR1P6, SWR1P8, SWR2P0, SWR2P5, SWR3P0, SWR4P0, SWR10},
  SWRDOWN + SWRHEIGHT + SCALEOFFSET, SCALEHEIGHT, TEXTOFFSET, SWRDOWN + SWRHEIGHT + 2 * SCALEHEIGHT,
  {"1.0", "1.1", "1.2", "1.4", "1.6", "1.8", "2.0", "2.5", "3.0", "4.0", "10"}
};
scale pwr = {0, PWRDOWN, "PWR", PWRLEFT, PWRDOWN + PWRHEIGHT + SCALEOFFSET, PWRMAX,
  {PWR0, PWR50, PWR100, PWR200, PWR300, PWR400, PWR500, PWR600},
  PWRDOWN + PWRHEIGHT + SCALEOFFSET, SCALEHEIGHT, TEXTOFFSET, PWRDOWN + PWRHEIGHT + 2 * SCALEHEIGHT,
  {"0", "50", "100", "200", "300", "400", "500", "600"}
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  tft.reset();
  uint16_t identifier = tft.readID();

  if (identifier == 0x8357) {
    Serial.println(F("Found HX8357D LCD driver"));
  } else {
    Serial.print(F("Unknown LCD driver chip: "));
    Serial.println(identifier, HEX);
    Serial.println(F("Something is really wrong"));
    return;
  }
  tft.begin(identifier);
  tft.setRotation(1);
  setupBaseScreen(tft);
  makeScale(tft, &swr);
  makeScale(tft, &pwr);

  makeButton(tft, &tuneButton);
  makeButton(tft, &bypassButton);

  state = POWERON;
  firstRound = true;
  roundCount = 0;
}

void loop() {

  //state machine
  switch (state) {
    case POWERON:
      readTuneButton();
      break;
    case PRETUNE:
      measurePower();
      break;
    case MEASURE:
      measureGamma();
      break;
    case RFHIGH:
      displayRFHigh();
      break;
    case RFLOW:
      displayRFLow();
      break;
    case TUNE:
      tuneUp();
      break;
    case MONITOR:
      monitorOp();
      break;
    case NOTUNE:
      seekHelp();
      break;
    default:
      scream();
      break;
  }
  delay(100);
}

void readTuneButton() {
  if (bool t = readButton(ts, &tuneButton)) {
    Serial.println("TUNE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
  if (bool b = readButton(ts, &bypassButton) && firstRound) {
    Serial.println("BYPASS");
    state = POWERON;
    firstRound = true;
    roundCount = 0;
  }
}

void measurePower() {
  if (firstRound) {
    Serial.println("Enter power level");
    firstRound = false;
  }
  if (Serial.available() > 0) {
    String s = Serial.readString();
    int power = s.toInt();
    Serial.print("Power is: ");
    Serial.println(power);
    if (power < 10) {
      state = RFLOW;
      firstRound = true;
      roundCount = 0;
      return;
    }
    if (power > 30) {
      state = RFHIGH;
      firstRound = true;
      return;
    }
    state = MEASURE;
    firstRound = true;
    roundCount = 0;
    return;
  }

}

void measureGamma() { //and frequency
  if (firstRound && roundCount == 0 ) {
    Serial.println("Enter magnitude of Gamma");
    firstRound = false;
  }
  if (Serial.available() && roundCount == 0) {
    String s = Serial.readStringUntil('\n');
    gammaMag = s.toFloat();
    Serial.print("Magnitude of Gamma is: ");
    Serial.println(gammaMag);
    firstRound = true;
    roundCount++;
  }
  if (firstRound && roundCount == 1) {
    Serial.println("Enter phase of Gamma");
    firstRound = false;
  }
  if (Serial.available() && roundCount == 1) {
    String s = Serial.readStringUntil('\n');
    gammaPhase = s.toFloat();
    Serial.print("Phase of Gamma is: ");
    Serial.println(gammaPhase);
    firstRound = true;
    roundCount++;
  }
  if (firstRound && roundCount == 2) {
    Serial.println("Enter frequency");
    firstRound = false;
  }
  if (Serial.available() && roundCount == 2) {
    String s = Serial.readStringUntil('\n');
    frequency = s.toFloat();
    Serial.print("Frequency is: ");
    Serial.print(frequency);
    Serial.println(" MHz");
    delay(10);
    state = TUNE;
    firstRound = true;
    roundCount = 0;
  }
}

void displayRFHigh() {
  if (firstRound) {
    firstRound = false;
    setupBaseScreen(tft);
    message m = {MESSAGELEFT, MESSAGEDOWN, MESSAGELINE, "Lower RF power", "Between 10 & 30 watts",
                 "then push done"
                };
    printMessage(tft, &m);
    button doneButton = {DONELEFT, DONEDOWN, DONEWIDTH, DONEHEIGHT, "DONE", DONETEXTLEFT, DONETEXTDOWN};
    makeButton(tft, &doneButton);
  }
  if (bool b = readButton(ts, &doneButton)) {
    Serial.println("DONE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
}

void displayRFLow() {
  if (firstRound) {
    firstRound = false;
    setupBaseScreen(tft);
    message m = {MESSAGELEFT, MESSAGEDOWN, MESSAGELINE, "Increase RF power", "Between 10 & 30 watts",
                 "then push done"
                };
    printMessage(tft, &m);
    button doneButton = {DONELEFT, DONEDOWN, DONEWIDTH, DONEHEIGHT, "DONE", DONETEXTLEFT, DONETEXTDOWN};
    makeButton(tft, &doneButton);
  }
  if (bool b = readButton(ts, &doneButton)) {
    Serial.println("DONE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
}

void tuneUp() {
  roundCount = 0;
  if (firstRound) {
    setupBaseScreen(tft);
    makeScale(tft, &swr);
    makeScale(tft, &pwr);
    makeButton(tft, &tuneButton);
    makeButton(tft, &bypassButton);
    firstRound = false;
    Serial.println("Tuning up");
    double rad = 2*(gammaPhase/360)*PI;
    struct gamma g = {gammaMag, rad, 0, 0};
    gammaRealImag(&g);
    calcRX0(&g);
   Serial.print("Magnitude: ");
   Serial.println(g.magnitude);
   Serial.print("Phase: ");
   Serial.println(g.phase);
   Serial.print("Real: ");
   Serial.println(g.real);
   Serial.print("Imaginary: ");
   Serial.println(g.imag);
   Serial.print("R0: ");
   Serial.println(g.r0);
   Serial.print("X0: ");
   Serial.println(g.x0);
  }
}

void monitorOp() {

}

void seekHelp() {

}

void scream() {

}
