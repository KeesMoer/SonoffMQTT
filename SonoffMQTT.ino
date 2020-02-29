// Simple MQTT test for Sonoff module, Kees Moerman January 2018
// Based on example code from www.baldengineer.com/mqtt-tutorial.html
// Make sure to include Sketch -> Include Library -> Manage Libraries -> PubSubClient
//
// Version 0.1: derived from Wemos_mqtt 0.3
// Version 0.2: added programmable subscription via topic Kees/<name>/Set
// Version 0.3: Some cleanup; but seems to crash after some time.
// Version 0.4: added queue for callback processing (copied from SerialWifiEnergy)
//              Still does a restart after first start... Why? Add some delays?
//                2018-12-23 12:28:43.539686: Kees/Log Sonoff2 0.4 started...
//                2018-12-23 12:28:43.572419: Kees/Log Sonoff2 died...
//                2018-12-23 12:28:43.826181: Kees/Log Sonoff2 new subscription...
// Version 0.5: Cleanup
//
// ToDo:
// - Fixed IP addresses (230 range? or <100 range?)
// - Commissioning via serial and/or mqtt generic name -> name, user, passw, subscr,
//   currently everything is hard coded
// - Use File system for commissioning data
// 
// Programming:
// Via een USB-to-serial kabeltje en aanpassing van 5 Volt naar de benodigde 3.3 Volt
// aansluiten op je computer. De Arduino-omgeving kan vervolgens de Flash op de Sonoff schrijven:
// knop ingedrukt houden tijdens power-on om in boot-mode te komen.
// Setting: 'Generic ESP8285 module', Flash 1 MB/64K SPIFFS, 26 MHz Xtal/80 MHz cpu, USBasp prog
// https://www.keesmoerman.nl/arduino.html#mozTocId292264
//
// pubsubclient API reference: 
// https://imroy.github.io/pubsubclient/classPubSubClient.html
//
// Current approach does not allow easy publishing of data, need to break open the loop
// https://github.com/knolleary/pubsubclient/issues/197
//
// Update in PubSub library, seems mandatory???
// In ~/Documents/Projects/Arduino/libraries/PubSubClient/src:
// #define MQTT_MAX_PACKET_SIZE 256
//
// Note: it does also listen to its own MQTT messages... Bidirectional link!!
// Need to update dashboard, but can also be updated by dashboard. OK
//
// Timer1 is used by WiFi, so standard timer libraries will not work. Use Ticker:
// https://arduino-esp8266.readthedocs.io/en/latest/libraries.html
//
// File system tool (not used yet)
// https://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
// https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.1.3/ESP8266FS-0.1.3.zip.
//
// Misc:
// Schematics https://www.itead.cc/wiki/File:Sonoff_TH_Schematic.pdf

#define SONOFF "Sonoff5"                // name of your device
#define MYTOPICS "MyMQTT/"              // MQTT path

#include <EEPROM.h>                     // Why needed?
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>

const char* ssid            = "<yourSSID>";     // Network WiFi SSID
const char* password        = "<yourPassword>"; // Network WiFi password

const char* mqtt_server     = "192.168.xxx.xx"; // MQTT server Ethernet address
const int   mqtt_port       = 1883;             // MQTT port number, 1883 is default
const char* mqtt_user       = SONOFF;           // the MQTT device name
const char* mqtt_password   = "MQTTpassword";   // the MQTT device password

const char* mqtt_topicSet   = MYTOPICS SONOFF "/Set";
const char* mqtt_topicDbg   = MYTOPICS "Debug";     // debugging messages
const char* mqtt_topicLog   = MYTOPICS "Log";       // Logging messages

void mqttCallback(char* topic, byte* payload, unsigned int length);
 
WiFiClient espClient;
PubSubClient mqttClient(mqtt_server, mqtt_port, mqttCallback, espClient);

const int relaisPin    = 12;            // Sonoff Relais: GPIO 12; active-HIGH
const int switchPin    =  0;            // Sonoff Physical key: GPIO0 (D3); active-low
const int ledPin       = 13;            // Sonoff green LED: GPIO13 (D3); active-low
const int extraPin     = 14;            // this is the extra pin on the 5-pin header

int relaisState  =  0;
int ledState     =  0;
int oldKeyState  =  0;

long lastMsgTimestamp = 0;

#define MQTTSUBSCRLEN 128               // Dynamic subscription acc to setting
char SonoffMqqtSubscr[MQTTSUBSCRLEN] = "\0";   // So far empty


