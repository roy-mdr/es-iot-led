/*****************************************************************************/
/*****************************************************************************/
/*               STATUS: WORKING                                             */
/*            TESTED IN: WeMos D1 mini                                       */
/*                   AT: 2022.07.29                                          */
/*     LAST COMPILED IN: PHI                                                 */
/*****************************************************************************/
/*****************************************************************************/

#define clid "ctrl_wemos0001"





#include <ESP8266HTTPClient.h> // Uses core ESP8266WiFi.h internally
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>



// *************************************** CONFIG "config_wifi_roy"

#include <config_wifi_roy.h>

#define EEPROM_ADDR_CONNECTED_SSID 1       // Start saving connected network SSID from this memory address
#define EEPROM_ADDR_CONNECTED_PASSWORD 30  // Start saving connected network Password from this memory address
#define AP_SSID clid                       // Set your own Network Name (SSID)
#define AP_PASSWORD "12345678"             // Set your own password

ESP8266WebServer server(80);
// WiFiServer wifiserver(80);

// ***************************************



// *************************************** CONFIG OTA LAN SERVER

ESP8266WebServer serverOTA(5000);
const String serverIndex = String("<div><p>Current sketch size: ") + ESP.getSketchSize() + " bytes</p><p>Max space available: " + ESP.getFreeSketchSpace() + " bytes</p></div><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

// ***************************************



// *************************************** CONFIG "no_poll_subscriber"

#include <NoPollSubscriber.h>

#define SUB_HOST "notify.estudiosustenta.myds.me"
#define SUB_PORT 80
// #define SUB_HOST "192.168.1.72"
// #define SUB_PORT 1010
#define SUB_PATH "/"

// Declaramos o instanciamos un cliente que se conectará al Host
WiFiClient sub_WiFiclient;

// ***************************************



// *************************************** CONFIG THIS SKETCH

#define PIN_LED_OUTPUT 5 // Pin de LED que va a ser controlado // before was LED_BUILTIN instead of 5
#define PIN_LED_CTRL 15 // Pin que cambia/controla el estado del LED en PIN_LED_OUTPUT manualmente (TOGGLE LED si cambia a HIGH), si se presiona mas de 2 degundos TOGGLE el modo AP y STA+AP, si se presiona 10 segundos se borra toda la memoria EEPROM y se resetea el dispositivo.

byte PIN_LED_CTRL_VALUE;

#define PUB_HOST "estudiosustenta.myds.me"
// #define PUB_HOST "192.168.1.72"

/***** SET PIN_LED_CTRL TIMER TO PRESS *****/
unsigned long lc_timestamp = millis();
unsigned int  lc_track = 0;
unsigned int  lc_times = 0;
unsigned int  lc_timeout = 10 * 1000; // 10 seconds

// ***************************************





///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// CONNECTION ///////////////////////////////////////////

HTTPClient http;
WiFiClient wifiClient;

/////////////////////////////////////////////////
/////////////////// HTTP GET ///////////////////

// httpGet("http://192.168.1.80:69/");
String httpGet(String url) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.GET();
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  
  if ( httpResponseCode > 0 ) {
    if ( httpResponseCode >= 200 && httpResponseCode < 300 ) {
      response = http.getString();
    } else {
      response = "";
    }

  } else {
    response = "";
    Serial.print("[HTTP] GET... failed, error: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }

  // Free resources
  http.end();

  return response;
}

/////////////////////////////////////////////////
/////////////////// HTTP POST ///////////////////

// httpPost("http://192.168.1.80:69/", "application/json", "{\"hola\":\"mundo\"}");
String httpPost(String url, String contentType, String data) {

  String response;

  http.begin(wifiClient, url.c_str());
  
  // Send HTTP GET request
  http.addHeader("Content-Type", contentType);
  http.addHeader("X-Auth-Bearer", clid);
  int httpResponseCode = http.POST(data);
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    response = http.getString();
  } else {
    Serial.print("HTTP Request error: ");
    Serial.println(httpResponseCode);
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    response = String("HTTP Response code: ") + httpResponseCode;
  }
  // Free resources
  http.end();

  return response;
}





///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// HELPERS /////////////////////////////////////////////

/////////////////////////////////////////////////
//////////////// KEEP-ALIVE LOOP ////////////////

int connId;
String connSecret;
long connTimeout;
bool runAliveLoop = false;

/***** SET TIMEOUT *****/
unsigned long al_timestamp = millis();
/*unsigned int  al_times = 0;*/
unsigned long  al_timeout = 0;
unsigned int  al_track = 0; // al_track = al_timeout; TO: execute function and then start counting

