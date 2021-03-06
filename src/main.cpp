/*
    Name:       BeeScale.cpp
    Created:	17/5/2020 5:50:14 PM
    Author:     RADE-PC\Rade
	Edited: 17/05/2020
*/

#include <RunningMedian.h>
#include <Adafruit_SHT31.h>
#include <avr/sleep.h>
#include <DS3231.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "SparkFunBME280.h"
#include <HX711.h>
#include <HX711-multi.h>

//Stting up initial state of the wakeup switch
//-------------------------------
int wakeupSwitch = 0;
float tare=0;
//-------------------------------

// Setting up interrupt pins
//-------------------------------
#define wakePin 2					// pin used for waking up the Arduino (interrupt 0)
#define wakePin2 3					// pin used for waking up the Arduino (interrupt 1)
//-------------------------------

//setting up pin for waking up GSM module  
//-------------------------------
const byte gsmWakePin = 4;			// pin  used for waking up the GSM module from sleep mode
//-------------------------------

//Setting up variables for measurement of the battery  
//-------------------------------
int batteryMax = 100;
//-------------------------------

//setting up variables for uploading to IOT sites
//-------------------------------
const String thingSpeakUpadate = "GET http://api.thingspeak.com/update?api_key=03SUMGLJ4MO9KAZK&";
//-------------------------------

//ds3231 working variables
//-------------------------------
char print_date[16];						//char array for date and time 
RTCDateTime dt;								//date and time class for ds3231
DS3231 clock;								//define class for DS3231 clock
byte ADay, AHour, AMinute, ASecond, ABits;	//define clock variables
bool ADy, A12h, Apm;						//define clock variables
//-------------------------------

//Initialisation of HX711 ( -Load cell amplifier- )
//----------------------------------------------
#define DOUT A2							//arduino pin for DOUT signal
#define CLK  A1							//arduino pin for CLK signal
HX711 scale(DOUT, CLK);					//initialisation of HX711
float scaleCalibrationFactor = -20350;	//Calibration factor for the scale 
//-------------------------------

//Variables for BME280 sensor
//-------------------------------
BME280 bme;									// Define Pressure sensor class
//-------------------------------

// initialisation of SHT31 sensor for Temperature and Humid
//----------------------------------------------
Adafruit_SHT31 sht31 = Adafruit_SHT31();
//----------------------------------------------

// initialisation of software serial feature for communicating with gsm module
//----------------------------------------------
SoftwareSerial gsmSerial(8, 7);				// Define pins for communicating with gsm module
//----------------------------------------------

//Setting up median filter library
//----------------------------------------------
RunningMedian measurements = RunningMedian(10);
//----------------------------------------------


float ReadWeight(int loops)
{
	// for resetting purposes becouse scale have error measurement after big weight change
	//for (int i = 0; i < loops; i++)
	//{
	//	weight = weight + scale.get_units(), 3;
	//	//Serial.println(weight);
	//	delay(100);
	//}
	float raw;

	for (int i = 1; i <= loops; i++)
	{
		raw = scale.get_units(loops),3;
		measurements.add(raw);
		delay(100);
	}
	float weightInGrams = measurements.getAverage(3)* 1000;
	//scale.tare();
	measurements.clear();
	return 	weightInGrams;
}

