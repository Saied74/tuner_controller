
#define CH0 0
#define CH1 1
#define CH2 2


void setupCounting( Tc *);
void startGenerator(Tc *, uint32_t, int);

uint32_t freqCount = 0; //ISR does not return value, so it will modify this
bool freqFlag = false;

//interrupt handler
void TC8_Handler() {
  Serial.println("Interrupted");
  NVIC_DisableIRQ(TC8_IRQn);
  TC_Stop(TC2, CH2);
  if ((TC_GetStatus(TC2, CH2) & TC_SR_LDRBS) == TC_SR_LDRBS) {
    uint32_t capturedRA = TC2->TC_CHANNEL[2].TC_RA;   //tc_read_ra(TC2, CH2);
    uint32_t capturedRB = TC2->TC_CHANNEL[2].TC_RB;  //tc_read_rb(TC2, CH2);
    freqCount = capturedRB - capturedRA;
    NVIC_EnableIRQ(TC8_IRQn);
    TC_Start(TC2, CH2);
  }
  freqFlag = true;
}

//timebase for measureing frequency (1 ms high pulse)
//to caputure A register on the rising edge and B register on the falling edge
//total count = RB - RA pulses/ms
//output on pin 3 of the Due from the Due schematic pin labled PWM3
//labled PC28 on the chip (also from the schematic)
//labled PIOA7, PC28, peripheral B from table 36-4 of the data sheet
void generateTimebase(Tc * tc, IRQn_Type irq) {
  TC_Configure(tc, CH1,
               TC_CMR_WAVE |
               TC_CMR_WAVSEL_UP_RC |
               TC_CMR_TCCLKS_TIMER_CLOCK1 |
               TC_CMR_ACPA_TOGGLE);

  TC_SetRA(tc, CH1, 0xA410); // 50% duty cycle square wave
  TC_SetRC(tc, CH1, 0xA410); // 42,000 decimal at 42 MHz to generate 1 ms high pulse

  PIO_Configure(PIOC,
                PIO_PERIPH_B,
                PIO_PC28B_TIOA7,
                PIO_DEFAULT);

  TC_Start(tc, CH1);
}

//frequency generator - 21 MHz (84 MHz / 4)
//output on pin 5 of the Due from the Due schematic pin labled PWM5
//labled PC25 on the chip (also from the schematic)
//labled PIOA6, PC25, peripheral B from table 36-4 of the data sheet
void startGenerator(Tc * tc, uint32_t channel, int i) {
  TC_Configure(tc, channel,
               TC_CMR_WAVE |
               TC_CMR_WAVSEL_UP_RC |
               TC_CMR_TCCLKS_TIMER_CLOCK1 |
               TC_CMR_ACPA_TOGGLE);
  TC_SetRA(tc, channel, i); // 50% duty cycle square wave
  TC_SetRC(tc, channel, i);
  PIO_Configure(PIOC,
                PIO_PERIPH_B,
                PIO_PC25B_TIOA6,
                PIO_DEFAULT);

  TC_Start(tc, channel);
}

//counts the pulses between rising and falling edge of TIOA8
//connect pin 5 (frequenccy generator outuput) to XC2 which would be
//TCLK8 in Table 36-4.  It is signal PD9 on peripheral B.
//This labled "Pin30" on the microcontroller chip in the schmatic
//which maps into pin 30 of the Arduino Due board (and it is stenciled as such)
void countPulses() {

  setupCounting(TC2);

  NVIC_EnableIRQ(TC8_IRQn);

  TC2->TC_CHANNEL[CH2].TC_IER =  TC_IER_LDRBS | TC_IER_LDRBS;
  TC2->TC_CHANNEL[CH2].TC_IDR = ~(TC_IER_LDRBS | TC_IER_LDRBS);
  TC_Start(TC2, CH2);
}


//set up (in this case TC2 channel 2) for counting pulses
//output on pin 11 of the Due from the Due schematic pin labled PWM11
//labled PD7 on the chip (also from the schematic)
//labled PIOA8, PD7, peripheral B from table 36-4 of the data sheet
void setupCounting( Tc * tc) {
  TC_Configure(tc, CH2,
               TC_CMR_TCCLKS_XC2 |          //clock selection
               TC_CMR_LDRA_RISING |         //RA loading on rising edge fo TIOA
               TC_CMR_LDRB_FALLING |        //RB loading on falling edge of TIOA
               TC_CMR_ABETRG |              //external trigger on TIOA
               TC_CMR_ETRGEDG_FALLING |    //external trigger edge falling
               TC_CMR_ENETRG);
}
