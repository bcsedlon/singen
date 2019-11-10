/*
// Calculation PWM duties
float x=0;
float y=0;
const float pi=3.14;
int z=0;
float v=0;
int w=0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  if (w==0){
    v=x*pi/180; // Making deg in radians
    y=sin(v);   // Calculate sine
    z=y*250;    // Calculate duty cycle(250 not 255 because will help to turn off transistors)
    delay(100);
    x=x+0.57;// increase the angle
  }

  if (x>90){
    // We stop to calculate we have the duty cycle for angles smaller than 90deg, the other half is symetric
    x=0;
    w==1;
  }
  Serial.println(z);// On the serial monitor will appear duty cycles between 0 and 90 deg
}
*/

/*  
Timer output  Arduino output  Chip pin  Pin name
OC0A  6 12  PD6
OC0B  5 11  PD5
OC1A  9 15  PB1
OC1B  10  16  PB2
OC2A  11  17  PB3
OC2B  3 5 PD3
*/

#define PWM_CHANNELS 2

#define SAMPLES 8
#define U0SET_PIN 3
#define P0SET_PIN 6
#define U1SET_PIN 7

#define IN0_PIN 7

#include <avr/io.h>
#include <avr/interrupt.h>


int lastVoltageSetpoint[PWM_CHANNELS];
int lastPhaseSetpoint[PWM_CHANNELS];
boolean bman = true;

bool brun = true;
bool breport = false;
unsigned long lastSerialReport;

int i[PWM_CHANNELS];
int phase[PWM_CHANNELS];
int positive[PWM_CHANNELS];
int x[PWM_CHANNELS];

float voltage[PWM_CHANNELS];
float voltageAmp[PWM_CHANNELS];
float voltageSetpoint[PWM_CHANNELS];
float phaseSetpoint[PWM_CHANNELS];
float freqSetpoint;

unsigned char sinPWM[] = {1,2,5,7,10,12,15,17,19,22,24,27,30,32,34,37,39,42,
  44,47,49,52,54,57,59,61,64,66,69,71,73,76,78,80,83,85,88,90,92,94,97,99,
  101,103,106,108,110,113,115,117,119,121,124,126,128,130,132,134,136,138,140,142,144,146,
  148,150,152,154,156,158,160,162,164,166,168,169,171,173,175,177,178,180,182,184,185,187,188,190,192,193,
  195,196,198,199,201,202,204,205,207,208,209,211,212,213,215,216,217,219,220,221,222,223,224,225,226,227,
  228,229,230,231,232,233,234,235,236,237,237,238,239,240,240,241,242,242,243,243,244,244,245,245,246,246,
  247,247,247,248,248,248,248,249,249,249,249,249,250,250,250,250,249,249,249,249,249,248,
  248,248,248,247,247,247,246,246,245,245,244,244,243,243,242,242,241,240,240,239,238,237,237,236,235,234,
  233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,217,216,215,213,212,211,209,208,207,205,204,
  202,201,199,198,196,195,193,192,190,188,187,185,184,182,180,178,177,175,173,171,169,168,166,164,162,160,
  158,156,154,152,150,148,146,144,142,140,138,136,134,132,130,128,126,124,121,119,117,115,113,110,108,106,
  103,101,99,97,94,92,90,88,85,83,80,78,76,73,71,69,66,64,61,59,57,54,52,49,47,44,42,39,37,34,32,30,
  27,24,22,19,17,15,12,10,7,5,2,1};
  // First value is 1 because we want to reduce the dead time betwen half cycles of sine signal.
  // To write the duty cycles we will use OCR0A and OCR0B for timer 0(pins  5 and 6), that means for one half cycle OCR0A will be equal with every component of vector myPWM and for other half cycle OCR0B will do that-see the post with Timer 0.
  // With the program below we generate phase correct signal at a 31372 Hz and 100 duty cycle(is between 0 and 255 on Timer 0).

unsigned char sinPWM0[314];
unsigned char sinPWM1[314];

int analogRead(int pin, int samples) {
  unsigned long r = 0;
  for(int i = 0; i < samples; i++) {
    r +=  analogRead(pin);
  }
  return r / samples;
}

