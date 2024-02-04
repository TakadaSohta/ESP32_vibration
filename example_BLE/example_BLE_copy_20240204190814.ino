#include <NimBLEDevice.h>

//firebase
// 今回のライブラリのメイン
#include <Firebase_ESP_Client.h>
// define the api key
const String apiKey = "AIzaSyCDI2DvjyWygrG73i8KsfWrEfYVJxjGL98";
// define the RTDB URL
const String databaseURL = "https://test-dff46-default-rtdb.firebaseio.com";
// define the user Email and password tha alreadey registrerd or added in my project
const String userEmail = "takada@ah.iit.tsukuba.ac.jp";
const String userPassword = "EnWeSeAd90";

// Firebase data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// アカウントへのサインインを確認する Firebase.ready() を loop() 行うので、
// 永遠とデータを送受信してしまわないようにする。
bool taskCompleted = false;

unsigned long tmr;

// メモリを節約するために不要なクラスを除外する
#define ENABLE_FIRESTORE
#define ENABLE_FCM
#define ENABLE_GC_STORAGE
#define ENABLE_FB_FUNCTIONS

//音声
#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_ADDRESS 0x40

//Wifi設定
#include <WiFi.h>
//const char* ssid      = "AppliedHapticsLab";
//const char* password  = "Touch2020!";
const char* ssid      = "Buffalo-A-2B1E";
const char* password  = "n83vtfssn3r7u";

WiFiServer server(80);

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define COMPLETE_LOCAL_NAME "esp-test-device"
#define SERVICE_UUID "3c3996e0-4d2c-11ed-bdc3-0242ac120002"
#define CHARACTERISTIC_UUID "3c399a64-4d2c-11ed-bdc3-0242ac120002"
#define CHARACTERISTIC_UUID_NOTIFY "3c399c44-4d2c-11ed-bdc3-0242ac120002"

#define LED_PIN 10 //LEDピン

bool PlayFlag = false;
bool LEDFlag = false;

NimBLECharacteristic *pNotifyCharacteristic;
NimBLEServer *pServer = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
String stateValue = "0";
uint8_t data_buff[2];

uint8_t byteArray[4];

String stringValue = "";
int Heartrate = 0;

class ServerCallbacks : public NimBLEServerCallbacks
{
  //接続時
  void onConnect(NimBLEServer *pServer)
  {
    Serial.println("Client connected");
    deviceConnected = true;
  };
  //切断時
  void onDisconnect(NimBLEServer *pServer)
  {
    Serial.println("Client disconnected - start advertising");
    deviceConnected = false;
    NimBLEDevice::startAdvertising();
  };
  void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc)
  {
    //Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
  };
  // Passのリクエスト
  uint32_t onPassKeyRequest()
  {
    Serial.println("Server Passkey Request");
    return 1234;
  };
  //確認
  bool onConfirmPIN(uint32_t pass_key)
  {
    Serial.print("The passkey YES/NO number: ");
    Serial.println(pass_key);
    return true;
  };
  //認証完了時の処理
  void onAuthenticationComplete(ble_gap_conn_desc *desc)
  {
    if (!desc->sec_state.encrypted)
    {
      NimBLEDevice::getServer()->disconnect(desc->conn_handle);
      Serial.println("Encrypt connection failed - disconnecting client");
      return;
    }
    Serial.println("Starting BLE work!");
  };
};

// Bluetooth LE Recive
class BLECallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      String keyLockedState = value.c_str();
      Serial.println(keyLockedState);
      if (keyLockedState == "OFF")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,HIGH);
        PlayFlag = false;
        taskCompleted = false;
      }
      else if (keyLockedState == "ON")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,LOW);
        PlayFlag = true;
       
      }

      if (keyLockedState == "OK")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,LOW);
        LEDFlag = true;
        Serial.println("LEDON");
      }
      else if (keyLockedState == "NO")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,LOW);
        LEDFlag = false;
      }
      if (keyLockedState == "WIFI")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,LOW);
        WiFi.reconnect();

      }
      else if (keyLockedState == "WIFIOFF")
      {
        stateValue = keyLockedState;
        pNotifyCharacteristic->setValue(stateValue);
        pNotifyCharacteristic->notify();
        //digitalWrite(SERVO_PIN,LOW);
        WiFi.disconnect(true);
      }
    }
  }
};

