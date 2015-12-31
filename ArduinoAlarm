int sensorPin = 0;
int buttonState = 0;
int alarm = 1;
int TxPinAndroid = 0;
int RxPinAndroid = 1;
int TxPinServant = 19;
int RxPinServant = 18;
int KeyPinServant = 9;
int LedPinServant = 8;
int buttonPush = 6;
int light = 13;
int receiveLight = 7;

unsigned char convert = 48;
int digit1, digit2, digit3, digit4 = 0;
String str;


#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
void setup() {
  
  Serial.begin(9600);
  Serial1.begin(9600);
  
  pinMode(light, OUTPUT);
  pinMode(buttonPush, INPUT);
  pinMode(receiveLight, OUTPUT);
  lcd.begin(16,1);
}

void loop() {
    lcd.setCursor(0, 1);
    if(Serial1.available())
    {
      convert = Serial1.read();
      
      Serial1.write(convert);
      str = String(convert - 48);
      
      lcd.print(str);
      
      lcd.print("   inches   ");
      
      delay(100);
    }
    else
    {
      digitalWrite(receiveLight, HIGH);
    }
    delay(100);
    
    buttonState = digitalRead(buttonPush);
    if(buttonState == HIGH)
    {
      digitalWrite(light, HIGH);
      Serial.write("#go;");
    }
    else
    {
      digitalWrite(light, LOW);
      Serial.write("#stop;");
    }    
}