void ResetScale(int mode )
{
	if (mode == 1)
	{
		dt = clock.getDateTime();
		if (dt.hour == 00 && dt.minute==00)
		{
			Serial.println("midnight");
			scale.tare();
		}
	}
	else if (mode == 2)
	{
		scale.tare();
	}
}
void PrintTimeAndDate()
{
	dt = clock.getDateTime();
	sprintf(print_date, "%02d/%02d/%d %02d:%02d:%02d", dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);
	Serial.println(print_date);
}
float ReadBattery(int loops)
{
	analogRead(A0);  // used only for A0 pin to settle. Measurement is ignored
	float voltage;
	int raw;
	for (int i = 1; i <= loops; i++)
	{
		raw = analogRead(A0);
		measurements.add(raw);
	}
	voltage = (measurements.getAverage(3) / 1023) * 1100; //number in get.Average function is for calculating average of three middle measurements
	measurements.clear();


	//map min and max voltage values on analog pin to scale 0% to 100%
	/*

	For clarification:
	Analog reading on defined analog pin A0 is measured based on internal voltage of 1.1V
	If voltage on the pin is 1.1V, arduino will measure 1023 value.
	If voltage on pin is 0 arduino will measure 0 value.

	BUT!!!

	LiIon battery can have voltage values between about 4.3V(full charge) and about 3.7V (empty)

	SO!!!

	We have to scale input voltage on that pin to be at maximum 1.1V when battery is full.
	It can be done with voltage divider (see schematics)
	With battery I use maximum value on the pin is 1035mV (on voltage divider), and I consider battery empty when value falls to 860)

		*/
	voltage = map(voltage, 860, 1035, 0, 100);

	/*if (voltage > batteryMax)
	{
		voltage = batteryMax;
	}
	else
	{
		*/
		batteryMax = voltage;
	//}

	return voltage;
}
float ReadSoil(int loops)
{
	analogReference(DEFAULT);
	analogRead(A3);	// used only for A3 pin to settle. Measurement is ignored
	float soilMoisture;
	float percentRaw;
	float soilRaw;
	for (int i = 1; i <= loops; i++)
	{
		soilRaw = analogRead(A3);
		percentRaw = ((1023 - soilRaw) / 1023) * 100;
		measurements.add(percentRaw);
	}
	soilMoisture = measurements.getAverage(3);
	analogReference(INTERNAL);
	analogRead(A3);
	measurements.clear();
	return soilMoisture;
}
float ReadSht31Temp()
{
	float sht31Temp = sht31.readTemperature();
	return sht31Temp;
}
float ReadSht31Humid()
{
	float sht31Humid = sht31.readHumidity();
	return sht31Humid;
}
float ReadBmeTemperature()
{
	float bmeTemp = bme.readTempC();
	return bmeTemp;
}
float ReadBmeHumid()
{
	float bmeHumid = bme.readFloatHumidity();
	return bmeHumid;
}
float ReadBmePressure()
{
	float bmePressure = bme.readFloatPressure()/100; // 100 Pa = 1 millibar;
	return bmePressure;
}
void DisplayMeasurementsOnSerialMonitor()
{
	PrintTimeAndDate();
	Serial.println("Battery status: " + String(ReadBattery(10))+" %");
	Serial.println("BME sensor temperature: " + String(ReadBmeTemperature())+" C");
	Serial.println("BME sensor humid: " + String(ReadBmeHumid())+" %");
	Serial.println("BME sensor pressure: " + String(ReadBmePressure()) + " mbar");
	Serial.println("SHT31 sensor temperature: " + String(ReadSht31Temp()) + " C");
	Serial.println("SHT31 sensor Humid: " + String(ReadSht31Humid()) + " %");
	Serial.println("Soil moisture: " + String(ReadSoil(10)) + "%");
	Serial.println("Current weight: " + String(ReadWeight(10)) + " gr");
}

void ReadGsmBuffer()
{
	while (gsmSerial.available())
	{
		Serial.write(gsmSerial.read());
	}
}

