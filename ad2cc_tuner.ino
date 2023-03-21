/*
Tuner controller is a state machine.  The user requirements are discribed in
my blog:
https://ad2cc.blogspot.com/2022/08/500-watt-antenna-tuner-part-7-user.html
The design of the controller sofware is described in:

*/

#include <Display_Setup.h>
#include <Display_Calc.h>
#include <Timer_Functions.h>

#define POWERON  0
#define PRETUNE  1 //measure power level for tuning
#define MEASURE  2 //measure reflection coefficient gain and phase + frequency
#define RFHIGH   3 //RF power is too high for tuning
#define RFLOW    4 //RF power is too low for tuning
#define TUNE     5 //clculate parallel C and series L
#define MONITOR  6
#define PREBP    7 //measure power before activating the bypass relay
#define RFHIGHBP 8 //RF level too high to activate the BP relay
#define BYPASS   9
#define NOTUNE   10



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
float measurePower();
void measureGamma();
void displayRFHigh();
void displayRFLow();
void tuneUp();
void monitorOp();
void seekHelp();
void scream();
void measureHiLoPower();
void measureHiPower();
void highForBypass();

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

  //<--------------------- Screen and Button Setup --------------------------->
  tft.begin(identifier);
  tft.setRotation(1);
  setupBaseScreen(tft);
  makeScale(tft, &swr);
  makeScale(tft, &pwr);

  makeButton(tft, &tuneButton, WHITE);
  makeButton(tft, &bypassButton, WHITE);

//<------------------------ State Machine Setup ------------------------------>
  state = POWERON;
  firstRound = true;
  roundCount = 0;

//<-----------------------  Time/Counter Setup -------------------------------->
pmc_set_writeprotect(false);
pmc_enable_periph_clk(TC6_IRQn);
pmc_enable_periph_clk(TC7_IRQn);
pmc_enable_periph_clk(TC8_IRQn);
}

void loop() {

  //state machine
  switch (state) {
    case POWERON:
      readTuneButton();
      break;
    case PRETUNE:
      measureHiLoPower();
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
    case PREBP:
      measureHiPower();
      break;
    case RFHIGHBP:
      highForBypass();
      break;
    case BYPASS:
      byPass();
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
    state = PREBP;
    firstRound = true;
    roundCount = 0;
  }
}

void measureHiLoPower() {
  if (firstRound) {
    setupBaseScreen(tft);
    makeScale(tft, &swr);
    makeScale(tft, &pwr);
    makeButton(tft, &tuneButton, WHITE);
    makeButton(tft, &bypassButton, WHITE);
    power = measurePower();
    firstRound = false;
  }
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
    measureFrequency();
    frequency = float(freqCount);
    state = TUNE;
    firstRound = true;
    roundCount = 0;
  }

  // if (firstRound && roundCount == 2) {
  //   Serial.println("Enter frequency");
  //   firstRound = false;
  // }
  // if (Serial.available() && roundCount == 2) {
  //   String s = Serial.readStringUntil('\n');
  //   frequency = s.toFloat();
  //   Serial.print("Frequency is: ");
  //   Serial.print(frequency);
  //   Serial.println(" MHz");
  //   delay(10);
  //   state = TUNE;
  //   firstRound = true;
  //   roundCount = 0;
  // }
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
    makeButton(tft, &doneButton, WHITE);
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
    makeButton(tft, &doneButton, WHITE);
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
    makeButton(tft, &tuneButton, WHITE);
    makeButton(tft, &bypassButton, WHITE);
    firstRound = false;
    Serial.println("Tuning up");
    double rad = 2*(gammaPhase/360)*PI;
    struct gamma g = {gammaMag, rad, frequency*1.0e3, 0};
    calcStart(&g);
    estRegion(&g);
    calcEnd(&g);
    calcLC(&g);
    Serial.print("R0: ");
    Serial.println(g.r0);
    Serial.print("X0: ");
    Serial.println(g.x0);
    Serial.print("R1: ");
    Serial.println(g.r1);
    Serial.print("X1: ");
    Serial.println(g.x1);
    Serial.print("Inductor (nH): ");
    Serial.println(g.inductor * 1e9, 2);
    Serial.print("Capacitor (pF): ");
    Serial.println(g.capacitor * 1e12, 2);
    Serial.print("Region: ");
    Serial.println(g.region);
    char l = calcRelays(g.inductor * 1e9, inductorBank);
    char c = calcRelays(g.capacitor * 1e12, capacitorBank);
    Serial.print("Inductor relay setting: ");
    for (int i = 0; i < 8; i++) {
      Serial.print(l & 1);
      l = l >> 1;
    }
    Serial.println("");
    Serial.print("Capacitor relay setting: ");
    for (int i = 0; i < 8; i++) {
      Serial.print(c & 1);
      c = c >> 1;
    }
    Serial.println("");
  }
  state = MONITOR;
  firstRound = true;
}

