#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <stdint.h>
#include <TouchScreen.h>
#include <math.h>

#define PI 3.1415926535897932384626433832795


//The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0


//Note:  the pinMode statements XM and YP are necessary
//to turn these pins around to output pins.  Touchscreen makes them inputs.
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 8   // can be a digital pin
#define XP 9   // can be a digital pin

//currently the schematic uses the rest pin
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

//touch screen pressure settings
#define MINPRESSURE 200
#define MAXPRESSURE 1000

//text sizes
#define BODYTEXT 2
#define TITLETEXT 3  //title ext size and large lables

//display title location
#define TITLELEFT 40 //upper left corner of the title
#define TITLEDOWN 10 //upper left corner of the title

//data for building the scale line for SWR and for power
#define SCALEOFFSET 2 //distance of scale line to the scale bar
#define SCALETEXT 1 //small text for the scale label
#define TEXTOFFSET -6 //to center the text abou the tick mark

//SWR scale and graph data
#define SWRLEFT 65   //upper left corner of the SWR bar
#define SWRMAX 480 -SWRLEFT  //maximum SWR bar travel
#define SWRDOWN 55   //upper left corner of the SWR bar
#define SWRHEIGHT 50 //height of the SWR bar
#define SWRLABELLEFT 5
#define SWRLABELDOWN SWRDOWN+15
#define SWR1P0 SWRLEFT
#define SWR1P1 SWRLEFT+22
#define SWR1P2 SWRLEFT+44
#define SWR1P4 SWRLEFT+84
#define SWR1P6 SWRLEFT+114
#define SWR1P8 SWRLEFT+139
#define SWR2P0 SWRLEFT+164
#define SWR2P5 SWRLEFT+210
#define SWR3P0 SWRLEFT+247
#define SWR4P0 SWRLEFT+289
#define SWR10  SWRLEFT+405
#define SCALEHEIGHT 7



//Power scale and graph data
#define PWRLEFT 65   //upper left corner of the PWR bar
#define PWRMAX 480 -PWRLEFT  //maximum PWR bar travel
#define PWRDOWN 145   //upper left corner of the PWR bar
#define PWRHEIGHT 50 //height of the PWR bar
#define PWRLABELLEFT 5
#define PWRLABELDOWN PWRDOWN+15
#define PWR0 PWRLEFT
#define PWR20 PWRLEFT+13
#define PWR50 PWRLEFT+33
#define PWR100 PWRLEFT+66
#define PWR200 PWRLEFT+132
#define PWR300 PWRLEFT+198
#define PWR400 PWRLEFT+263
#define PWR500 PWRLEFT+329
#define PWR600 PWRLEFT+395

//tune button data
#define TUNEDOWN 230
#define TUNELEFT 65
#define TUNEHEIGHT 65
#define TUNEWIDTH 150
#define TUNETEXTDOWN TUNEDOWN+22
#define TUNETEXTLEFT TUNELEFT+40

//bypass button data
#define PASSDOWN 230
#define PASSLEFT 265
#define PASSHEIGHT 65
#define PASSWIDTH 150
#define PASSTEXTDOWN PASSDOWN + 22
#define PASSTEXTLEFT PASSLEFT +20

//done button data
#define DONEDOWN 230
#define DONELEFT 150
#define DONEHEIGHT 65
#define DONEWIDTH 150
#define DONETEXTDOWN DONEDOWN + 22
#define DONETEXTLEFT DONELEFT +20

//touch button boarder
#define TOUCHMARGIN 5

//messagebox
#define MESSAGELEFT 65
#define MESSAGEDOWN 80
#define MESSAGELINE 40


//colors
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

struct button{
  int16_t x0;
  int16_t y0;
  int16_t w;
  int16_t h;
  String label;
  int16_t labelX;
  int16_t labelY;
};

struct scale{
  int16_t labelX;
  int16_t labelY;
  String label;
  int16_t hLineX;
  int16_t hLineY;
  int16_t hLineLength;
  int16_t vLineX[12];
  int16_t vLineY;
  int16_t vLineHeight;
  int16_t vTtextOffset;
  int16_t vTickY;
  String tickLabel[12];
};

struct message {
  int16_t x0;
  int16_t y0;
  int16_t spacing;
  String line1;
  String line2;
  String line3;
};


void setupBaseScreen(Adafruit_TFTLCD tft){
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(TITLETEXT);
  tft.setCursor(TITLELEFT, TITLEDOWN);
  tft.print("AD2CC 500 Watt HF Tuner");
}

void makeButton(Adafruit_TFTLCD tft, button *b, int color){
  tft.setTextSize(TITLETEXT);
  tft.setTextColor(BLACK);
  tft.fillRect(b->x0, b->y0, b->w, b->h, color);
  tft.setCursor(b->labelX, b->labelY);
  tft.print(b->label);
}

bool readButton(TouchScreen ts, button *b) {
  bool touch;
  // a point object holds x y and z coordinates
  TSPoint p = ts.getPoint();

  //Note:  the pinMode statements XM and YP are necessary
  //to turn these pins around to output pins.  Touchscreen makes them inputs.
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  int16_t realX, realY;

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    realX = map(p.y, 120, 910, 0, 480);
    realY = map(890 - p.x, 0, 740, 0, 320);

    touch = realX > b->x0 + TOUCHMARGIN;
    touch = touch && realX < b->x0 + b->w - TOUCHMARGIN;
    touch = touch && realY > b->y0 + TOUCHMARGIN;
    touch = touch && realY < b->y0 + b->h - TOUCHMARGIN;
    if (touch) {
      return true;
    }
  }
  return false;
}

void makeScale(Adafruit_TFTLCD tft, scale *s){
  tft.setCursor(s->labelX, s->labelY);
  tft.setTextSize(TITLETEXT);
  tft.print(s->label);
  tft.drawFastHLine(s->hLineX, s->hLineY, s->hLineLength, WHITE);
    tft.setTextSize(SCALETEXT);
  for (int i = 0; i < 12; i++) {
  tft.drawFastVLine(s->vLineX[i], s->vLineY, s->vLineHeight, WHITE);
  tft.setCursor(s->vLineX[i]+ s->vTtextOffset, s->vTickY);
  tft.print(s->tickLabel[i]);
}
}

void printMessage(Adafruit_TFTLCD tft, message *m){
  tft.setTextSize(TITLETEXT);
  tft.setCursor(m->x0, m->y0);
  tft.print(m->line1);
  tft.setCursor(m->x0, m->y0 + m->spacing);
  tft.print(m->line2);
  tft.setCursor(m->x0, m->y0 + 2 * m->spacing);
  tft.print(m->line3);
}
