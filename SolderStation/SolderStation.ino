//*******************************//
// Soldering Station
// Matthias Wagner
// www.k-pank.de/so
// Get.A.Soldering.Station@gmail.com
//*******************************//


#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#define ST7735_GREY 0x3333
#include <SPI.h>


#include "SolderStation.h"
#include "iron.h"
#include "stationLOGO.h"


Adafruit_ST7735 tft = Adafruit_ST7735(cs_tft, dc, rst);  // Invoke custom library

int pwm_out = 0;
int target = AMBIENT_TEMP;
int actual_temp = 25;
unsigned long last_time;
long sum = 0;
boolean standby = false;
int initial_pos;
boolean unplugged_time;
boolean stopped = true;
unsigned long idle_time;

/*
 * Setup
 */
void setup(void) {
	
	pinMode(BLpin, OUTPUT);
	digitalWrite(BLpin, LOW);
	
	pinMode(STANDBYin, INPUT_PULLUP);
	
	pinMode(PWMpin, OUTPUT);
	digitalWrite(PWMpin, LOW);
	setPwmFrequency(PWMpin, PWM_DIV);
	digitalWrite(PWMpin, LOW);
	
	tft.initR(INITR_BLACKTAB);
	SPI.setClockDivider(SPI_CLOCK_DIV4);  // 4MHz

	initial_pos = analogRead(POTI);
	
	tft.setRotation(0);	// 0 - Portrait, 1 - Lanscape
	tft.fillScreen(ST7735_WHITE);
	tft.setTextWrap(true);
	
	//BAcklight on
	digitalWrite(BLpin, HIGH);
	last_time = micros();
  unplugged_time = millis();
	
#if defined(INTRO)
	
	//Print PTL logo
	tft.fillScreen(ST7735_WHITE);
	tft.drawBitmap(0,0,ptl_logo_bits,128,160,ST7735_GREY, ST7735_WHITE);
	delay(2500);

	//Print station Logo
	tft.fillScreen(ST7735_WHITE);
	tft.drawBitmap(2,1,stationLOGO1,124,47,ST7735_GREY);

	tft.drawBitmap(3,3,stationLOGO1,124,47,ST7735_BLUE);
	tft.drawBitmap(3,3,stationLOGO2,124,47,ST7735_GREY);
	tft.drawBitmap(15,50,iron,100,106,ST7735_GREY);
	tft.drawBitmap(17,52,iron,100,106,ST7735_BLUE);

	delay(1000);
	tft.fillScreen(ST7735_BLACK);
	tft.setTextColor(ST7735_YELLOW);
	tft.setCursor(75,1);
	tft.print("PTL v");
	tft.print(VERSION);
#endif


	tft.fillRect(0,47,128,125,ST7735_BLACK);
	tft.setTextColor(ST7735_WHITE);

	tft.setTextSize(1);
	tft.setCursor(1,84);
	tft.print("actual");

	tft.setTextSize(2);
	tft.setCursor(117,47);
	tft.print("o");

	tft.setTextSize(1);
	tft.setCursor(1,129);
	tft.print("target");

	tft.setTextSize(2);
	tft.setCursor(117,92);
	tft.print("o");

	tft.setCursor(80,144);
	tft.print("   %");

	tft.setTextSize(1);
	tft.setCursor(1,151);		//60
	tft.print("pwm");

	tft.setTextSize(2);

}

/*
 * Main loop
 */
void loop() {

	int old_temp = actual_temp;
	actual_temp = getTemperature();

  /* Detect unplugged pen, based on erratic mesures */
	if(abs(old_temp-actual_temp) > 20 || millis() < unplugged_time){
		writeHeating(target, AMBIENT_TEMP, pwm_out);
    unplugged_time = millis() + 1000;
    return;
	} else
		writeHeating(target, actual_temp, pwm_out);

	if((abs(analogRead(POTI)-initial_pos) < 10) && stopped)
		return;

	stopped = false;

	target = map(analogRead(POTI), 1023, 0, AMBIENT_TEMP, MAX_POTI);
  
	if (!digitalRead(STANDBYin)){ 
		doStandby();
	}	else {
		tft.setTextColor(ST7735_WHITE);
    standby = false;
	}

	/* PID */
	int dt = micros() - last_time;
    int dtemp = target - actual_temp;
  
	/* Proportional used when not in bang-bang mode */
	pwm_out = Kp*Kp*dtemp;
  
	/* Integral only to maintain temp when close to target (lower only) */
    (dtemp > 0 && dtemp < 15) ? sum += dtemp*dt : sum = 0;
	pwm_out += Ki*sum;

	/* Derivative NOT NECESSARY because we can't cool down */
	pwm_out += -Kd*dtemp/dt;

    /* constrain PWM between 0..PWM_MAX or bang-bang when far from target */
    if (pwm_out < 0) pwm_out = 0;
	if (pwm_out > 255 || dtemp > 20) pwm_out = PWM_MAX;

    /* safefty turn off in case of overrun */
    if (actual_temp > MAX_POTI + 50){
        target = AMBIENT_TEMP;
        pwm_out = 0;
		stopped = true;
	}
	
	analogWrite(PWMpin, pwm_out);	
}