void handleAliveLoop() {

  if (!runAliveLoop) {
    return;
  }

  al_timeout = connTimeout - 3000; // connTimeout - 3 seconds

  if ( millis() != al_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
    al_track++;
    al_timestamp = millis();
  }

  if ( al_track > al_timeout ) {
    // DO TIMEOUT!
    /*al_times++;*/
    al_track = 0;

    // RUN THIS FUNCTION!
    String response = httpPost(String("http://") + SUB_HOST + ":" + SUB_PORT + "/alive", "application/json", String("{\"connid\":") + connId + ",\"secret\":\"" + connSecret + "\"}");

    ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant

    // Stream& response;

    StaticJsonDocument<96> doc;

    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      runAliveLoop = false;
      return;
    }

    int connid = doc["connid"]; // 757
    const char* secret = doc["secret"]; // "zbUIE"
    long timeout = doc["timeout"]; // 300000

    ///// End Deserialize JSON

    connId = connid;
    connSecret = (String)secret;
    connTimeout = timeout;
    runAliveLoop = true;


    updateControllerData();
  }

}

/////////////////////////////////////////////////
//////////////// DETECT CHANGES ////////////////

bool changed(byte gotStat, byte &compareVar) {
  if(gotStat != compareVar){
    /*
    Serial.print("CHANGE!. Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    compareVar = gotStat;
    return true;
  } else {
    /*
    Serial.print("DIDN'T CHANGE... Before: ");
    Serial.print(compareVar);
    Serial.print(" | After: ");
    Serial.println(gotStat);
    */
    //compareVar = gotStat;
    return false;
  }
}

/////////////////////////////////////////////////
//////////// UPDATE CONTROLLER DATA ////////////

void updateControllerData() {
  Serial.print("Updating controller data in server... ");
  Serial.println(httpPost(String("http://") + PUB_HOST + "/controll/controller.php", "application/json", String("{\"controller\":\"") + clid + "\",\"ipv4_interface\":\"" + WiFi.localIP().toString().c_str() + "\"}"));
}

/////////////////////////////////////////////////
/////////////// FUNCTIONS IN LOOP ///////////////

void doInLoop() {

  //Serial.println("loop");

  handleAliveLoop();
  
  wifiConfigLoop(server);

  serverOTA.handleClient();

  if ( digitalRead(PIN_LED_CTRL) == 1 ) { // IF PIN_LED_CTRL IS PRESSED
    
    // START TIMER
    if ( millis() != lc_timestamp ) { // "!=" intead of ">" tries to void possible bug when millis goes back to 0
      lc_track++;
      lc_timestamp = millis();
    }

    if ( lc_track > lc_timeout ) {
      // DO TIMEOUT!
      lc_times++;
      lc_track = 0;

      if (lc_times == 1) {
        // ERASE ALL EEPROM MEMORY AND RESTART
        EEPROM_CLEAR();
        ESP.reset();
      }
    }
  }
  
  bool ledCtrlChanged = changed(digitalRead(PIN_LED_CTRL), PIN_LED_CTRL_VALUE);

  if (ledCtrlChanged) {
    
    if (PIN_LED_CTRL_VALUE == 0) { // IF PIN_LED_CTRL WAS RELEASED

      unsigned int pushedTime = lc_track;
      
      lc_times = 0; // RESET COUNTER
      lc_track = 0; // RESET TIMER

      if (pushedTime < 2000) { // IF PIN_LED_CTRL WAS PRESSED LESS THAN 2 SECONDS...
        
        // TOGGLE LED STATUS
        digitalWrite(PIN_LED_OUTPUT, !digitalRead(PIN_LED_OUTPUT));
      
        Serial.print("LED status toggled via pin. Now LED is: ");
        Serial.println(digitalRead(PIN_LED_OUTPUT));
      
        if (WiFi.status() == WL_CONNECTED) {

          String led_state_string = "error";

          if( digitalRead(PIN_LED_OUTPUT) == 1 ) {
            led_state_string = "true";
          } else if( digitalRead(PIN_LED_OUTPUT) == 0 ) {
            led_state_string = "false";
          }
        
          Serial.println("Notifying LED was changed manually...");
          httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=" + led_state_string + "&info=pin_change_on_device&clid=" + clid);
          Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=led_wemos0001&shout=true&log=pin_change_on_device&state_changed=true", "application/json", "{\"type\":\"change\", \"data\":" + led_state_string + ", \"whisper\":0}"));
        }
      }

      if ( (pushedTime >= 2000) && (pushedTime < lc_timeout) ) { // IF PIN_LED_CTRL WAS PRESSED FOR 2 SECONDS BUT LESS THAN THE TIMEOUT...
        // TOGGLE SERVER STATUS
        ESP_AP_TOGGLE(true);
      }
    }
  }
  

}