void setup() {
  for(int channel = 0; channel < PWM_CHANNELS; channel++) {
    i[channel] = 0; 
    phase[channel] = 0; 
    voltageSetpoint[channel] = 100.0; 
    voltageAmp[channel] = 100.0; 
  }
  pinMode(IN0_PIN, INPUT_PULLUP);
  //memcpy(sinPWM0, sinPWM, sizeof(sinPWM));
  //memcpy(sinPWM1, sinPWM, sizeof(sinPWM));
  for(int j = 0; j < 314; j++) {
    sinPWM0[j] = 0.0;
    sinPWM1[j] = 0.0;
  }  
   
  cli(); // stop interrupts

  TCCR0A = 0; // reset the value
  TCCR0B = 0; //reset the value
  TCNT0 = 0; //reset the value
  TCCR0A = 0b10100001; // phase correct pwm mode
  TCCR0B = 0b00000001; // no prescaler
  
  TCCR2A = 0; // reset the value
  TCCR2B = 0; //reset the value
  TCNT2 = 0; //reset the value
  TCCR2A = 0b10100001; // phase correct pwm mode
  TCCR2B = 0b00000001; // no prescaler
  
  
  TCCR1A = 0; // reset the value
  TCCR1B = 0; // reset the value
  
  TCNT1 = 0; // reset the value
  OCR1A = 509; // compare match value
  
  TCCR1B = 0b00001001; // WGM12 bit is 1 and no prescaler

  TIMSK1 |=(1 << OCIE1A);

  sei(); // enable interrupts

  Serial.begin(115200);
  Serial.println("\nsinGEN\n");
  //analogReference(INTERNAL); // built-in reference, equal to 1.1 volts on the ATmega168 or ATmega328P

  Serial.println("USE x = 0, 1 COMMANDS:");
  Serial.println("USx:100\t[%]");
  Serial.println("PSx:180\t[deg]");
  Serial.println("FS0:1\t[-]");
  Serial.println("B\tBEGIN");
  Serial.println("E\tEND");
  Serial.println("M\tMANUAL");

  pinMode(4, INPUT_PULLUP);

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  //pinMode(9, OUTPUT);
  //pinMode(10, OUTPUT);

  pinMode(11, OUTPUT);
  pinMode(3, OUTPUT);
}

//ISR(TIMER0_COMPA_vect){
//}
//ISR(TIMER2_COMPA_vect){
//}
ISR(TIMER1_COMPA_vect){ // interrupt when timer 1 match with OCR1A value
  /*
  if(i[0] > 313 && positive[0] == 0){// final value from vector for pin 6
    i[0] = 0;// go to first value of vector
    positive[0] = 1;//enable pin 5
  }
  if(i[0] > 313 && positive[0] == 1){// final value from vector for pin 5
    i[0] = 0;//go to firs value of vector
    positive[0] = 0;//enable pin 6
  }
  x[0] = sinPWM[i[0]];// x take the value from vector corresponding to position i(i is zero indexed)
  i[0]++ ;// go to the next position
    
  if(i[1] > 313 && positive[1] == 0){// final value from vector for pin 6
    i[1] = 0;// go to first value of vector
    positive[1] = 1;//enable pin 5
  }
  if(i[1] > 313 && positive[1] == 1){// final value from vector for pin 5
    i[1] = 0;//go to firs value of vector
    positive[1] = 0;//enable pin 6
  }
  x[1] = sinPWM[i[1]];// x take the value from vector corresponding to position i(i is zero indexed)
  i[1]++ ;// go to the next position
  */

  positive[0] = (i[0] <= 313);
  if(positive[0])
    x[0] = sinPWM0[i[0]];
  else
    x[0] = sinPWM0[i[0] - 314];
  if(i[0] >= (314 * 2))
    i[0] = 0;
  i[0]++;  
  
  positive[1] = (i[1] <= 313);
  if(positive[1])
    x[1] = sinPWM1[i[1]];
  else
    x[1] = sinPWM1[i[1] - 314];
  if(i[1] >= (314 * 2))
    i[1] = 0;
  i[1]++; 
  /*
  for(int channel = 0; channel < PWM_CHANNELS; channel++) {
    int iphase = i + phase[channel];
    
    
    if(iphase > 313 && positive[channel] == 0  ) {
      positive[channel] = 1; //enable positive pin      
    }
    if(iphase > 313 && positive[channel] == 1){
      positive[channel] = 0;//enable negative pin
    }

    iphase %= 313;
    x[channel] = sinPWM[iphase];
    //x[channel] = sinPWM[i];
    //x[channel] = float(x[channel]) * voltageAmp[channel]; 
  }
  i++;
  if(i > 313)
    i = 0;
  */
  //voltageAmp[0] = 1.0;
  //voltageAmp[1] = 1.0;
  //x[0] = round(float(x[0]) * voltageAmp[0]);
  //x[1] = round(float(x[1]) * voltageAmp[1]);
  //x[0] /= int(voltageSetpoint[0]);
  //x[1] /= int(voltageSetpoint[1]);
  
  if(!digitalRead(4) || !brun) {
    for(int channel = 0; channel < PWM_CHANNELS; channel++) {
      i[channel] = phase[channel];
      x[channel] = 0;
    }
  }

  if(positive[0] == 0){
    OCR0B = 0; //make pin 5 0
    OCR0A = x[0]; //128; //enable pin 6 to corresponding duty cycle
  }
  if(positive[0] == 1){
    OCR0A = 0; //make pin 6 0
    OCR0B = x[0]; //128; //enable pin 5 to corresponding duty cycle
  }
/*
 if(positive[1] == 0){
    OCR1B = 0;
    OCR1A = 128;//x[1];
  }
  if(positive[1] == 1){
    OCR1A = 0;
    OCR1B = 128;//x[1];
  }
*/
  if(positive[1] == 0){
    OCR2B = 0;
    OCR2A = x[1];
  }
  if(positive[1] == 1){
    OCR2A = 0;
    OCR2B = x[1];
  }
}