Ticker tick50ms;                        // Ticker object
unsigned int tickCounter  = 0;          // Current counter value
unsigned int tickPrevious = 0;          // Value to where we catched up already



// Just some help for compact debug print statements
#define Serial_println2(x1,x2)         do { Serial.print((x1)); Serial.println((x2)); } while (0)
#define Serial_println3(x1,x2,x3)      do { Serial.print((x1)); Serial.print((x2)); Serial.println((x3)); } while (0)
#define Serial_println4(x1,x2,x3,x4)   do { Serial.print((x1)); Serial.print((x2)); Serial.print((x3)); Serial.println((x4)); } while (0)
#define Serial_println5(x1,x2,x3,x4,x5)       do { Serial.print((x1)); Serial.print((x2)); Serial.print((x3)); Serial.print((x4)); Serial.println((x5)); } while (0)
#define Serial_println6(x1,x2,x3,x4,x5,x6)    do { Serial.print((x1)); Serial.print((x2)); Serial.print((x3)); Serial.print((x4)); Serial.print((x5)); Serial.println((x6)); } while (0)


void tick()                             // 'interrupt' scheduled by OS at 50 ms
{
    tickCounter++;                      // just increment ticker; expensive actions in main loop
}

void setup_ticks(void)
{
    tick50ms.attach_ms(50, tick);       // 50 ms == 20 Hz, should be good enough
}

void setRelais(bool relaisOn)           // Control interface for 220V relais
{
    relaisState = relaisOn?1:0;
    digitalWrite(relaisPin, relaisOn?HIGH:LOW); // Active high
}

bool mqttResultCheckPrint(bool succeeded, const char* action, const char* message)
{                                       // print informative error message
    Serial_println4(succeeded?"Success ":"Failed", action, " with ", message);
    return succeeded;                   // pass down for further error handling
}

void setup_wifi() {                     // initialise the WiFi connection
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial_println2("Connecting to ",ssid);
  
    WiFi.mode(WIFI_STA);                // Station, not access point!      
    WiFi.begin(ssid, password);
    WiFi.mode(WIFI_STA);                // before or after? Doc shows before, internet after ...
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      ledState = 1-ledState;            // Flash LED, low = on
      digitalWrite(ledPin, ledState?HIGH:LOW);
    }
  
    Serial.println();
    Serial_println2("WiFi connected, IP=", WiFi.localIP());
    ledState = 0;                       // Enable LED, low = on 
    digitalWrite(ledPin, ledState?HIGH:LOW);
}


// ------ Don't want to do too much in the callback itself, so move to queue(s)

#define QUEUESIZE 4
#define QUEUEMESSLEN 255

struct Mqtt_message_struct
{
    char topic[QUEUEMESSLEN+1];         // MQTT topic path/name
    char payload[QUEUEMESSLEN+1];       // MQTT payload
    unsigned int payloadlen;            // and the length of the payload
} mqttQueue[QUEUESIZE];

unsigned int  mqtt_readptr = 0;         // pointers equal -> queue empty
unsigned int  mqtt_writeptr = 0;


void mqttCallback(char* topic, byte* payload, unsigned int length)
{                                       // lightweigth IRQ: only store message, don't execute
    unsigned int payloadLength = min(length, (unsigned int)QUEUEMESSLEN);
    
    payload[payloadLength] = '\0';
    Serial_println6("Message received [", topic, "]= (", payloadLength, "): ", (char *)payload);
    
    mqtt_writeptr++;                    // next empty entry in queue
    if(mqtt_writeptr >= QUEUESIZE) { mqtt_writeptr = 0; }
    
    strncpy(mqttQueue[mqtt_writeptr].topic, topic, QUEUEMESSLEN);
    strncpy(mqttQueue[mqtt_writeptr].payload, (char *)payload, payloadLength);
    mqttQueue[mqtt_writeptr].payloadlen = payloadLength;
    mqttQueue[mqtt_writeptr].payload[payloadLength] = (char)0; // terminator
}