/////////////////////////////////////////////////
//////////////// GET LAST STATUS ////////////////

bool requestLast() {

  Serial.println("Getting last value from database...");

  String lastValue = httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/getLast.php");
  Serial.print("Last value: ");
  Serial.print(lastValue);



  ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant

  // Stream& lastValue;
  StaticJsonDocument<16> filter;
  filter["value"] = true;

  StaticJsonDocument<48> doc;

  DeserializationError error = deserializeJson(doc, lastValue, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  const char* value = doc["value"]; // "ON" - "OFF"

  ///// End Deserialize JSON



  if ( strcmp(value, "ON") == 0 ) {
    Serial.println(" (on)");
    digitalWrite(PIN_LED_OUTPUT, HIGH);   // Turn the LED on
    Serial.println("Notifying we got the last value correctly...");
    httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=true&info=device_checked_last_status&clid=" + clid);
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=led_wemos0001&shout=true&log=device_just_connected_to_broker_and_checked_last_status___is_on", "application/json", "{\"type\":\"change\", \"data\":1, \"whisper\":0}"));
    
    return true;
  } else if ( strcmp(value, "OFF") == 0 ) {
    Serial.println(" (off)");
    digitalWrite(PIN_LED_OUTPUT, LOW);   // Turn the LED off
    Serial.println("Notifying we got the last value correctly...");
    httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=false&info=device_checked_last_status&clid=" + clid);
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=led_wemos0001&shout=true&log=device_just_connected_to_broker_and_checked_last_status___is_off", "application/json", "{\"type\":\"change\", \"data\":0, \"whisper\":0}"));
    return true;
  } else {
    Serial.println();
    Serial.print("CAN NOT CONNECT TO HOST: ");
    Serial.println(PUB_HOST);
    return false;
  }

}

/////////////////////////////////////////////////
//////////////// ON-PARSED LOGIC ////////////////

void onParsed(String line) {

  Serial.print("Got JSON: ");
  Serial.println(line);



  ///// Deserialize JSON. from: https://arduinojson.org/v6/assistant
  
  // Stream& line;

  StaticJsonDocument<0> filter;
  filter.set(true);

  StaticJsonDocument<384> doc;

  DeserializationError error = deserializeJson(doc, line, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  long long iat = doc["iat"]; // 1659077886875

  const char* ep_requested = doc["ep"]["requested"]; // "@SERVER@"
  const char* ep_emitted = doc["ep"]["emitted"]; // "@SERVER@"

  const char* e_type = doc["e"]["type"]; // "info"

  JsonObject e_detail = doc["e"]["detail"];
  int e_detail_connid = e_detail["connid"]; // 757
  const char* e_detail_secret = e_detail["secret"]; // "zbUIE"
  long e_detail_timeout = e_detail["timeout"]; // 300000
  const char* e_detail_device = e_detail["device"]; // "led_wemos0001"
  int e_detail_whisper = e_detail["whisper"]; // 6058
  const char* e_detail_data = e_detail["data"]; // "OFF"

  ///// End Deserialize JSON



  if (strcmp(e_type, "led_write") == 0 && strcmp(e_detail_data, "ON") == 0) {
    Serial.println("Servidor pide cambiar LED a estado: on");
    digitalWrite(PIN_LED_OUTPUT, HIGH);   // Turn the LED off
    Serial.println("Notifying LED status changed successfully...");
    httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=true&info=changed_by_request&clid=" + clid);
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&shout=true&log=changed_by_request___to_on&state_changed=true", "application/json", String("{\"type\":\"change\", \"data\":1, \"whisper\":") + String(e_detail_whisper) + "}"));
  }

  if (strcmp(e_type, "led_write") == 0 && strcmp(e_detail_data, "OFF") == 0) {
    Serial.println("Servidor pide cambiar LED a estado: off");
    digitalWrite(PIN_LED_OUTPUT, LOW);   // Turn the LED on
    Serial.println("Notifying LED status changed successfully...");
    httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=false&info=changed_by_request&clid=" + clid);
    Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&shout=true&log=changed_by_request___to_off&state_changed=true", "application/json", String("{\"type\":\"change\", \"data\":0, \"whisper\":") + String(e_detail_whisper) + "}"));
  }

  if (strcmp(e_type, "led_read") == 0) {
    Serial.print("Servidor solicita el estado actual del LED. Estado actual: ");
    Serial.println(digitalRead(PIN_LED_OUTPUT));
    if (digitalRead(PIN_LED_OUTPUT) == 1) {
      httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=true&info=current_status_requested&clid=" + clid);
      Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&log=current_status_requested___is_on", "application/json", String("{\"type\":\"change\", \"data\":1, \"whisper\":") + String(e_detail_whisper) + "}"));
    }
    if (digitalRead(PIN_LED_OUTPUT) == 0) {
      httpGet(String("http://") + PUB_HOST + "/test/WeMosServer/controll/response.php?set=false&info=current_status_requested&clid=" + clid);
      Serial.print(httpPost(String("http://") + PUB_HOST + "/controll/res.php?device=" + e_detail_device + "&log=current_status_requested___is_off", "application/json", String("{\"type\":\"change\", \"data\":0, \"whisper\":") + String(e_detail_whisper) + "}"));
    }
  }



  if (strcmp(ep_emitted, "@SERVER@") == 0) {
    connId = e_detail_connid;
    connSecret = (String)e_detail_secret;
    connTimeout = e_detail_timeout;
    runAliveLoop = true;
  }