/*
void calcSinPWM(float voltageSetpoint, unsigned char aSinPWM[]) {
  for(int j = 0; j < 314; j++) {
    aSinPWM[j] = float(sinPWM[j]) * voltageSetpoint / 100.0;
  }
}
*/

void serialReport() {
  int iphase[PWM_CHANNELS];
  int ix[PWM_CHANNELS];
  iphase[0] = i[0];
  iphase[1] = i[1];
  ix[0] = x[0];
  ix[1] = x[1];
    
  Serial.print("RUN:\t");  
  Serial.println(brun);   
  for(int channel = 0; channel < PWM_CHANNELS; channel++) {
    Serial.print("CH:\t");
    Serial.print(channel);   
    Serial.print("\tUS:\t"); //V
    Serial.print(voltageSetpoint[channel]);
    Serial.print("\tUM: \t"); //V
    Serial.print(voltage[channel]);
    Serial.print("\tPS: \t"); //degree
    Serial.print(phaseSetpoint[channel]);
    Serial.print("\tPM: \t"); //degree
    Serial.print(iphase[channel]);
    Serial.print("\tPWM: \t"); //-
    Serial.print(ix[channel]);
    Serial.print("\tF:\t");
    Serial.print(freqSetpoint); 
    Serial.println();
  }
  Serial.println();
}

