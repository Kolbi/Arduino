/*
  MQ-7 CO (carbon monoxide) Gas Sensor
  
  This sketch includes the timer function and the sensor reading functions.
 
  Sensor must be one time 'burned-in' for 24 hours prior to general use.  
  Burn-in consists of 5.0V for 60 sec followed by 1.4V for 90 sec.
  You can run this sketch for 24 hours (ignoring the readings / alarms), to burn-in the sensor.
  
  After burn-in, regular use of the sensor consists of calibration in clean air.  
  Therafter, the sensor needs to be subjected to a heating cycle of 5.0V for 60 sec, followed by 1.4V for 90 sec.  
  After two heating cycles (5.0 min total), a reading may be taken at the end of the high 5.0V heating cycle, 
  just before transitioning to the low 1.4V heating cycle.  
  
  This sketch checks for the presense of CO every 20 minutes.  
  It performs a 2 cycle (5.0 min) warm up (blinking the green LED), then it takes a sensor reading (blinking the red LED).  
  If the COG threshold of 50 ppm is exceeded, an alarm condition is set and it persists until the device is reset.  
  
  In the United States, OSHA limits long-term workplace exposure levels above 50 ppm.
  The average level in homes is 0.5-5ppm. The level near properly adjusted gas stoves in homes and from 
  modern vehicle exhaust emissions is 5-15ppm. The exhaust from automobiles in Mexico City central area is 100-200ppm. 
  The amount of CO that can be created from the exhaust from a home wood fire is 5000ppm. Concentrations as low as 667ppm 
  may cause up to 50% of the body's hemoglobin to convert to carboxyhemoglobin (a level that may result in seizure, 
  coma, and death). 

  Startup & calibration:
    Green LED = fast blink. Heating up MQ-7 sensor.
    Red LED = fast blink.  Taking MQ-7 sensor reading.
    Red LED = on 5 sec + buzzer chirp.  DHT11 sensor error.
    Red LED = steady on + buzzer = CO concentration exceeded !
  
  Normal operation:
    Green LED = steady on.  Power on
    Red LED = on ~3 sec.  Taking MQ-7 sensor reading.
    Red LED = on 5 sec + buzzer chirp.  DHT11 sensor error.
    Red LED = steady on + buzzer = CO concentration exceeded !
  
  Resources:
    A0  gas sensor signal
    DIO2  green LED.  
    DIO3  buzzer  
    DIO4  red LED.  
    DIO5  DHT11 temperature / humidity sensor.
    DIO7  NPN transistor for 12VDC relay
 
 Written by:  Mark Kiehl
 Source: http://www.savvymicrocontrollersolutions.com/arduino.php?topic=arduino-mq7-CO-gas-sensor
 */

byte pinGreenLED = 2;
byte pinRedLED = 4;

byte pinBuzzer = 3;

boolean heaterHigh = true;
byte heat_cycles = 0;

//  60 sec high heat (5.0V)
unsigned long timerA = 60000;  
unsigned long timerAlap = millis();  // timer

//  90 sec low heat (1.4V)
unsigned long timerB = 90000;  
unsigned long timerBlap = millis();  // timer
//  The difference between timerB and timerRead is 
//  how long a measurement will be made for MQ-7.
unsigned long timerRead = 60000;

byte pinNPN = 7;
byte pinMQ = A0;
boolean alarmCO = false;
//  The USA OSHA exposure limit for CO is 50 ppm.
//  The average level in a home is 0.5 to 5 ppm.
//  The level in a home with a proper adj gas stove is 5 to 15 ppm.
//  A CO of 667 ppm may result in seizure, coma, and death.
const unsigned int CO_threshold = 50;
// Adjust vRef to be the true supply voltage in mV.
float vRef = 5000.0;
float RL = 10.0;  //  load resistor value in k ohms
float Ro = 10.0;  //  default value 10 k ohms.  Revised during calibration.
const float Ro_clean_air_factor = 10.0;

float mV = 0.0;
unsigned long samples = 0;

////////////////////////////////////////////////////////////////////////
// DHT11 humidity/temperature sensor
#include "DHT.h"
const unsigned int pinDHT = 5;
boolean errDHT11 = false;
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11     // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(pinDHT, DHTTYPE);
////////////////////////////////////////////////////////////////////////
// With sensor holes facing you:
// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor


void setup() {
  pinMode(pinGreenLED, OUTPUT);
  delay(1);
  pinMode(pinRedLED, OUTPUT);
  delay(1);
  pinMode(pinMQ, INPUT); 
  delay(1);
  pinMode(pinBuzzer, OUTPUT);
  delay(1);
  pinMode(pinNPN, OUTPUT);
  delay(1);

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  //  Delay to give the Arduino Micro time to connect the Serial Monitor.
  delay(35000);

  Serial.print("DHT");
  Serial.print(DHTTYPE);
  Serial.println(" setup");  
  dht.begin();
  //  Get the ambient conditions (deg C & relative humidity) from DHT11
  float ambRH = dht.readHumidity();
  float ambTemp = dht.readTemperature();
  Serial.println("  ");
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(ambTemp) || isnan(ambRH)) {
    Serial.println("Failed to read from DHT");
    Serial.println("  ");
    errDHT11 = true;
    digitalWrite(pinRedLED, HIGH);
    digitalWrite(pinBuzzer, HIGH);
    delay(100);
    digitalWrite(pinBuzzer, LOW);
    delay(5000);
    digitalWrite(pinRedLED, LOW);
  } else {
    //  DHT11 ok, .. proceed.
    Serial.print("ambTemp = ");
    Serial.print(ambTemp);
    Serial.println(" deg C");
    Serial.print("ambRH = ");
    Serial.print(ambRH);
    Serial.println("% ");
  }
  Serial.println("  ");

  Serial.println("Calibrating MQ-7 CO sensor in clean air..");
  Serial.println("  60 sec high heat cycle..");
  digitalWrite(pinNPN, HIGH);
  // set = 200
  for(int i = 200; i>0; i--){
    blinkLED(pinGreenLED);
  }
  digitalWrite(pinNPN, LOW);
  Serial.println("  60 sec warmup complete");
  
  Serial.println("  90 sec heat cycle..");
  // set = 300
  for(int i = 300; i>0; i--){
    blinkLED(pinGreenLED);
  }
  Serial.println("  90 sec warmup complete.  Reading MQ-7..");
  
  // If mV > 3000, then repeat warm-up..
  
  //  take a reading..
  // set = 300
  for(int i = 300; i>0; i--){
    blinkLED(pinGreenLED);
    mV += Get_mVfromADC(pinMQ);
    samples += 1;
  }
  mV = mV / (float) samples;
  Serial.print("  avg A");
  Serial.print(pinMQ);
  Serial.print(" for ");
  Serial.print(samples);
  Serial.print(" samples = ");
  Serial.print(mV);
  Serial.println(" mV");
  Serial.print("  Rs = ");
  Serial.println(CalcRsFromVo(mV));
  //  Conv output to Ro
  //  Ro = calibration factor for measurement in clean air.
  //  Ro = ((vRef - mV) * RL) / (mV * Ro_clean_air_factor);
  //  Hereafter, measure the sensor output, convert to Rs, and
  //  then calculate Rs/Ro using: Rs = ((Vc-Vo)*RL) / Vo
  Ro = CalcRsFromVo(mV) / Ro_clean_air_factor;
  Serial.print("  Ro = ");
  Serial.println(Ro);
  //  Values in clean air are:
  //    Rs = 6.99
  //    Ro = 0.70
  Serial.println("Sensor calibration in clean air complete");
  Serial.println("Setup complete.  Monitoring for CO..");
  Serial.println("  ");
  digitalWrite(pinNPN, LOW);
  mV = 0.0;
  samples = 0;

  //  Start with heater on high
  heaterHigh = true;
  timerAlap = millis(); // reset the timer
  
  blinkLED(pinGreenLED);
  blinkLED(pinRedLED);
  //  Chirp the buzzer
  digitalWrite(pinBuzzer, HIGH);
  delay(1);
  digitalWrite(pinBuzzer, LOW);
  delay(1);
  //digitalWrite(pinGreenLED, HIGH);
  //delay(1);
}

void loop() {
  
  // if millis() or timer wraps around, we'll just reset it
  if (timerAlap > millis())  timerAlap = millis();
  if (timerBlap > millis())  timerBlap = millis();

  if (heaterHigh == false && heat_cycles == 2 && (millis() - timerBlap > timerRead)) {
    //  take reading of MQ sensor..
    digitalWrite(pinGreenLED, HIGH);
    mV += Get_mVfromADC(pinMQ);
    samples += 1;
  } else {
    digitalWrite(pinGreenLED, LOW);
  }

  
  if (heaterHigh == true) {
    //  High heat applied for 60 sec
    digitalWrite(pinNPN, HIGH);
    //  Timer A
    if (millis() - timerAlap > timerA) { 
      timerAlap = millis(); // reset the timer
      timerBlap = millis(); // reset the timer
      heaterHigh = false;
    }
  } else {
    //  heaterHigh = false
    //  Low heat applied for 90 sec
    digitalWrite(pinNPN, LOW);
    //  Timer B
    if (millis() - timerBlap > timerB) { 
      timerAlap = millis(); // reset the timer
      timerBlap = millis(); // reset the timer
      heaterHigh = true;
      heat_cycles += 1;
      Serial.print("end of heat_cycle = ");
      Serial.println(heat_cycles);
      //  Report on MQ-7 measurement at end of 
      //  the low phase of the 3rd heat cycle.
      if (heat_cycles == 3) {
        mV = mV / float (samples);
        Serial.print("samples = ");      
        Serial.println(samples);
        Serial.print("A");
        Serial.print(pinMQ);
        Serial.print("  = ");
        Serial.print(mV);
        Serial.println(" mV");
        Serial.print("Rs = ");
        Serial.println(CalcRsFromVo(mV));
        Serial.println("  ");
        mV = 0.0;
        samples = 0;
      }
    }
  }
  
  if (heat_cycles >= 3) {
    heat_cycles = 0;
  }
  
}