void PutGsmToSleep()
{
	gsmSerial.println(F("AT+CSCLK=1"));	//prepare for sleep mode when gsmWakePin is High
	ReadGsmBuffer();
	digitalWrite(gsmWakePin, HIGH);
}
void WakeUpGsm()
{
	digitalWrite(gsmWakePin, LOW);	//Awake Gsm
}
void PutScaleToSleep()
{
	scale.power_down();		// Put scale in sleep mode
}
void WakeUpScale()
{
	scale.power_up(); //Awake scale
}
void wakeUp()
{
	Serial.println("Interrupt Fired");
	sleep_disable();
	detachInterrupt(0);
	detachInterrupt(1);
	
}
void wakeUp2()
{
	Serial.println("Interrupt Fired");
	sleep_disable();
	detachInterrupt(0);
	detachInterrupt(1);

	if (wakeupSwitch == 0)
	{
		wakeupSwitch=1;
	}
	else if (wakeupSwitch == 1)
	{
		wakeupSwitch=2;
	}
}
void PutArduinoToSleep()
{
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);		// setting the sleep mode
	sleep_cpu();

	digitalWrite(LED_BUILTIN, LOW);
	delay(1000);

	attachInterrupt(0, wakeUp, LOW);
	attachInterrupt(1, wakeUp2, LOW);
	sleep_enable();
	sleep_mode();

	clock.clearAlarm1();						// Clear the DS3231 alarm (ready for the next triggering)
	clock.clearAlarm2();						// Clear the DS3231 alarm (ready for the next triggering)

}
void SignalForWakeUp()
{
	for (int i=0;i<10;i++)
	{
		digitalWrite(LED_BUILTIN, HIGH);
		delay(50);
		digitalWrite(LED_BUILTIN, LOW);
	}
}
void SetupWakeUpAlarm(int i)
{
	if (i == 1)
	{
		clock.setAlarm1(0, 0, 0, 0, DS3231_MATCH_S);
	}
	else if (i == 30)
	{
		clock.setAlarm1(0, 0, 30, 0, DS3231_MATCH_M_S);
		clock.setAlarm2(0, 0, 00, DS3231_MATCH_M);
	}
	else if (i == 60)
	{
		clock.setAlarm2(0, 0, 00, DS3231_MATCH_M);
	}
	else
	{
		clock.setAlarm1(0, 0, 0, 0, DS3231_MATCH_S);
	}
}

void PurgeGsmBuffer()
{
	while (gsmSerial.available() > 0)
	{
		gsmSerial.read();
	}
}
void InitialGsmSetup()
{
	gsmSerial.println(F("ATE1"));		//
	delay(100);
	gsmSerial.println(F("AT&D2"));		//Switch on Echo
	delay(100);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CMGF=1"));	// put SMS module into Text mode
	delay(100);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSHUT"));	//close the GPRS PDP context
	delay(100);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CSCLK=1"));	//prepare for sleep mode when gsmWakePin is High
	delay(100);
	ReadGsmBuffer();
}

void SetUpDs3231()
{
	clock.armAlarm1(false);
	clock.armAlarm2(false);
	clock.clearAlarm1();
	clock.clearAlarm2();
	clock.begin();
	//clock.setDateTime(__DATE__, __TIME__);

	// Disable square wave output (use alarm triggering)
	// setting 0Eh register on ds3231. very important! see technical specs of ds3231!!
	Wire.beginTransmission(0x68);
	Wire.write(0x0e);
	Wire.write(0b00110111);
	Wire.endTransmission();
}


void UploadToIot()
{
	Serial.println(F("Uploading:..."));
	gsmSerial.println(F("AT+CREG?"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CGATT?"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSHUT"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSTATUS"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPMUX=0"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT + CSTT = \"internet\",\"\",\"\""));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIICR"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIFSR"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSPRT=0"));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\""));
	delay(2000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSEND"));
	delay(2000);
	ReadGsmBuffer();
	gsmSerial.println(thingSpeakUpadate + "&field1=" + String(ReadSht31Temp()) + "&field2=" + String(ReadSht31Humid()) + "&field3=" + String(ReadBattery(10)) + "&field4=" + String(ReadBmeTemperature()) + "&field5=" + String(ReadBmePressure()) + "&field6=" + String(ReadBmeHumid()) + "&field7=" + String(tare + ReadWeight(10)) + "&field8=" + String(ReadSoil(10)));
	delay(2000);
	ReadGsmBuffer();
	gsmSerial.println(String(char(26)));
	delay(1000);
	ReadGsmBuffer();
	gsmSerial.println(F("AT+CIPSHUT"));
	delay(5000);
	ReadGsmBuffer();
	Serial.println(F("Finished uploading!"));
	delay(1000);
}