// Bluetooth LE loop
void loopBLE()
{
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);
    pServer->startAdvertising();
    Serial.println("restartAdvertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting NimBLE Server");
  // CompleteLocalNameのセット
  NimBLEDevice::init(COMPLETE_LOCAL_NAME);
  // TxPowerのセット
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  //セキュリティセッティング
  //bonding,MITM,sc
  //ボンディング有り、mitm有り,sc有り
  NimBLEDevice::setSecurityAuth(false, false, false);
  // PassKeyのセット
  //NimBLEDevice::setSecurityPasskey(1234);
  //パラメータでディスプレイ有りに設定
  //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // RxCharacteristic
  NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
  // NotifyCharacteristic Need Enc Authen
  pNotifyCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_NOTIFY, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);

  // RxCharacteristicにコールバックをセット
  pRxCharacteristic->setCallbacks(new BLECallbacks());
  // Serivice開始
  pService->start();
  //アドバタイズの設定
  NimBLEAdvertising *pNimBleAdvertising = NimBLEDevice::getAdvertising();
  //アドバタイズするUUIDのセット
  pNimBleAdvertising->addServiceUUID(SERVICE_UUID);
  //アドバタイズにTxPowerセット
  pNimBleAdvertising->addTxPower();

  //アドバタイズデータ作成
  NimBLEAdvertisementData advertisementData;
  //アドバタイズにCompleteLoacaNameセット
  advertisementData.setName(COMPLETE_LOCAL_NAME);
  //アドバタイズのManufactureSpecificにデータセット
  advertisementData.setManufacturerData("NORA");
  // ScanResponseを行う
  pNimBleAdvertising->setScanResponse(true);
  // ScanResponseにアドバタイズデータセット
  pNimBleAdvertising->setScanResponseData(advertisementData);
  //アドバタイズ開始

  pNimBleAdvertising->start();
  Serial.println("first startAdvertising");

  //ピンの初期化
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
        
  server.begin();

  //Firebase
  // Firebaseの初期化をする
  // APIKeyを割り当てる 
  config.api_key = apiKey;

  // ユーザー認証情報を割り当てる 
  auth.user.email = userEmail;
  auth.user.password = userPassword;

  // Real time data baseのURLを割り当てる 
  config.database_url = databaseURL;

  Firebase.begin(&config, &auth);

  // これを設定すると、切断されたら自動的に再接続する
  Firebase.reconnectWiFi(true);

  tmr = millis();
  
}

void loop()
{
  loopBLE();

  if(PlayFlag){
    

    // 1秒ごとに値を更新して通知
    if (millis() % 10 == 0) {

      stringValue = String("inter")+"\n"+String("Voltage*24")+"\n"+String("rotation");

      // キャラクタリスティックに新しい値をセット
      pNotifyCharacteristic->setValue(stringValue);

      // 通知を送信
      pNotifyCharacteristic->notify();
      Serial.println(stringValue);
    }
    
  }

  if(LEDFlag)
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else if(LEDFlag == false) digitalWrite(LED_PIN, LOW);
  delay(1);

  //Wifi用
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("Click <a href=\"/H\">here</a> to turn the LED on pin 5 on.<br>");
            client.print("Click <a href=\"/L\">here</a> to turn the LED on pin 5 off.<br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(LED_PIN, HIGH);               // GET /H turns the LED on
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(LED_PIN, LOW);                // GET /L turns the LED off
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }

  //Firebase
  if (Firebase.ready() && !taskCompleted) {
    taskCompleted = true;
    Serial.printf("Example... %s\n", Firebase.RTDB.getDouble(&fbdo, "/Heartbeat/Watch1/HeartRate") ? "ok" : fbdo.errorReason().c_str());
    //printf("Heartrate: %lld\n", fbdo.to<uint64_t>());
    Heartrate = static_cast<int>(fbdo.to<uint64_t>());
    Serial.printf("Heartrate: %d\n", Heartrate);
  }

  if(millis() - tmr > 2000)
  {
    taskCompleted = false;
    tmr = millis();
  }
}