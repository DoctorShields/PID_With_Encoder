#include <ClickEncoder.h>
#include <TimerOne.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

//#define SERIAL_MONITOR 1
#define motorPin       3
#define interruptPin   2

ClickEncoder *encoder;
int16_t last, value;

void timerIsr() {
  encoder->service();
}

uint8_t fullArrow[8] = {0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10};
uint8_t emptyArrow[8] = {0x10, 0x8, 0x4, 0x2, 0x4, 0x8, 0x10};
uint8_t smallArrow[8] = {0x0, 0x10, 0x18, 0x1c, 0x18, 0x10};

int cols[] = {0,0,3,8};
int rows[] = {0,1,1,0};
float PID[] = {EEPROM.read(0),EEPROM.read(1),EEPROM.read(2)};
String pidText[] = {"P","I","D"};
int pidIndex = 0;
int cursorPos = 0;
int arrowSize = 2;
int buttonState, lastState, aState, aLastState;
int selected = 0;
int kP, kI, kD;
int setPoint = 0;
float setPointAct;
int maxPID = 999;
int maxSpeed = 13000;
int rotCount = 0; 
int encoderBump;
bool pidToggle = false;
bool saved = false;
float lastRPM = 0;
long now;
float msDelay = 250;
float RPM;
int slots = 5;
volatile int count = 0;
int multiplier = 60*(1000/msDelay)/slots;
float error, lastError;
float P, I, D, drive;

LiquidCrystal_I2C lcd(0x3f, 16, 2);

void setup()
{
	encoder = new ClickEncoder(A1, A0, A2);
	Timer1.initialize(1000);
  	Timer1.attachInterrupt(timerIsr); 
  	last = -1;
	lcd.begin();
	lcd.backlight();
	lcd.createChar(0, fullArrow);
	lcd.createChar(1, emptyArrow);
	lcd.createChar(2, smallArrow);
	lcd.home();
	lcd.print(" PID:OFF ");
	lcd.print("S:    0");
	lcd.setCursor(0,1);
	lcd.print(" kP .");
	lcd.print(fixSpeed(PID[0],2,"0"));
	lcd.print(" A:    0");
	pinMode(interruptPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(interruptPin), blink, RISING);
	Serial.begin(9600);
}

void moveCursor() {
	for(int i=0; i<4; i++) {
		lcd.setCursor(cols[i],rows[i]);
		if(i==cursorPos) 
			lcd.write(arrowSize);
		else
			lcd.print(" ");
	}
}

void loop()
{	
	now = millis();
	if(now > lastRPM + msDelay) {
		calcRPM();
		setMotor();
		lastRPM = now;
	}
	value += encoder->getValue();
	if(value != last){  
		if(value>last)
			encoderBump = 1;
		else
			encoderBump = -1;					
		if(arrowSize == 2) {	// you're moving the cursor
			cursorPos += encoderBump;
			if(cursorPos > 3)
				cursorPos = 0;
			if(cursorPos < 0)
				cursorPos = 4;
		} else {				// you're changing a value
			switch(cursorPos) {
				case 0:			// you're turning PID on or off
					lcd.setCursor(6,0);
					pidToggle = !pidToggle;
					if(pidToggle)
						lcd.print("N ");
					else
						lcd.print("FF");
					break;
				case 1:			// you're cycling through kP, kI, kD
					lcd.setCursor(2,1);
					pidIndex += encoderBump;
					if(pidIndex > 2)
						pidIndex = 0;
					if(pidIndex < 0)
						pidIndex = 2;
					lcd.print(pidText[pidIndex]);
					lcd.print(" .");
					lcd.print(fixSpeed(PID[pidIndex],2,"0"));
					break;
				case 2:			// you're changing PID values 
					lcd.setCursor(5,1);
					if(PID[pidIndex] >= 0 && PID[pidIndex] <= maxPID)
						if(PID[pidIndex] + encoderBump >= 0)
							PID[pidIndex] += encoderBump;
					lcd.print(fixSpeed(PID[pidIndex],2,"0"));
					break;
				case 3:
					lcd.setCursor(11,0);
					if(setPoint < maxSpeed)
						setPoint += encoderBump;
					setPointAct = map(setPoint,0,255,0,maxSpeed);
					lcd.print(fixSpeed(setPointAct,4," "));
					break;
			}
		}
		rotCount ++;
	}
	ClickEncoder::Button b = encoder->getButton();
  	if (b != ClickEncoder::Open) {
  		switch (b) {
  			case ClickEncoder::Clicked:
				if(arrowSize == 0)
					arrowSize = 2;
				else 
					arrowSize = 0;
			break;
			case ClickEncoder::Held:
      			if(!saved) {
      				Serial.println("Saving");
      				for(int i=0; i<3; i++) {
      					EEPROM.write(i,PID[i]);
      				}
      				saved = true;
      				lcd.noBacklight();
      				delay(500);
					lcd.backlight();
      			}
      		break;
      		case ClickEncoder::Released:
      			saved = false;
      		break;
      	}

	}
	moveCursor();
	last = value;
}

String fixSpeed(int speed, int max, String what) {
	String str = "";
	for(int i=log10(speed); i<max; i++)
		str += what;
	str += String(speed);
	return str;
}

void calcRPM() {
	detachInterrupt(digitalPinToInterrupt(interruptPin));
	RPM = count*multiplier;
	lcd.setCursor(11,1);
	lcd.print(fixSpeed(RPM,4," "));
	lcd.print("    ");
	count = 0;
	attachInterrupt(digitalPinToInterrupt(interruptPin), blink, RISING);
}

void setMotor() {
	if(pidToggle) {
		error = setPointAct - RPM;
		P = error * PID[0]/1000;
		I = I + error * PID[1]/1000;
		D = (error-lastError)/(msDelay/1000) * PID[2]/1000;
		drive = P + I + D;
		lastError = error;
		if(drive > 255)
			drive = 255;
		if(drive < 0)
			drive = 0;
		//printStuff();
		analogWrite(motorPin, drive);
	} else {
		analogWrite(motorPin, setPoint);
	}
}
void blink() {
	count++;
}

void printStuff() {
	#ifdef SERIAL_MONITOR
		Serial.print("set: ");
		Serial.print(setPointAct);
		Serial.print("\t");
		Serial.print("rpm: ");
		Serial.print(RPM);
		Serial.print("\t");
		Serial.print("error: ");
		Serial.print(error);
		Serial.print("\t");
		Serial.print("P: ");
		Serial.print(P);
		Serial.print("\t");
		Serial.print("I: ");
		Serial.print(I);
		Serial.print("\t");
		Serial.print("D: ");
		Serial.print(D);
		Serial.print("\t");
		Serial.print("drive: ");
		Serial.println(drive);
	#else
		Serial.print(setPointAct);
		Serial.print(",");
		Serial.print(RPM);
		Serial.print(",");
		Serial.print(P*5);
		Serial.print(",");
		Serial.print(I*5);
		Serial.print(",");
		Serial.println(D*5);
	#endif
}