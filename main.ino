//DISPLAY 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//SENSOR DHT11
#include <DFRobot_DHT11.h>
DFRobot_DHT11 DHT;
#define DHT11_PIN 2

//SENSOR DE UMIDADE DO SOLO 
#define PIN_SensorUmidadeSolo A3

//VARIÁVEIS 
String mensagem1;
String mensagem2;
String mensagemX;
int x1;
int x2;
int x3;

//SETUP
void setup() {
    lcd.init();
    lcd.backlight();
    pinMode(PIN_SensorUmidadeSolo, INPUT);
    Serial.begin(9600);
}

//LOOP
void loop() {
    temperatura();
    umidadeDoAr();
    display();
    umidadeDaTerra();
    display();
    bluetoohMensagem();
}

//FUNÇÕES 
void umidadeDaTerra() {
    int umidade = analogRead(PIN_SensorUmidadeSolo);
    int porcento = map(umidade, 0, 1023, 0, 100);
    x3 = porcento;
    mensagem1 = "Umidade do Solo:";
    mensagem2 = String(porcento) + "%";
}
void umidadeDoAr() {
    DHT.read(DHT11_PIN);
    int h = DHT.humidity;
    x1 = h;
    mensagem1 = "Umidade do Ar: ";
    mensagem2 = String(h) + "%";
}
void temperatura() {
    DHT.read(DHT11_PIN);
    int t = DHT.temperature;
    x2 = t;
    mensagem2 = String(t);
    lcd.setCursor(0, 0);
    lcd.print("Temperatura:");
    lcd.setCursor(0, 1);
    lcd.print(mensagem2);
    lcd.write(B11011111);
    lcd.print("C");
    delay(3000);
    lcd.clear();
}
//Inserir informações no Display 
void display() {
    lcd.setCursor(0, 0);
    lcd.print(mensag / em1);
    lcd.setCursor(0, 1);
    lcd.print(mensagem2);
    delay(3000);
    lcd.clear();
}
//Transmitir mensagens via Bluetooth 
void bluetoohMensagem() {
    DHT.read(DHT11_PIN);
    mensagemX = (String)
    "[ Umid do Ar: " + x1 + "% ]   [ Temp: " + x2 + "°C ]   [ Umid da Terra: " + x3 + "% ]";
    Serial.println(mensagemX);
    delay(2000);
}