/* PARA COMPROBAR QUE PASA SI SE REINICIA EL MODULO Y EN EL INTER CAMBIA EL ESTADO (EN WEB)
Serial.println("restarting...");
ESP.restart(); // tells the SDK to reboot, not as abrupt as ESP.reset()
*/

}

/////////////////////////////////////////////////
////////////// ON-CONNECTED LOGIC //////////////

void onConnected() {
  requestLast();
  updateControllerData();
}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// SETUP //////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  EEPROM.begin(4096);
  Serial.print("...STARTING...");
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT); // COMMENT THIS LINE IF YOUR PIN_LED_OUTPUT = LED_BUILTIN
  pinMode(PIN_LED_OUTPUT, OUTPUT); // Initialize as an output // To controll LED in this pin
  pinMode(PIN_LED_CTRL, INPUT);    // Initialize as an input // To toggle LED status manually and TOGGLE AP/STA+AP MODE (long press)

  setupWifiConfigServer(server, EEPROM_ADDR_CONNECTED_SSID, EEPROM_ADDR_CONNECTED_PASSWORD, AP_SSID, AP_PASSWORD);

  /*** START SERVER ANYWAY XD ***/
  //Serial.println("Starting server anyway xD ...");
  //ESP_AP_STA();
  /******************************/

  setLedModeInverted(true);





  serverOTA.on("/", HTTP_GET, []() {

    // /*
    Serial.print("ESP.getBootMode(): ");
    Serial.println(ESP.getBootMode());
    Serial.print("ESP.getSdkVersion(): ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("ESP.getBootVersion(): ");
    Serial.println(ESP.getBootVersion());
    Serial.print("ESP.getChipId(): ");
    Serial.println(ESP.getChipId());
    Serial.print("ESP.getFlashChipSize(): ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("ESP.getFlashChipRealSize(): ");
    Serial.println(ESP.getFlashChipRealSize());
    Serial.print("ESP.getFlashChipSizeByChipId(): ");
    Serial.println(ESP.getFlashChipSizeByChipId());
    Serial.print("ESP.getFlashChipId(): ");
    Serial.println(ESP.getFlashChipId());
    Serial.print("ESP.getFreeHeap(): ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("ESP.getSketchSize(): ");
    Serial.println(ESP.getSketchSize());
    Serial.print("ESP.getFreeSketchSpace(): ");
    Serial.println(ESP.getFreeSketchSpace());
    Serial.print("ESP.getSketchMD5(): ");
    Serial.println(ESP.getSketchMD5());
    // */

    serverOTA.sendHeader("Connection", "close");
    serverOTA.send(200, "text/html", serverIndex);
  });
  serverOTA.on("/update", HTTP_POST, []() {
    serverOTA.sendHeader("Connection", "close");
    serverOTA.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = serverOTA.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: Sketch too big?");
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: Error writing sketch");
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success!\nNew sketch using: %u\n bytes.\nRebooting...\n", upload.totalSize);
        serverOTA.send(200, "text/plain", "OK");
      } else {
        Update.printError(Serial);
        serverOTA.send(200, "text/plain", "ERROR: No update.end() ?");
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  serverOTA.begin();

}





///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// LOOP //////////////////////////////////////////////

void loop() {
  // put your main code here, to run repeatedly:

  handleNoPollSubscription(sub_WiFiclient, SUB_HOST, SUB_PORT, SUB_PATH, "POST", String("{\"clid\":\"") + clid + "\",\"ep\":[\"wid-0001/request\",\"controll/led/ctrl_wemos0001/req\"]}", "wid0001/2022", doInLoop, onConnected, onParsed);
  
}






