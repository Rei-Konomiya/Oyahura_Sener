#include "token.h"

#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#define JST 3600* 9

#define DOOR_PIN 22    //確定
#define SENSER_PIN 19  //確定
#define LED_PIN 12     //確定
#define SPKR_PIN 26    //確定
#define VOLUME_PIN 25  //確定
#define SW_PIN 4       //確定
#define INOUT_PIN 18   //確定
#define WAIT_PIN 17    //確定

// スピーカーのPWM出力関連を定義
#define LEDC_CHANNEL_0 0      // LEDCのPWMチャネル0から15
#define LEDC_TIMER_13_BIT 13  // LEDCタイマーの精度13ビット
#define LEDC_BASE_FREQ 5000   // LEDCのベース周波数5000Hz

int LED_COUNT = 12;
int ledNum = 0;

boolean b_play = false;  //ブザーをON・OFF
int b_duration = 0;      //ブザーの鳴る時間をカウント
int b_volume = 100;      //音量
int b_tone = 0;

boolean door = false;
boolean doorOld = false;
boolean senser = false;
boolean senserOld = false;

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

String inMode = "1";
String inColor = "red";
String inVolume = "100";
String inTiming = "mix";
String outMode = "0";
String outColor = "yellow";
String outVolume = "25";
String outTiming = "mix";

boolean swData;
boolean swDataOld = false;
boolean inOut = true;  //true = IN , false = OUT

const char* host  = "notify-api.line.me";

//以下BLE接続
#define DEVICE_NAME "BLE-ESP32"
#define SERVICE_UUID "d6f19b1b-3b62-4d39-ad7d-ec2dddbeb0ee"  // サービスのUUID
// ESP32からスマホにセンサーの値を送信するためのキャラスタリスティックを識別するためのUUID
#define SENSOR_INFO_CHARACTERISTIC_UUID "78b5f08c-a793-4127-9a94-85a246056038"

// Bluetoothのパケットサイズ
#define MTU_SIZE 200

BLECharacteristic *sensorInfoCharacteristic;
BLECharacteristic *needInfoCharacteristic;
BLECharacteristic *colorInfoCharacteristic;
BLECharacteristic *buzzerInfoCharacteristic;
BLECharacteristic *timeInfoCharacteristic;
BLECharacteristic *goNeedInfoCharacteristic;
BLECharacteristic *goTimeInfoCharacteristic;
BLECharacteristic *goColorInfoCharacteristic;
BLECharacteristic *goBuzzerInfoCharacteristic;

bool deviceConnected = false;
bool connectingWifi = false;



// Bluetoothの接続状態の結果がこのクラスのメソッドが呼び出されることによって返ってくる(Observerパターン)
class ConnectionCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    pServer->updatePeerMTU(pServer->getConnId(), 200);
    Serial.println("接続された");
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("接続解除された");
  }
};
void startBluetooth() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer *pServer = BLEDevice::createServer();

  pServer->setCallbacks(new ConnectionCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  doPrepare(pService);
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
}

// 部屋
class NeedInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから必要か情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    inMode = value.c_str();
  }
};

class timeInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから通知情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    inTiming = value.c_str();
  }
};

class BuzzerInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホからブザー情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    inVolume = value.c_str();
  }
};

class ColorInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから色情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    inColor = value.c_str();
    Serial.println("内側情報を受け取りました");
  }
};

// 外
class GoNeedInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから必要か情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    outMode = value.c_str();
  }
};

class GoTimeInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから通知情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    outTiming = value.c_str();
  }
};

class GoBuzzerInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホからブザー情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    outVolume = value.c_str();
  }
};

class GoColorInfoBleCallback : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pC) {
  }

  // スマホから色情報が書き込まれた場合ここが呼び出される
  void onWrite(BLECharacteristic *pC) {
    std::string value = pC->getValue();
    outColor = value.c_str();
    Serial.println("外側情報を受け取りました");
  }
};

// 部屋
// スマホから必要か情報を送信するためのキャラスタリスティックを識別するためのUUID
//#define NEED_INFO_CHARACTERISTIC_UUID "2dff4dad-bfa5-4265-bdfc-f0d0a85dd2e0"
// スマホから通知の情報を送信するためのキャラスタリスティックを識別するためのUUID
#define TIME_INFO_CHARACTERISTIC_UUID "391c0103-9e67-47b0-9a73-a4481a1768b5"
// スマホから色情報を送信するためのキャラスタリスティックを識別するためのUUID
#define COLOR_INFO_CHARACTERISTIC_UUID "30dfe503-7a09-4c5f-9a9f-352d773666d3"
// スマホからブザー情報を送信するためのキャラスタリスティックを識別するためのUUID
#define BUZZER_INFO_CHARACTERISTIC_UUID "14594b63-f642-4da4-bdf8-c44165677d96"