float RsRoAtAmbientTo20C65RH(float RsRo_atAmb, float ambTemp, float ambRH) {
  //  Using the datasheet for MQ-7 sensor, derive Rs/Ro values 
  //  from - 10 to 50 C and 33, 65, and 85 % relative humidity.
  //  For the measured Rs/Ro, use linear interpolation to calculate the
  //  standard Rs/Ro values for the measured ambient temperature and RH.
  //  Next, calculate a correction factor from the standard Rs/Ro at ambient
  //  temp and RH relative to standard Rs/Ro at 20C and 65 RH.  
  //  Apply this correction factor to the measured Rs/Ro value and return the
  //  corrected value.  This corrected value may then be used against the Rs/Ro
  //  Rs/Ro vs CO concentration (ppm) chart to estimate the concentration of CO.
  
  //  Calc RsRo values at ambTemp & 33% RH, 65% and 85% RH
  float RsRo_at_ambTemp_33RH = -0.00000593 * pow(ambTemp, 3) + 0.000533 * pow(ambTemp, 2) - 0.0182 * ambTemp + 1.20;
  float RsRo_at_ambTemp_85RH = -0.0000000741 * pow(ambTemp, 3) + 0.000114 * pow(ambTemp, 2) - 0.0114 * ambTemp + 1.03;
   //float RsRo_at_65RH = ((65.0-33.0)/(85.0-65.0));
  float RsRo_at_ambTemp_65RH = ((65.0-33.0)/(85.0-33.0)*(RsRo_at_ambTemp_85RH-RsRo_at_ambTemp_33RH)+RsRo_at_ambTemp_33RH)*1.102;
  //  Linear interpolate to get the RsRo at the ambient RH value (ambRH).
  float RsRo_at_ambTemp_ambRH;
  if (ambRH < 65.0) {
    RsRo_at_ambTemp_ambRH = (ambRH - 33.0)/(65.0 - 33.0)*(RsRo_at_ambTemp_65RH - RsRo_at_ambTemp_33RH) + RsRo_at_ambTemp_33RH;  
  } else {
    // ambRH > 65.0
    RsRo_at_ambTemp_ambRH = (ambRH - 65.0)/(85.0 - 65.0)*(RsRo_at_ambTemp_85RH - RsRo_at_ambTemp_65RH) + RsRo_at_ambTemp_65RH;
  }
  //  Calc the correction factor to bring RsRo at ambient temp & RH to 20 C and 65% RH.
  const float refRsRo_at_20C65RH = 1.00;
  float RsRoCorrPct = 1 + (refRsRo_at_20C65RH - RsRo_at_ambTemp_ambRH)/refRsRo_at_20C65RH;
  //  Calculate what the measured RsRo at ambient conditions would be corrected to the
  //  conditions for 20 C and 65% RH.
  float measured_RsRo_at_20C65RH = RsRoCorrPct * RsRo_atAmb;
  return measured_RsRo_at_20C65RH;
}

float CalcRsFromVo(float Vo) {
  //  Vo = sensor output voltage in mV.
  //  VRef = supply voltage, 5000 mV
  //  RL = load resistor in k ohms
  //  The equation Rs = (Vc - Vo)*(RL/Vo)
  //  is derived from the voltage divider
  //  principle:  Vo = RL * Vc (Rs + RL)
  //
  //  Note.  Alternatively you could calc
  //         Rs from ADC value using
  //         Rs = RL * (1024 - ADC) / ADC
  float Rs = (vRef - Vo) * (RL / Vo);  
  return Rs;
}

unsigned int GetCOPpmForRatioRsRo(float RsRo_ratio) {
  //  If you extract the data points from the CO concentration
  //  versus Rs/Ro chart in the datasheet, plot the points,
  //  fit a polynomial curve to the points, you come up with the equation
  //  for the curve of:  Rs/Ro = 22.073 * (CO ppm) ^ -0.66659
  //  This equation is valid for ambient conditions of 20 C and 65% RH.
  //  Solving for the concentration of CO you get:
  //    CO ppm = [(Rs/Ro)/22.073]^(1/-0.66666)
  float ppm;
  ppm = pow((RsRo_ratio/22.073), (1/-0.66659));
  return (unsigned int) ppm;
}

float Get_mVfromADC(byte AnalogPin) {
    // read the value from the sensor:
    int ADCval = analogRead(AnalogPin);  
    // It takes about 100 microseconds (0.0001 s) to read an analog input
    delay(1);
    //  Voltage at pin in milliVolts = (reading from ADC) * (5000/1024) 
    float mV = ADCval * (vRef / 1024.0);
    return mV;
}

void blinkLED(byte ledPIN){
  //  consumes 300 ms.
  for(int i = 5; i>0; i--){
    digitalWrite(ledPIN, HIGH);
    delay(30);
    digitalWrite(ledPIN, LOW);
    delay(30);
  }    
}
