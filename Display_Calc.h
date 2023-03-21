#include <math.h>

#define PI 3.1415926535897932384626433832795

struct gamma {
  double magnitude0;
  double phase0;
  double frequency;
  double r0;
  double x0;
  double g0;
  double b0;
  double z0Squared;
  int region;
  double r1;
  double x1;
  double g1;
  double b1;
  double z1Squared;
  double inductor;
  double capacitor;
};

int inductorBank[8] = {3200, 1600, 800, 400, 200, 100, 50, 25}; //in nH
int capacitorBank[8] = {1370, 680, 340, 170, 86, 43, 22, 12}; //in pF


void calcStart(struct gamma *g){
  double real0 = g->magnitude0 * cos(g->phase0);
  double imag0 = g->magnitude0 * sin(g->phase0);
  double gSquared = g->magnitude0 * g-> magnitude0;
  double den = 1.0 + gSquared - 2 * real0;
  g->r0 = (1.0 - gSquared) / den;
  g->x0 = (2.0 * imag0)/den;
  g->z0Squared = g->r0 * g->r0 + g->x0 * g->x0;
  g->g0 = g->r0/g->z0Squared;
  g->b0 = -g->x0/g->z0Squared;
}

void estRegion(struct gamma *g){
  bool r1 = g->r0 > 1 || (g->x0 > 0 && g->r0 < g->z0Squared);
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
  double real1, imag1;
  if (g->region == 1 ){
    real1 = (1-g->g0)/(1 + 3 * g->g0);
    imag1 = -sqrt(real1 - real1 * real1);
    double gSquared = real1 * real1 + imag1 * imag1;
    double den = 1.0 + gSquared - 2* real1;
    g->r1 = (1 - gSquared) / den;
    g->x1 = (2.0 * imag1)/den;
    g->z1Squared = g->r1 * g->r1 + g->x1 * g->x1;
    g->g1 = g->r1/g->z1Squared;
    g->b1 = -g->x1/g->z1Squared;
    return;
  }
  real1 = (g->r0-1)/(3 * g->r0 + 1);
  imag1 = sqrt(-real1 - real1 * real1);
  double gSquared = real1 * real1 + imag1 * imag1;
  double den = 1.0 + gSquared - 2 * real1;
  g->r1 = (1.0 - gSquared) / den;
  g->x1 = (2.0 * imag1)/den;
  g->z1Squared = g->r1 * g->r1 + g->x1 * g->x1;
  g->g1 = g->r1/g->z1Squared;
  g->b1 = -g->x1/g->z1Squared;
}

void calcLC(struct gamma *g) {
  switch (g->region) {
    case 1:
    g->capacitor = (g->b1 - g->b0)/(100.0 * PI * g->frequency);
    g->inductor = -(50.0 * g->x1)/(2.0 * PI * g->frequency);
    break;
    case 3:
    g->inductor = -(50.0 * g->x1)/(2.0 * PI * g->frequency);
    break;
    case 4:
    g->capacitor = -(g->b1)/(100.0 * PI * g->frequency);
    break;
    case 2:
    g->capacitor = -(g->b1)/(100.0 * PI * g->frequency);
    g->inductor = (50.0 * (g->x1 - g->x0))/(2.0 * PI * g->frequency);
    break;
  }
}

char calcRelays(double x, int bank[8]){
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
