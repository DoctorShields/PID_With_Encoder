#define WITH_LCD 1

#include <ClickEncoder.h>
#include <TimerOne.h>

#ifdef WITH_LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x3f, 16, 2);
#endif

ClickEncoder *encoder;
int16_t last, value;

void timerIsr() {
  encoder->service();
}

#ifdef WITH_LCD
  void displayAccelerationStatus() {
    lcd.setCursor(0, 1);  
    lcd.print("Acceleration ");
    lcd.print(encoder->getAccelerationEnabled() ? "on " : "off");
  }
#endif

void setup() {
  Serial.begin(9600);
  encoder = new ClickEncoder(A1, A0, A2);

#ifdef WITH_LCD
  lcd.begin();
  lcd.backlight();
  displayAccelerationStatus();
#endif

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 
  
  last = -1;
}

void loop() {  
  value += encoder->getValue();
  
  if (value != last) {
    last = value;
    Serial.print("Encoder Value: ");
    Serial.println(value);
  #ifdef WITH_LCD
      lcd.setCursor(0, 0);
      lcd.print("         ");
      lcd.setCursor(0, 0);
      lcd.print(value);
  #endif
  }
  
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    Serial.print("Button: ");
    #define VERBOSECASE(label) case label: Serial.println(#label); break;
    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);
      VERBOSECASE(ClickEncoder::Held)
      VERBOSECASE(ClickEncoder::Released)
      VERBOSECASE(ClickEncoder::Clicked)
      case ClickEncoder::DoubleClicked:
          Serial.println("ClickEncoder::DoubleClicked");
          encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
          Serial.print("  Acceleration is ");
          Serial.println((encoder->getAccelerationEnabled()) ? "enabled" : "disabled");
          #ifdef WITH_LCD
            displayAccelerationStatus();
          #endif
        break;
    }
  }    
}