/*
 * 
 */
int getTemperature()
{
	analogWrite(PWMpin, 0);		// switch off heater
	delay(DELAY_MEASURE);			// wait for some time (to get low pass filter in steady state)
	int adcValue = analogRead(TEMPin); // read the input on analog pin 7:
	Serial.print("ADC Value ");
	Serial.print(adcValue);
	analogWrite(PWMpin, pwm_out);	//switch heater back to last value
	return round(((float) adcValue)*ADC_TO_TEMP_GAIN+AMBIENT_TEMP); //apply linear conversion to actual temperature
}

/*
 * 
 */
void writeHeating(int solder, int actual, int pwm)
{
	static int solder_old = 	10;
	static int old_temp = 	10;
	static int pwm_old = 	10;

	pwm = map(pwm, 0, 255, 0, 100);
	
	tft.setTextSize(5);
	if (old_temp != actual){
		tft.setCursor(30,57);
		tft.setTextColor(ST7735_BLACK);

		if ((old_temp /100) != (actual /100)){
			tft.print(old_temp /100);
		}
		else
			tft.print(" ");
		
		if ( ((old_temp /10)%10) != ((actual /10)%10) )
			tft.print((old_temp /10)%10 );
		else
			tft.print(" ");
		
		if ( (old_temp %10) != (actual %10) )
			tft.print(old_temp %10 );
		
		tft.setCursor(30,57);
		tft.setTextColor(ST7735_WHITE);
		
		if (actual < 100)
			tft.print(" ");
		if (actual <10)
			tft.print(" ");
		
		int tempDIV = round(float(solder - actual)*8.5);
		tempDIV = tempDIV > 254 ? tempDIV = 254 : tempDIV < 0 ? tempDIV = 0 : tempDIV;
		tft.setTextColor(Color565(tempDIV, 255-tempDIV, 0));
		if (standby)
			tft.setTextColor(ST7735_CYAN);
		tft.print(actual);
		
		old_temp = actual;
	}
	
	if ( abs(solder_old - solder) > 2 ) {
		tft.setCursor(30,102);
		tft.setTextColor(ST7735_BLACK);

		if ((solder_old /100) != (solder /100)){
			tft.print(solder_old /100);
		}
		else
			tft.print(" ");
		
		if ( ((solder_old /10)%10) != ((solder /10)%10) )
			tft.print((solder_old /10)%10 );
		else
			tft.print(" ");
		
		if ( (solder_old %10) != (solder %10) )
			tft.print(solder_old %10 );
		
		//Post new value in White
		tft.setCursor(30,102);
		tft.setTextColor(ST7735_WHITE);
		if (solder < 100)
			tft.print(" ");
		if (solder <10)
			tft.print(" ");
		
		tft.print(solder);
		solder_old = solder;
		
	}
	
	
	tft.setTextSize(2);
	if (pwm_old != pwm){
		tft.setCursor(80,144);
		tft.setTextColor(ST7735_BLACK);
		if ((pwm_old /100) != (pwm /100)){
			tft.print(pwm_old /100);
		}
		else
			tft.print(" ");
		
		if ( ((pwm_old /10)%10) != ((pwm /10)%10) )
			tft.print((pwm_old /10)%10 );
		else
			tft.print(" ");
		
		if ( (pwm_old %10) != (pwm %10) )
			tft.print(pwm_old %10 );
		
		tft.setCursor(80,144);
		tft.setTextColor(ST7735_WHITE);
		if (pwm < 100)
			tft.print(" ");
		if (pwm <10)
			tft.print(" ");
		
		tft.print(pwm);
		pwm_old = pwm;
		
	}
	
}

/*
 * change target temp to STANDBY_TEMP
 */
void doStandby()
{  
	tft.setCursor(2,55);
	tft.setTextColor(ST7735_BLACK);
 
  if(!standby){
    standby = true;
    idle_time = millis() + (MAX_IDLE_TIME);
  }
  /* turn off if idle time exceeded */ 
  if(millis() > idle_time){
    target = AMBIENT_TEMP;
    stopped = true;
  }
  
	target = (!stopped && (target >= STANDBY_TEMP ))?  STANDBY_TEMP : target;

}

/*
 *  
 */
uint16_t Color565(uint8_t r, uint8_t g, uint8_t b) 
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


/*
 * 
 */
void setPwmFrequency(int pin, int divisor) 
{
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