// 外
// スマホから必要かの情報を送信するためのキャラスタリスティックを識別するためのUUID
#define GO_NEED_INFO_CHARACTERISTIC_UUID "8da3e607-bda7-42b6-8123-53b4cc9d3318"
// スマホから通知の情報を送信するためのキャラスタリスティックを識別するためのUUID
#define GO_TIME_INFO_CHARACTERISTIC_UUID "07438c11-eba9-4487-9871-87fc62fd517d"
// スマホから色情報を送信するためのキャラスタリスティックを識別するためのUUID
#define GO_COLOR_INFO_CHARACTERISTIC_UUID "b1b0e200-8854-4931-a1f2-08b6502be9b6"
// スマホからブザー情報を送信するためのキャラスタリスティックを識別するためのUUID
#define GO_BUZZER_INFO_CHARACTERISTIC_UUID "1373250b-45ac-4213-a5b7-c91944cf57ca"


void doPrepare(BLEService *pService) {

  // 部屋
  // 必要か情報をスマホから書き込むためのキャラクタリスティックを作成する
  /*needInfoCharacteristic = pService->createCharacteristic(
                                          NEED_INFO_CHARACTERISTIC_UUID, 
                                          BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                          );*/
  // 通知情報をスマホから書き込むためのキャラクタリスティックを作成する
  timeInfoCharacteristic = pService->createCharacteristic(
    TIME_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  // ブザー情報をスマホから書き込むためのキャラクタリスティックを作成する
  buzzerInfoCharacteristic = pService->createCharacteristic(
    BUZZER_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  // 色情報をスマホから書き込むためのキャラクタリスティックを作成する
  colorInfoCharacteristic = pService->createCharacteristic(
    COLOR_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  //外
  // 必要か情報をスマホから書き込むためのキャラクタリスティックを作成する
  goNeedInfoCharacteristic = pService->createCharacteristic(
    GO_NEED_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  // 通知情報をスマホから書き込むためのキャラクタリスティックを作成する
  goTimeInfoCharacteristic = pService->createCharacteristic(
    GO_TIME_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  // ブザー情報をスマホから書き込むためのキャラクタリスティックを作成する
  goBuzzerInfoCharacteristic = pService->createCharacteristic(
    GO_BUZZER_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  // 色情報をスマホから書き込むためのキャラクタリスティックを作成する
  goColorInfoCharacteristic = pService->createCharacteristic(
    GO_COLOR_INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);


  // 部屋
  //needInfoCharacteristic->setCallbacks(new NeedInfoBleCallback());
  timeInfoCharacteristic->setCallbacks(new timeInfoBleCallback());
  buzzerInfoCharacteristic->setCallbacks(new BuzzerInfoBleCallback());
  colorInfoCharacteristic->setCallbacks(new ColorInfoBleCallback());

  // 外
  goNeedInfoCharacteristic->setCallbacks(new GoNeedInfoBleCallback());
  goTimeInfoCharacteristic->setCallbacks(new GoTimeInfoBleCallback());
  goBuzzerInfoCharacteristic->setCallbacks(new GoBuzzerInfoBleCallback());
  goColorInfoCharacteristic->setCallbacks(new GoColorInfoBleCallback());
}


void send_line(String message) {
  time_t t;
  struct tm *tm;
  static const char *wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};

  t = time(NULL);
  tm = localtime(&t);
  Serial.printf(" %04d/%02d/%02d(%s) %02d:%02d:%02d\n",
        tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
        wd[tm->tm_wday],
        tm->tm_hour, tm->tm_min, tm->tm_sec);

  message = String(tm->tm_hour) +"時 "+ String(tm->tm_min) +"分、"+ message;

  // HTTPSへアクセス（SSL通信）するためのライブラリ
  WiFiClientSecure client;

  // サーバー証明書の検証を行わずに接続する場合に必要
  client.setInsecure();

  Serial.println("Try");

  //LineのAPIサーバにSSL接続（ポート443:https）
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return;
  }
  Serial.println("Connected");

  // リクエスト送信
  String query = "message=" + message;
  String request = String("") +
                  "POST /api/notify HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "Authorization: Bearer " + token + "\r\n" +
                  "Content-Length: " + String(query.length()) +  "\r\n" + 
                  "Content-Type: application/x-www-form-urlencoded\r\n\r\n" +
                  query + "\r\n";
  client.print(request);

  // 受信完了まで待機
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  String line = client.readStringUntil('\n');
  Serial.println(line);
}

// 嫌がらせ
void dislike(String colors) {
  uint32_t none = pixels.Color(0, 0, 0);
  uint32_t red = pixels.Color(120, 0, 0);
  uint32_t yellow = pixels.Color(230, 60, 0);
  uint32_t blue = pixels.Color(0, 0, 120);

  ++ledNum %= LED_COUNT;

  pixels.clear();
  for (int i = 0; i < (LED_COUNT / 3); i++) {
    if (colors == "red") {
      pixels.setPixelColor((i + ledNum) % LED_COUNT, red);
    } else if (colors == "yellow") {
      pixels.setPixelColor((i + ledNum) % LED_COUNT, yellow);
    } else if (colors == "blue") {
      pixels.setPixelColor((i + ledNum) % LED_COUNT, blue);
    }
  }
  pixels.show();  // 設定の反映
}

int b_toneMin = 260;  //音階の聞こえる範囲の一番下 C4
int b_toneMax = 4186;

void setBuzzer(boolean mode) {
  if (mode) {
    b_play = true;   //ブザーをON
    b_duration = 0;  //鳴る時間を0に戻す
  } else {
    b_play = false;  //ブザーをOFF
    ledcWriteTone(LEDC_CHANNEL_0, 0);
  }
}

void adminBuzzer() {
  if (b_duration == 0) {
    b_play = !b_play;
    b_tone = 500;
  }
  runBuzzer(10);
}

void runBuzzer(int Delay) {
  ++b_duration %= (500 / Delay);
  if (b_play && b_volume > 0) {
    b_tone += (b_toneMax - b_toneMin) / (500 / Delay);
    dacWrite(VOLUME_PIN, b_volume/100.0 * 255);
    ledcWriteTone(LEDC_CHANNEL_0, b_tone);
  } else {
    ledcWriteTone(LEDC_CHANNEL_0, 0);
  }
}

boolean wifiMode;
boolean wifiModeOld = true;

void setup() {
  Serial.begin(115200);

  pinMode(DOOR_PIN, INPUT);    // センサーを入力に設定
  pinMode(SENSER_PIN, INPUT);  // センサーを入力に設定
  pinMode(SPKR_PIN, OUTPUT);   // スピーカーを出力に設定
  pinMode(VOLUME_PIN, OUTPUT);
  pinMode(SW_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(INOUT_PIN, OUTPUT);
  pinMode(WAIT_PIN, OUTPUT);

  digitalWrite(WAIT_PIN, HIGH);

  pixels.begin();
  pixels.clear();
  pixels.show();
  Serial.println("Start program ...");
  // put your setup code here, to run once:
  startBluetooth();

  // スピーカー用のチャネル設定(チャネル番号, ベース周波数, 精度ビット数)
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  // チャネルにスピーカーピンを接続(ピン番号, チャネル番号)
  ledcAttachPin(SPKR_PIN, LEDC_CHANNEL_0);

  ledcWriteTone(LEDC_CHANNEL_0, 0);  //消音

  // WiFi接続
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int connectTime = 0;
  // WiFiの接続状態を確認
  while (WiFi.status() != WL_CONNECTED && connectTime < 30) {
    delay(1000);
    Serial.print(".");
    connectTime++;
  }
  Serial.println("");
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(WAIT_PIN, LOW);
  } else {
    Serial.println("WiFi disconnected.");
  }
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  Serial.print("\n\nStart\n");
}

int loopCount = 0;

void loop() {
  wifiMode = (WiFi.status() == WL_CONNECTED);
  if(!wifiMode && wifiModeOld){
    loopCount = 0;
    Serial.println("Wifi disConnected");
  }

  if(!wifiMode && loopCount == 0){
    digitalWrite(WAIT_PIN, HIGH);
    digitalWrite(INOUT_PIN, LOW);
    pixels.clear();
    pixels.show();
    ledcWriteTone(LEDC_CHANNEL_0, 0);  //消音
    
    // WiFi接続
    Serial.print("\nReConnecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int connectTime = 0;

    // WiFiの接続状態を確認
    while (!wifiMode && connectTime < 30) {
      delay(1000);
      Serial.print(".");
      connectTime++;
    }

    wifiMode = (WiFi.status() == WL_CONNECTED);
    Serial.println("");
    if(wifiMode){
      Serial.println("WiFi connected.");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      digitalWrite(WAIT_PIN, LOW);
    } else {
      Serial.println("WiFi disconnected.");
    }

    configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  }

  if(wifiMode) {
    digitalWrite(WAIT_PIN, LOW);
  }else{
    if(loopCount % 200 == 0) digitalWrite(WAIT_PIN, HIGH);
    if(loopCount % 200 == 100) digitalWrite(WAIT_PIN, LOW);
  }

  swData = digitalRead(SW_PIN);
  if (swData && !swDataOld) {
    inOut = !inOut;
    door = false;
    senser = false;

    String str;
    if (inOut) {
      str = "Inside";
    } else {
      str = "Oustside";
    }
    Serial.printf("所有者の状態が「%s」になりました\n", str);
    pixels.clear();
    pixels.show();
  }

  if (inOut) {
    digitalWrite(INOUT_PIN, HIGH);
    if (inMode == "1") {
      if (inTiming == "before" || inTiming == "mix") {
        senser = digitalRead(SENSER_PIN);
      }
      if (inTiming == "after" || inTiming == "mix") {
        door = !digitalRead(DOOR_PIN);
      }
    }

    if (inVolume == "0") {
      b_volume = 0;
    } else if (inVolume == "25") {
      b_volume = 25;
    } else if (inVolume == "50") {
      b_volume = 50;
    } else if (inVolume == "75") {
      b_volume = 75;
    } else if (inVolume == "100") {
      b_volume = 100;
    }

    //人感センサー
    if (senser && !senserOld) {
      Serial.println("IN...SenserOn");
      setBuzzer(senser);
      if(WL_CONNECTED) send_line("誰かが部屋に近づいてきました");
    } else if (!senser && senserOld) {
      Serial.println("IN...SenserOn");
      setBuzzer(senser);
    }
    if (senser) {
      adminBuzzer();
      dislike(inColor);
    }

    //ドアセンサー
    if (door && !doorOld) {
      Serial.println("IN...DoorOpen");
      setBuzzer(door);
      if(WL_CONNECTED) send_line("ドアが開きました");
    } else if (!door && doorOld) {
      Serial.println("IN...DoorClose");
      setBuzzer(door);
    }
    if (door) {
      adminBuzzer();
      dislike(inColor);
    }

  } else {
    digitalWrite(INOUT_PIN, LOW);
    if (outMode == "1") {
      if (outTiming == "before" || outTiming == "mix") {
        senser = digitalRead(SENSER_PIN);
      }
      if (outTiming == "after" || outTiming == "mix") {
        door = !digitalRead(DOOR_PIN);
      }
    }
    if (outVolume == "0") {
      b_volume = 0;
    } else if (outVolume == "25") {
      b_volume = 25;
    } else if (outVolume == "50") {
      b_volume = 50;
    } else if (outVolume == "75") {
      b_volume = 75;
    } else if (outVolume == "100") {
      b_volume = 100;
    }
    //人感センサー
    if (senser && !senserOld) {
      Serial.println("OUT...SenserOn");
      setBuzzer(senser);
      if(WL_CONNECTED) send_line("誰かが部屋に近づいてきました");
    } else if (!senser && senserOld) {
      Serial.println("OUT...SenserOff");
      setBuzzer(senser);
    }
    if (senser) {
      adminBuzzer();
      dislike(outColor);
    }

    //ドアセンサー
    if (door && !doorOld) {
      Serial.println("OUT...DoorOpen");
      setBuzzer(door);
      if(WL_CONNECTED) send_line("ドアが開きました");
    } else if (!door && doorOld) {
      Serial.println("OUT...DoorClose");
      setBuzzer(door);
    }
    if (door) {
      adminBuzzer();
      dislike(outColor);
    }
  }

  doorOld = door;
  senserOld = senser;
  swDataOld = swData;
  wifiModeOld = wifiMode;
  if (!doorOld && !senserOld) {
    pixels.clear();
    pixels.show();
  }

  ++loopCount %= (1000 /10 *3 *60);
  delay(10);
}