# Sonoff_MQTT

Educational simple Sonoff power switching via WiFi and MQTT 

## Introduction

This is a **very basic** MQTT interface for two members of the Sonoff Wifi-controlled 
power switch family, either the block with screw terminals one, or the more
fancy S20 wall outlet. This software shows you how to control these Sonoff's via
WiFi and MQTT, by flashing this Arduino software into it.
You can use it as is, or as a basis for more fance ones.

I actually created it when my discrete timer broke down, which I used for the 
power switching of the Christmass tree decoration, and I wanted a more fancy timer.
The standard software on the Sonoff wants to connect to some server in the cloud,
however I wanted to have control by myself, and connect it to my home control system.

It is now part of the home control, where the
timer itself runs on a Raspberry, and I can just send timing strings to it like:
`7:00-9:00;17:30-23:00` to switch the lights on in the morning and evening.
A small python script on the Raspberry translates this to an up-to-date on/off (1/0)
status in the MQTT database as (for example) topic `MyMQTT/Timer1`,
to which the Sonoff switch can subscribe.

You will have to modify your Sonoff device to have a programming header.
However, not that difficult as the PCB has already the holes to solder it.
You also will need to have a USB-based programmer with 3.3 V supply voltage.

Some more background information, although in Dutch, with pictures
on [my Arduino web page](https://www.keesmoerman.nl/arduino.html#mozTocId292264).

## Getting Started

This is a basic Arduino `sonoff_mqtt.ino` file.

### Prerequisites

You will need to have the Arduino development system installed, and to have some
experience with it. Also, you need to have a running MQTT server on your
local network, and preferably some experience with it.

You will need to have the [esp8266](http://arduino.esp8266.com/stable/package_esp8266com_index.json)
board support installed in your Arduino environment.
Also, you need to have the [PubSubClient](https://imroy.github.io/pubsubclient/classPubSubClient.html)
and [Ticker](https://arduino-esp8266.readthedocs.io/en/latest/libraries.html) libraries.

As said, you need a running MQTT server on your WiFi home network (for example
[mosquitto](http://mosquitto.org/man/mosquitto-8.html)
on a Raspberry Pi), and a way to publish messages via MQTT (for example
[MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash) ).

## Deployment

As this is a very basic example, it still has the WiFi access point and password
details hard-coded in the `.ino` source file, as well as the MQTT server details
(as Ethernet address and port number). As I have my MQTT protected with individual
device names and passwords, also these have to be updated.
Edit these to reflect your own situation.

Part of source code to adapt:

```
#define SONOFF "Sonoff5"                        // name of your device
#define MYTOPICS "MyMQTT/"                      // MQTT path

const char* ssid            = "<yourSSID>";     // Network WiFi SSID
const char* password        = "<yourPassword>"; // Network WiFi password

const char* mqtt_server     = "192.168.xxx.xx"; // MQTT server Ethernet address
const int   mqtt_port       = 1883;             // MQTT port number, 1883 is default
const char* mqtt_user       = SONOFF;           // the MQTT device name
const char* mqtt_password   = "MQTTpassword";   // the MQTT device password
```

The Sonoff with this firmware can be configured via MQTT to subscribe (listen) to
a specific MQTT topic. This way, you can have multiple Sonoffs listening to the
same topic, switching simultaneously.

For example, and with the above settings: the Sonoff is default listening to topic
`MyMQTT/Sonoff5` for a topic to get its control from. Publish `MyMQTT/Timer1`
to `MyMQTT/Sonoff5` to have the Sonoff subscribe and listen to Timer1,
where it will expect the string '0' or '1' for respectively off and on.

## Authors

* **Kees Moerman** - *Initial work* - [My pages](https://www.keesmoerman.nl/arduino.html)


## License

GNU General Public License v3.0

Permissions of this strong copyleft license are conditioned on making available
complete source code of licensed works and modifications, which include larger
works using a licensed work, under the same license. Copyright and license
notices must be preserved. Contributors provide an express grant of patent rights.