void loop() {

  if(breport) {
      if(millis() - lastSerialReport > 100000) {
        lastSerialReport = millis();      
        serialReport();
      }  
  }

  if(bman && !digitalRead(IN0_PIN)) {
    lastVoltageSetpoint[0] = max(0, 100 - analogRead(U0SET_PIN, SAMPLES) / 10); //1023 * 100
    lastVoltageSetpoint[1] = max(0, 100 - analogRead(U1SET_PIN, SAMPLES) / 10);
    if(abs(lastVoltageSetpoint[0] - voltageSetpoint[0]) > 0) {
      voltageSetpoint[0] = lastVoltageSetpoint[0];
      //calcSinPWM(voltageSetpoint[0], &sinPWM0[0]);
      for(int j = 0; j < 314; j++) {
        sinPWM0[j] = float(sinPWM[j]) * voltageSetpoint[0] / 100.0;
      }   
    }
    if(abs(lastVoltageSetpoint[1] - voltageSetpoint[1]) > 0) {
      voltageSetpoint[1] = lastVoltageSetpoint[1];
      //calcSinPWM(voltageSetpoint[0], &sinPWM1[1]);
      for(int j = 0; j < 314; j++) {
        sinPWM1[j] = float(sinPWM[j]) * voltageSetpoint[1] / 100.0;
      }   
    }

    lastPhaseSetpoint[0] = float(1023 - analogRead(P0SET_PIN, SAMPLES)) / 2.5; //1023 / 360
    if(abs(lastPhaseSetpoint[0] - phaseSetpoint[0]) > 0) {
      phaseSetpoint[0] = lastPhaseSetpoint[0];
      phase[0] = round(float(phaseSetpoint[0]) * 1.7444444444);
      i[0] = phase[0];
      i[1] = phase[1];
    }
  }

  
  //Serial.println(Serial.read());
  if(Serial.available()) {

    //voltageSetpoint = Serial.parseFloat();
    String s = Serial.readString();

    //US?:10.2
    int pos = s.indexOf("US");
    if(pos >= 0) {
      bman = false;
          
      int channel = s[pos + 2] - 48;
      if(channel >= 0 && channel < PWM_CHANNELS) {
        s = s.substring(4);
        voltageSetpoint[channel] = s.toFloat();

        if(channel == 0) {
          //calcSinPWM(voltageSetpoint[0], &sinPWM0[0]);
          for(int j = 0; j < 314; j++) {
            sinPWM0[j] = float(sinPWM[j]) * voltageSetpoint[channel] / 100.0;
          }
        }
       if(channel == 1) {
          //calcSinPWM(voltageSetpoint[1], &sinPWM1[0]);
          for(int j = 0; j < 314; j++) {
            sinPWM1[j] = float(sinPWM[j]) * voltageSetpoint[channel] / 100.0;
          }
        }
      }
    }

    //PS?:10.2
    pos = s.indexOf("PS");
    if(pos >= 0) {
      bman = false;
          
      int channel = s[pos + 2] - 48;
      if(channel >= 0 && channel < PWM_CHANNELS) {
        s = s.substring(4);
        phaseSetpoint[channel] = s.toFloat();
        //phase[channel] = phaseSetpoint[channel];//round(float(phaseSetpoint[channel] * 100) / 57.0); //0.57
        phase[channel] = round(phaseSetpoint[channel] * 1.7444444444);
        i[0] = phase[0];
        i[1] = phase[1];
      }
    }

    pos = s.indexOf("FS0:");
    if(pos >= 0) {
      //bman = false;
      s = s.substring(4);
      freqSetpoint = s.toFloat();
      //freqSetpoint = max(, freqSetpoint);
      //freqSetpoint = min(, freqSetpoint);
      OCR1A = 509 - freqSetpoint;
    }


    //BEGIN
    pos = s.indexOf("B");
    if(pos >= 0) {
      brun = true;
    }
    //END
    pos = s.indexOf("E");
    if(pos >= 0) {
      brun = false;
    }
    //MANUAL
    pos = s.indexOf("M");
    if(pos >= 0) {
      bman = true;
    }
    //REPORT ON
    pos = s.indexOf("R");
    if(pos >= 0) {
      breport = true;
    }
    //REPORT OFF
    pos = s.indexOf("T");
    if(pos >= 0) {
      breport = false;
    }

    serialReport();
    /*
    int iphase[PWM_CHANNELS];
    int ix[PWM_CHANNELS];
    iphase[0] = i[0];
    iphase[1] = i[1];
    ix[0] = x[0];
    ix[1] = x[1];
    
    Serial.print("RUN:\t");  
    Serial.println(brun);   
    for(int channel = 0; channel < PWM_CHANNELS; channel++) {
      Serial.print("CH:\t");
      Serial.print(channel);   
      Serial.print("\tUS:\t"); //V
      Serial.print(voltageSetpoint[channel]);
      Serial.print("\tUM: \t"); //V
      Serial.print(voltage[channel]);
      Serial.print("\tPS: \t"); //degree
      Serial.print(phaseSetpoint[channel]);
      Serial.print("\tPM: \t"); //degree
      Serial.print(iphase[channel]);
      Serial.print("\tPWM: \t"); //-
      Serial.print(ix[channel]);
      Serial.print("F:\t");
      Serial.println(freqSetpoint); 
      Serial.println();
    }
    Serial.println();
    */
  }

  float sensorValue = analogRead(A0);  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  voltage[0] = sensorValue / 1023.0 * 5.0;
  sensorValue = analogRead(A1); 
  voltage[1] = sensorValue / 1023.0 * 5.0;
  //sensorValue = analogRead(A2); 
  //voltage[2] = sensorValue * (5.0 / 1023.0);

  /*
  //TODO: 
  for(int channel = 0; channel < PWM_CHANNELS; channel++) {
    if(voltage[channel] < voltageSetpoint[channel]) {
      voltageAmp[channel] += 1;
    }
    if(voltage[channel] > voltageSetpoint[channel]) {
      voltageAmp[channel ]-= 1;
    }
    voltageAmp[channel] = voltageSetpoint[channel];
   
    voltageAmp[channel] = min(voltageAmp[channel], 1.0);
    voltageAmp[channel] = max(voltageAmp[channel], 0.0);
  }
  */
}
