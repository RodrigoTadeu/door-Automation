#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
// #include <ESP32Servo.h>

void configureDisplay();
void print_wakeup_reason();

int NewContador;                 // variavel para marcar tempo de inicio do deep sleep
RTC_DATA_ATTR int bootCount = 0; // RTC_DATA_ATTR aloca a variável na memoria RTC
// para contar o numero que vees que o sistema deu boot de fomra interna (a depender do deeep sleep)
unsigned long debounce;  // Variavel para determinar o pressionar de uma tecla
int debounceTime = 4000; // Variavel para determinar tempo de permissão para que uma nova tecla seja precionada

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
int pos = 0, pos1;
int unlock = 21;
char ok = '2';
int led = 2;
// int servoPin = 18;
// Servo servo;

// Motor A
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 14;

// Setting PWM properties
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;
const int pinoChave = 33; // PINO DIGITAL UTILIZADO PELA CHAVE FIM DE CURSO

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    if (millis() > debounce)
    {
      debounce = millis() + debounceTime;
      std::string value = pCharacteristic->getValue();
      Serial.print("O valor anterior: ");
      Serial.println(ok);
      Serial.print("O valor recebido pelo app: ");
      Serial.println(value[0]);
      if (value.length() > 0)
      {
        if ((value[0] == '1') && (value[0] != ok))
        {
          digitalWrite(led, HIGH); // LIGA o LED
          Serial.println("Sentido Horário");
          digitalWrite(motor1Pin1, LOW);
          digitalWrite(motor1Pin2, HIGH);
          ok = value[0];
        }

        if ((value[0] == '0') && (value[0] != ok))
        {
          digitalWrite(led, LOW); // DESLIGA o LED
          Serial.println("Sentido Anti-horário");
          digitalWrite(motor1Pin1, HIGH);
          digitalWrite(motor1Pin2, LOW);
          ok = value[0];
        }

        if ((value[0] == '3') && (value[0] != ok))
        {
          digitalWrite(led, LOW); // DESLIGA o LED
          Serial.println("Parado");
          digitalWrite(motor1Pin1, LOW);
          digitalWrite(motor1Pin2, LOW);
          ok = value[0];
        }
      }
    }
  }
};

void setup()
{
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  pinMode(pinoChave, INPUT_PULLUP); // DEFINE O PINO COMO ENTRADA / "_PULLUP" É PARA ATIVAR O RESISTOR INTERNO
  // configure LED PWM functionalitites
  ledcSetup(pwmChannel, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(enable1Pin, pwmChannel);

  Serial.begin(115200);
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  pinMode(led, OUTPUT);
  pinMode(unlock, OUTPUT);
  digitalWrite(unlock, LOW);
  // servo.attach(servoPin);
  BLEDevice::init("Cermob");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  // servo.write(pos);

  delay(1000);

  ++bootCount;                                 // contador da quantidade de boot's ocorridos
  NewContador = 0;                             // contador de tempo limite interno para que seja possivel ocorrer o deep sleep
  configureDisplay();                          // função para imprimir quantidade de vezes em que houve boot pós deep sleep
  print_wakeup_reason();                       // função para informar o motivo do boot
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1); // 1 = High, 0 = Low Uso de gpio 4 para despertar o esp
}

void loop()
{
  // pos1 = servo.read();
  // Serial.print(pos1);

  if (deviceConnected)
  {
    digitalWrite(unlock, HIGH); // impede que a fechadura (chave) seja movida
    if (digitalRead(pinoChave) == LOW && digitalRead(led) == HIGH)
    {
      digitalWrite(motor1Pin1, LOW);
      digitalWrite(motor1Pin2, LOW);
      pCharacteristic->setValue("Porta Trancada");
      pCharacteristic->notify();
      delay(220);
    }

    if (digitalRead(pinoChave) == LOW && digitalRead(led) == LOW)
    {
      digitalWrite(motor1Pin1, LOW);
      digitalWrite(motor1Pin2, LOW);
      pCharacteristic->setValue("Porta Destrancada");
      pCharacteristic->notify();
      delay(220);
    }
    /*if (digitalRead(led) == 1 || servo.read() >= 120) {
      pCharacteristic->setValue("Porta Trancada");
      pCharacteristic->notify();
      delay(220);
    } else {
      pCharacteristic->setValue("Porta Destrancada");
      pCharacteristic->notify();
      delay(220);
    }*/

    NewContador = 0;
  }

  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
    digitalWrite(unlock, LOW); // permite que a fechadura (chave) se mova livremente
    digitalWrite(led, LOW);    // DESLIGA o LED
                               // servo.write(0);
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
  Serial.print("Tempo passado:");
  Serial.println(NewContador);
  NewContador++;

  if (NewContador == 11)
  {
    deviceConnected = false;
    Serial.println("entrando em modo sleep");
    esp_deep_sleep_start(); // força o ESP32 entrar em modo SLEEP
  }
  delay(1000);
}

void configureDisplay()
{
  Serial.println("BOOT NUM:");
  Serial.println(String(bootCount).c_str());
  Serial.println("MOTIVO:");
}

// função para imprimir a causa do ESP32 despertar
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  String reason = "";

  wakeup_reason = esp_sleep_get_wakeup_cause(); // recupera a causa do despertar

  switch (wakeup_reason)
  {
  case 1:
    reason = "EXT1 RTC_CNTL";
    break;
  case 2:
    reason = "EXT0 RTC_IO BTN";
    break;
  case 3:
    reason = "TOUCHPAD";
    break;
  case 4:
    reason = "TIMER";
    break;
  case 5:
    reason = "ULP PROGRAM";
    break;
  default:
    reason = "NO DS CAUSE";
    break;
  }
  Serial.println(reason);
}