void monitorOp() {
  if (firstRound){
  setupBaseScreen(tft);
  makeScale(tft, &swr);
  makeScale(tft, &pwr);
  makeButton(tft, &tuneButton, GREEN);
  makeButton(tft, &bypassButton, WHITE);
  firstRound = false;
  }
  if (bool t = readButton(ts, &tuneButton)) {
    Serial.println("TUNE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
  if (bool b = readButton(ts, &bypassButton)) {
    Serial.println("BYPASS");
    state = PREBP;
    firstRound = true;
    roundCount = 0;
  }
}

void measureHiPower(){
    if (firstRound){
      setupBaseScreen(tft);
      makeScale(tft, &swr);
      makeScale(tft, &pwr);
      makeButton(tft, &tuneButton, WHITE);
      makeButton(tft, &bypassButton, WHITE);
      firstRound = false;
      power = measurePower();
      firstRound = false;
    }
    if (power > 30) {
      state = RFHIGHBP;
      Serial.println(state);
      firstRound = true;
      return;
    }
    state = BYPASS;
    firstRound = true;
    roundCount = 0;
    return;
}

void byPass() {
  if (firstRound){
  setupBaseScreen(tft);
  makeScale(tft, &swr);
  makeScale(tft, &pwr);
  makeButton(tft, &tuneButton, WHITE);
  makeButton(tft, &bypassButton, GREEN);
  firstRound = false;
  }
  if (bool t = readButton(ts, &tuneButton)) {
    Serial.println("TUNE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
}

void highForBypass(){
  if (firstRound) {
    firstRound = false;
    setupBaseScreen(tft);
    message m = {MESSAGELEFT, MESSAGEDOWN, MESSAGELINE, "Lower RF power", "Between 10 & 30 watts",
                 "then push done"
                };
    printMessage(tft, &m);
    button doneButton = {DONELEFT, DONEDOWN, DONEWIDTH, DONEHEIGHT, "DONE", DONETEXTLEFT, DONETEXTDOWN};
    makeButton(tft, &doneButton, WHITE);
  }
  if (bool b = readButton(ts, &doneButton)) {
    Serial.println("DONE");
    state = PREBP;
    firstRound = true;
    roundCount = 0;
  }
}

void seekHelp() {
  if (firstRound) {
    setupBaseScreen(tft);
    makeButton(tft, &tuneButton, WHITE);
    makeButton(tft, &bypassButton, WHITE);
  }
  message m = {MESSAGELEFT, MESSAGEDOWN, MESSAGELINE, "There is something wrong",
              "fix the problem",
               "then push TUNE or BYPASS"
              };
  printMessage(tft, &m);

  if (bool t = readButton(ts, &tuneButton)) {
    Serial.println("TUNE");
    state = PRETUNE;
    firstRound = true;
    roundCount = 0;
  }
  if (bool b = readButton(ts, &bypassButton) && firstRound) {
    Serial.println("BYPASS");
    state = PREBP;
    firstRound = true;
    roundCount = 0;
  }

}

void scream() {

}

float measurePower(){
  Serial.println("Enter power level");
  while (1){
    if (Serial.available()) {
      String s = Serial.readStringUntil('\n');
      power = s.toFloat();
      Serial.print("Power level is: ");
      Serial.println(power);
      return power;
    }
  }
}

void measureFrequency(){
  int n;
  Serial.println("Enter an integeer divisor");
  while (1) {
    if (Serial.available()) {
      String s = Serial.readString();
      n = s.toInt();
      if (n == 0) {
        Serial.println("Divisor is an integer, try again");
      } else {
        break;
      }
    }
  }
  Serial.print("n = ");
  Serial.println(n);
  generateTimebase(TC2, TC7_IRQn); //1ms timebase for counting counting pules
  startGenerator(TC2, CH0, n); //simulated frequency generator, deleted in the final
  countPulses();


  TC_Start(TC2, CH2);
  //startGenerator(TC2, CH0, n);
  Serial.println(freqFlag);
  while (!freqFlag) {
    delay(1);
  } //wait for event
  freqFlag = false;
  Serial.print("Frequency: ");
  Serial.print(float(freqCount)/1000.0);
  Serial.println(" MHz");

  //stop all counters (on the count of noise generation)
  // TC_Stop(TC2, CH0);
  // TC_Stop(TC2, CH1);
  TC_Stop(TC2, CH2);
}
