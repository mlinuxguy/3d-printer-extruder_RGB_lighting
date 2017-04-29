// Markham Thomas  4/29/2017
// This implments RGB lighting for a 3D printer.  It uses an Arduino Nano + Thermistor + Dotstar LED strip
// The code starts the LED strip at white until the extruder temperature grows over some set amount then
//  begins a sweep from Blue to Red (green is not used)
// The RGB layout is an X-Y axis with blue at the top of Y (255) and red at the end of X (255):
//  B (255)
//  |
//  |_____R (255)
// Nano
// Pin 11 and 13 (MOSI and CLK),  ADC0 has 100k resistor to +5, then from ADC0 to ground is a 10uF capacitor
//   The thermistor is across the ground and ADC0 pins
//  +5v ---|
//         100k resistor
//         |
//   ADC0--|----------------|
//         |                |
//       || 10uf cap       Thermistor 3950 100k
//         |                |
//   GND---|-----------------

#include <Adafruit_DotStar.h>
#include <SPI.h>      

#define NUMPIXELS 15 // Number of LEDs in strip, I cut the strip in half
#define T_IDLE    10 // Temp must grow over ambient at Nano power up plus this value before sweeping color
// Note: if you restart the Nano whatever the extruder temp is at the point of reset becomes ambient

// setup for Arduino Nano,  you will need to run gnd and ADC0 to the extruder thermistor lines
#define DATAPIN    11     // MOSI
#define CLOCKPIN   13     // CLK
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

#define IDLE_COLOR 0x7f7f7f   // dim white

// which analog pin to connect
#define THERMISTORPIN A0         
// resistance at 25 degrees C
#define THERMISTORNOMINAL 100000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25   
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 100000    

int   samples[NUMSAMPLES];
float ambient_temp;
byte  I_R, I_G, I_B;
 
// average NUMSAMPLES to even out thermistor readings,  we use steinhart algorithm to avoid NTC 3950 table
float read_adc_in_C(void) {
  uint8_t i;
  float average;
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(THERMISTORPIN);
   delay(10);
  }
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;
  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  return (steinhart); 
}

void setup() {
  strip.begin(); // Initialize pins for output
  strip.clear();
  strip.setPixelColor(0,IDLE_COLOR);
  strip.show();  // Turn all LEDs off ASAP
  ambient_temp = read_adc_in_C();               // ambient becomes extruder temp at reset
                                                // could easily add a thermometer module instead
  Serial.begin(9600);
}

// feed in temp and (0,255) to (255,0) to move along color line between blue and red
void line(int temp, int x0, int y0, int x1, int y1) {
  int count = 0;
  int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
  int err = (dx>dy ? dx : -dy)/2, e2;
  for(;;) {
    I_R = x0;
    I_B = y0;
    I_G = 0;
    if (x0==x1 & y0 == y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
    count += 1;
    if (count >= temp) {
      I_R = x0;
      I_B = y0;
      I_G = 0;
//      Serial.print(temp);
//      Serial.print(", ");
//      Serial.print(x0);
//      Serial.print(",");
//      Serial.println(y0);
      break;
    }
  }
}

// currently sets the entire pixel string to the same color
// could change by setting half the pixels to bed temperature
void scale_temp(float temp) {
  int z;
//  Serial.print("Temp: ");
//  Serial.println(temp);
  line(temp,0,255,255,0);
  for (z=0;z<NUMPIXELS;z++) {
    strip.setPixelColor(z,I_R,I_G,I_B);
  }
  strip.show();
}

void loop() {
  int x;
  float r;
  r = read_adc_in_C();
//  Serial.print("Main temp: ");
//  Serial.println(r);
  // if extruder starts to warm this will exceed T_IDLE + ambient and start color changes
  if (r <= (T_IDLE + ambient_temp)) {
    for (x=0;x<NUMPIXELS;x++) {
      strip.setPixelColor(x,127,127,127);
    }
    strip.show();
  } else scale_temp(r);
  delay(1000);                     
}
