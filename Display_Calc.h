#include <math.h>

#define PI 3.1415926535897932384626433832795

struct gamma {
  float magnitude0;
  float phase0;
  float frequency;
  float r0;
  float x0;
  float g0;
  float b0;
  float z0Squared;
  int region;
  float r1;
  float x1;
  float g1;
  float b1;
  float z1Squared;
  float inductor;
  float capacitor;
};

int inductorBank[8] = {3200, 1600, 800, 400, 200, 100, 50, 25}; //in nH
int capacitorBank[8] = {1370, 680, 340, 170, 86, 43, 22, 12}; //in pF


void calcStart(struct gamma *g){
  float real0 = g->magnitude0 * cos(g->phase0);
  float imag0 = g->magnitude0 * sin(g->phase0);
  float gSquared = g->magnitude0 * g-> magnitude0;
  float den = 1.0 + gSquared - 2 * real0;
  g->r0 = (1.0 - gSquared) / den;
  g->x0 = (2.0 * imag0)/den;
  g->z0Squared = g->r0 * g->r0 + g->x0 * g->x0;
  g->g0 = g->r0/g->z0Squared;
  g->b0 = -g->x0/g->z0Squared;
}

void estRegion(struct gamma *g){
  bool r1 = (g->r0 > 1) || ((g->x0 > 0) && (g->r0 < g->z0Squared));
  if (r1) {
    g->region = 1;
    return;
  }
  if (g->r0 == 1) {
    g->region = 3;
    return;
  }
  if (g->b0 == 1) {
    g->region = 4;
    return;
  }
  g->region = 2;
  return;
}

void calcEnd(struct gamma *g){
  float real1, imag1;
  if (g->region == 1 ){
    real1 = (1-g->g0)/(1 + 3 * g->g0);
    imag1 = -sqrt(real1 - real1 * real1);
    float gSquared = real1 * real1 + imag1 * imag1;
    float den = 1.0 + gSquared - 2* real1;
    g->r1 = (1 - gSquared) / den;
    g->x1 = (2.0 * imag1)/den;
    g->z1Squared = g->r1 * g->r1 + g->x1 * g->x1;
    g->g1 = g->r1/g->z1Squared;
    g->b1 = -g->x1/g->z1Squared;
    return;
  }
  real1 = (g->r0-1)/(3 * g->r0 + 1);
  imag1 = sqrt(-real1 - real1 * real1);
  float gSquared = real1 * real1 + imag1 * imag1;
  float den = 1.0 + gSquared - 2 * real1;
  g->r1 = (1.0 - gSquared) / den;
  g->x1 = (2.0 * imag1)/den;
  g->z1Squared = g->r1 * g->r1 + g->x1 * g->x1;
  g->g1 = g->r1/g->z1Squared;
  g->b1 = -g->x1/g->z1Squared;
}

void calcLC(struct gamma *g) {
  switch (g->region) {
    case 1:
    g->capacitor = (g->b1 - g->b0)/(314.1593 * g->frequency);
    g->inductor = -(7.9577 * g->x1)/(g->frequency);
    break;
    case 3:
    g->inductor = -(7.9577 * g->x1)/(g->frequency);
    break;
    case 4:
    g->capacitor = -(g->b1)/(314.1593 * g->frequency);
    break;
    case 2:
    g->capacitor = -(g->b1)/(314.1593 * PI * g->frequency);
    g->inductor = (7.9577 * (g->x1 - g->x0))/(g->frequency);
    break;
  }
}

char calcRelays(float x, int bank[8]){
  char n = 0;
  for (int i = 0; i < 8; i++) {
        if (x >= bank[i]){
            x = x -bank[i];
            n = (n << 1) | 1;
        } else {
            n = n << 1;
        }
    }
  return n;
}

//The Arduino Due analog input is read with 12 bit accuracy.
//The reference voltage is 3.3 volts which represents 4096 digital
//To convert the digital reading of the analog input, the value
//is divided by 4096 and multiplied by 3.3 to obtain the voltage
float getVoltage(float readValue) {
  float r =  readValue / 4096.0;
  r = 3.274 * r;
  return r;
}

//<----------------  AD8302 Configuration ----------------------->
//The equations of operation are stated in terms of channel A and B
//Channel A input is pin 2 which is the reflected voltage (Vref)
//Channel B input is pin 6 which is the forward voltage (Vfor)
//The maginitude output is in terms of 20Log(VA/VB).  
//Reflection coefficient is Vref/Vfor = VA/VB

//<-----------------  Magnitude of Gamma ------------------------>
//From the Analog Devices AD8302 data sheet we have the two ends of the
//magintude of Gamma curve at:
//20Log(VA/VB) = -30 dB at 30 mV
//20Log(VA/VB) = +30 dB at 1.8 volts (1800 mV)
//When the two are equal at 900 mV
//Maximum value of the reflection coefficient is 1 or 0 dB
//Putting a straight line through 0 dB and -30 dB
//We have the output and we want to calculate L
//L = 34.5 x output - 31.0345. Note that L is always negative
//since the reflection coefficient is always less than one

float calcMagGamma(float m){ //m is the output voltage of the AD8302
  float inDB = 34.5 * m - 31.0345;
  float r = inDB / 20;
  r = pow(10.0, r);
  return r;
}

//<---------------------  Phase of Gamma ------------------------>
//180 degree phase difference (p = 180) output is 30 mV or 0.030 volts
//zero degree phase difference (p = 0) output is 1.8 volts
//+/-90 degrees phase difference (p = 90) output is 0.900 volts
//using Excel to put a liner refreesion line through these points
//output = -0.0098 x p + 1.795 where p is the phase difference
//But we have the output and we want to calculate phase difference
//p = -101.69 x output + 182.53 
float calcPhaseGamma(float v) {
  return -101.69 * v + 182.53;
}