void process_queue(void)                // handle one queued MQTT message
{                                       // (to be called in main loop)
    if(mqtt_readptr != mqtt_writeptr)
    {
      mqtt_readptr++;                   // OK, we have something in queue
      if(mqtt_readptr >= QUEUESIZE) { mqtt_readptr = 0; }

      if(strcmp(mqttQueue[mqtt_readptr].topic, mqtt_topicSet) == 0)
      {
        if(strlen(SonoffMqqtSubscr))
        {
          mqttResultCheckPrint(mqttClient.unsubscribe(SonoffMqqtSubscr),
            "unsubscribe", SonoffMqqtSubscr);       // unsubscribe previous
        }
        SonoffMqqtSubscr[0] = 0;

        uint length = strlen(mqttQueue[mqtt_readptr].payload);
        if(length && (length < MQTTSUBSCRLEN))      // Could be empty: only unsubscribe
        {
          strncpy(SonoffMqqtSubscr, mqttQueue[mqtt_readptr].payload, MQTTSUBSCRLEN);
          SonoffMqqtSubscr[length] = 0;             // new subscription; remember
          mqttResultCheckPrint(mqttClient.subscribe(SonoffMqqtSubscr), // and subscribe
            "subscribe", SonoffMqqtSubscr);
        }
        mqttResultCheckPrint(mqttClient.publish(mqtt_topicLog, (char *)(SONOFF " new subscription...")),
          "publish", (char *)mqtt_topicLog);
      }
      else if(strcmp(mqttQueue[mqtt_readptr].topic, SonoffMqqtSubscr) == 0)
      {
        // No need for publishStatus, as we got here already due to a publication
        setRelais(mqttQueue[mqtt_readptr].payload[0] == '1');
        Serial_println2("Change state received: ", mqttQueue[mqtt_readptr].payload[0]);
      }
      else // For debug purposes: display unknown topic (where does subscription come from?)
      {
        Serial_println2("What? ", mqttQueue[mqtt_readptr].topic); 
      }
    }
}

// -------------------------------------------------


void publishStatus()                    // Publish our current State
{
    if (mqttClient.connected())
    {
      Serial.print("MQTT connected, ");
      mqttResultCheckPrint(mqttClient.publish(SonoffMqqtSubscr, relaisState?"1":"0"),
        "publishStatus", relaisState?"On":"Off");
    }
    else
    {
      Serial.println("MQTT connect failed.");
    }
}
 
void reconnect_subscribe()
{
    while (!mqttClient.connected())     // Loop until we're reconnected
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect to MQTT server:
      //   boolean connect(const char* id, const char* user, const char* pass,
      //       const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
      if (mqttClient.connect(SONOFF " Client", mqtt_user, mqtt_password, 
              mqtt_topicLog, 0, false, SONOFF " died..."))
      {
        delay(100);                     // Allow some time to initialise/stabilise
        Serial.println("connected");
        mqttResultCheckPrint(mqttClient.subscribe(mqtt_topicSet),  // Control interface setup
          "reconnect subscribe", (char *)mqtt_topicSet);
        delay(100);                     // Allow some time to switch to serial terminal
      }
      else
      {                                 // Keep on trying to initialise/stabilise
        Serial_println3("failed, rc=", mqttClient.state(), " try again in 5 seconds");
        delay(5000);
      }
    }
}

void setup() {
    delay(100);                         // Allow some time to switch to serial terminal
    Serial.begin(115200);
    delay(100);                         // Allow some time to switch to serial terminal
    Serial_println2("Setting up " SONOFF ", LED is pin ", relaisPin);
   
    pinMode(relaisPin, OUTPUT);
    setRelais(false);
    
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);          // low is on
    ledState = 1;
    
    pinMode(switchPin, INPUT_PULLUP);   // the push-button
    
    setup_wifi();
    setup_ticks();                      // the 50ms ticker
}
 
void loop()
{
    static bool firstTime = true;
    int keyState = digitalRead(switchPin);
     
    if (!mqttClient.connected()) {
      reconnect_subscribe();
    }
    mqttClient.loop();                  // does not loop; just executes one handling
  
  // Handling of (missed) ticks and input pins
    while(tickPrevious < tickCounter)
    {
      tickPrevious++;
      if(keyState != oldKeyState)
      {
        oldKeyState = keyState;
        if(keyState)                    // key released again?
        {
          setRelais(!relaisState);      // toggle LED
          publishStatus();              // and, as we decided, also publish updated status
        }
      }
      process_queue();
    }
  
    long now = millis();
    if (now - lastMsgTimestamp > 10000) // every 10 seconds
    {
      lastMsgTimestamp = now;
      Serial_println6("Tick count = ", tickCounter, ", State = ", relaisState, ", Switch = ", keyState);
  //  Serial.println("Publish message...");
  //  publishStatus();                  // Publish only when changed
    }

    if(firstTime && (tickCounter > 100))
    {
      firstTime = false;                // To get message in log file
      mqttResultCheckPrint(mqttClient.publish(mqtt_topicLog, (char *)(SONOFF " 0.4 started...")),
        "publish", (char *)mqtt_topicLog);
    }
}

// End of file