void setup()
{
	Serial.begin(9600);					//Start serial communication
	analogReference(INTERNAL);

	//setting up pins
	//----------------------------------------------
	pinMode(LED_BUILTIN, OUTPUT);		//led pin set for output
	digitalWrite(LED_BUILTIN, HIGH);	//integrated led set to low state

	pinMode(wakePin, INPUT_PULLUP);		//wakePin(2) set to input with pullUp resistor
	pinMode(wakePin2, INPUT_PULLUP);	//wakePin(3) set to input with pullUp resistor

	pinMode(gsmWakePin, OUTPUT);		//gsmWakePin set to Output
	digitalWrite(gsmWakePin, LOW);		//set gsmWakePin to low state (gsm module awake)
	//----------------------------------------------

	//Setting up HX711 scale
	//----------------------------------------------
	scale.set_scale(scaleCalibrationFactor);	//Calibration Factor obtained from calibrating sketch
	scale.tare();								//Reset the scale to 0  
	//----------------------------------------------

	//Setting up software serial communication
	//---------------------------------------------
	gsmSerial.begin(9600);
	//---------------------------------------------

	// setting up rtc communication trough i2c port (on A4 and A5)
	//---------------------------------------------
	Wire.begin();
	//---------------------------------------------

	//setting up DS3231 (alarms (clear), SQW, clock...)
	//---------------------------------------------
	SetUpDs3231();
	//---------------------------------------------

	//setting up gsm module initial state
	//---------------------------------------------
	InitialGsmSetup();
	//----------------------------------------------

	//Initial display on serial port
	//----------------------------------------------
	PrintTimeAndDate();
	//----------------------------------------------

	//sht31 check
	//----------------------------------------------
	if (!sht31.begin(0x44))  // Set to 0x45 for alternate i2c addr
	{   
		Serial.println(F("Couldn't find valid SHT31"));
	}
	//----------------------------------------------

	//bme280 check
	//----------------------------------------------
	if (!bme.beginI2C())
	{
		Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
	}
	//----------------------------------------------
}

// Add the main program code into the continuous loop() function
void loop()
{
	SetupWakeUpAlarm(30);		//Options are: 1-every minute, 30-every half hour , 60-every hour
	PutScaleToSleep();
	PutGsmToSleep();
	PutArduinoToSleep();

	//----------------------------------------------------------
	//Waking arduino if alarm or button (interrupt 1 or 2) is fired/pressed
	//----------------------------------------------------------
if (wakeupSwitch==0)
	{
		WakeUpGsm();
		WakeUpScale();
		SignalForWakeUp();
		ResetScale(0);
		//DisplayMeasurementsOnSerialMonitor();
		UploadToIot();
	}
	else if(wakeupSwitch==1)
	{
		WakeUpGsm();
		WakeUpScale();
		SignalForWakeUp();
		tare=tare + ReadWeight(10);	//keep value before beekeeper changes anything. If there are previous values add them also
		

		//TODO make some led lights go red or grin to indicate staus of the scale

		//DisplayMeasurementsOnSerialMonitor();
		//UploadToIot();
	}
	else if (wakeupSwitch==2)
	{
		WakeUpGsm();
		WakeUpScale();
		SignalForWakeUp();
		scale.tare();	//tare scales to cancel any changes it came from the beekeeper. In float tare we are keeping changes from the last measure and readings from the scale from this point will be added to that value
		
		//DisplayMeasurementsOnSerialMonitor();
		UploadToIot();
	}

}

