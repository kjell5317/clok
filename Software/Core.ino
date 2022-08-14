// Libraries
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

// ----------------------------------------------------------------------------

// ID
#define ID 1

// WiFi
const char *ssid = "IoT";
const char *password = "aHFpONs5iOVT84sEqSyx";

// MQTT
const char *mqtt_server = "10.0.20.20";
const int mqtt_port = 1883;
const char *mqtt_user = "mqtt";
const char *mqtt_pw = "5317wasch";

// ----------------------------------------------------------------------------

// PINOUT
#define STRIP1 14 // D5
#define NUM1 24

int button1 = 13; // D7
int button2 = 12; // D6

// NeoPixel
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUM1, STRIP1, NEO_GRBW + NEO_KHZ800);

// Network
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClient espClient;
PubSubClient client(espClient);

// Variables
// Timer
boolean pomodoro = false;
unsigned long startTime = 0;
unsigned int timerWork = 25;
unsigned int timerBreak = 5;
unsigned int sinceBoot = 0;
unsigned int inRow = 0;
// Clock
unsigned long lastTime = 0;
unsigned int minute;
unsigned int minuteLED;
unsigned int hour;
unsigned int hourLED;
// Light
unsigned int brightness = 125;
boolean light = true;
unsigned int num = 0;
boolean automatic = true;
// MQTT
String id = String("cloc-") + String(ID);
String topic;
char msg[8];
// Button
unsigned int lastState1 = 0;
unsigned int lastState2 = 0;

// Setup
void setup()
{
    Serial.begin(115200);
    pinMode(button1, INPUT);
    pinMode(button2, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    strip1.begin();
    strip1.clear(); // Initialize all pixels to 'off'
    strip1.show();

    setup_wifi();
    timeClient.begin();
    timeClient.setTimeOffset(7200);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    getTime();
}

void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (num >= strip1.numPixels())
        {
            num = 0;
        }
        strip1.clear();
        strip1.setPixelColor(num, strip1.Color(brightness, 0, 0, 0));
        strip1.show();
        num++;
    }
    Serial.println();
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect()
{
    Serial.print("Connecting to ");
    Serial.println(mqtt_server);
    while (!client.connected())
    {
        if (client.connect(id.c_str(), mqtt_user, mqtt_pw))
        {
            Serial.print("connected as ");
            Serial.println(id);
            topic = id + "/light";
            client.subscribe(topic.c_str());
            topic = id + "/timer";
            client.subscribe(topic.c_str());
            topic = id + "/light/brightness";
            client.subscribe(topic.c_str());
            topic = id + "/timer/duration/work";
            client.subscribe(topic.c_str());
            topic = id + "/timer/duration/break";
            client.subscribe(topic.c_str());

            topic = id + "/light/status";
            client.publish(topic.c_str(), "ON");
            topic = id + "/timer/status";
            client.publish(topic.c_str(), "OFF");
            sprintf(msg, "%d", sinceBoot);
            topic = id + "/timer/counter/sinceBoot";
            client.publish(topic.c_str(), msg);
            topic = id + "/timer/counter/inRow";
            sprintf(msg, "%d", inRow);
            client.publish(topic.c_str(), msg);
            topic = id + "/timer/duration/work/status";
            sprintf(msg, "%d", timerWork);
            client.publish(topic.c_str(), msg);
            topic = id + "/timer/duration/break/status";
            sprintf(msg, "%d", timerBreak);
            client.publish(topic.c_str(), msg);
            topic = id + "/light/brightness/status";
            client.publish(topic.c_str(), "A");
        }
        else
        {
            Serial.print(".");
            if (num >= strip1.numPixels())
            {
                num = 0;
            }
            strip1.clear();
            strip1.setPixelColor(num, strip1.Color(brightness, 0, 0, 0));
            strip1.show();
            num++;
            delay(500);
        }
    }
}

// Callback
void callback(char *inTopic, byte *payload, unsigned int length)
{
    String newTopic = inTopic;
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    int intPayload = newPayload.toInt();
    Serial.print("Message arrived: ");
    Serial.print(newTopic);
    Serial.print(" ");
    Serial.println(newPayload);
    if (newTopic == id + "/light")
    {
        if (newPayload == "ON")
        {
            startLight();
        }
        else if (newPayload == "OFF")
        {
            stopLight();
        }
    }
    else if (newTopic == id + "/timer")
    {
        if (newPayload == "ON")
        {
            startTimer();
        }
        else if (newPayload == "OFF")
        {
            stopTimer();
        }
    }
    else if (newTopic == id + "/light/brightness")
    {
        if (newPayload == "A")
        {
            automatic = true;
            topic = id + "/light/brightness/status";
            client.publish(topic.c_str(), "A");
        }
        else
        {
            automatic = false;
            brightness = map(constrain(intPayload, 8, 255), 0, 100, 8, 255);
            sprintf(msg, "%d", intPayload);
            topic = id + "/light/brightness/status";
            client.publish(topic.c_str(), msg);
        }
    }
    else if (newTopic == id + "/timer/duration/work")
    {
        if (intPayload > 0)
        {
            timerWork = intPayload;
            topic = id + "/timer/duration/work/status";
            sprintf(msg, "%d", timerWork);
            client.publish(topic.c_str(), msg);
        }
    }
    else if (newTopic == id + "/timer/duration/break")
    {
        if (intPayload > 0)
        {
            timerBreak = intPayload;
            topic = id + "/timer/duration/break/status";
            sprintf(msg, "%d", timerBreak);
            client.publish(topic.c_str(), msg);
        }
    }
}

// Light
void startLight()
{
    light = true;
    Serial.println("LIGHT: ON");
    topic = id + "/light/status";
    client.publish(topic.c_str(), "ON");
}
void stopLight()
{
    strip1.clear();
    strip1.show();
    light = false;
    Serial.println("LIGHT: OFF");
    topic = id + "/light/status";
    client.publish(topic.c_str(), "OFF");
}

// Clock
void getTime()
{
    if (millis() - lastTime > 30000 || lastTime == 0)
    {
        timeClient.update();
        lastTime = millis();

        hour = timeClient.getHours();
        Serial.print("Time: ");
        Serial.print(hour);
        if (hour >= 12)
        {
            hour -= 12;
        }
        minute = timeClient.getMinutes();
        Serial.print(":");
        Serial.print(minute);
        Serial.println();

        hourLED = map(hour, 0, 11, 0, strip1.numPixels() - 1);
        minuteLED = map(minute, 0, 59, 0, strip1.numPixels() - 1);
    }
    updateClock();
}

void updateClock()
{
    if (light)
    {
        strip1.clear();
        if (hourLED != minuteLED)
        {
            strip1.setPixelColor(minuteLED, strip1.Color(brightness, 0, 0, 0));
        }
        strip1.setPixelColor(hourLED, strip1.Color(0, 0, brightness, 0));
        strip1.show();
    }
}

// Timer
void checkButton()
{
    int state1 = digitalRead(button1);
    int state2 = digitalRead(button2);
    if (state1 != lastState1)
    {
        lastState1 = state1;
        if (state1 == HIGH)
        {
            if (pomodoro)
            {
              stopTimer();
            }
            else
            {
              startLight();
              startTimer();
            }
        }
    }
    if (state2 != lastState2)
    {
      lastState2 = state2;
      if (state2 == HIGH)
      {
        if (light)
        {
          stopLight();
        }
        else
        {
          startLight();
        }
      }
    }
}

void startTimer()
{
    pomodoro = true;
    startTime = millis();
    Serial.println("TIMER: ON");
    topic = id + "/timer/status";
    client.publish(topic.c_str(), "ON");
    if (sinceBoot > 0)
    {
        topic = id + "/timer/counter/sinceBoot";
        sprintf(msg, "%d", sinceBoot);
        client.publish(topic.c_str(), msg);
    }
    if (inRow == 0)
    {
        topic = id + "/timer/counter/inRow";
        sprintf(msg, "%d", inRow);
        client.publish(topic.c_str(), msg);
    }
    else if (inRow > 0)
    {
        topic = id + "/timer/counter/inRow";
        sprintf(msg, "%d", inRow);
        client.publish(topic.c_str(), msg);
    }
}

void stopTimer()
{
    pomodoro = false;
    inRow = 0;
    Serial.println("TIMER: OFF");
    topic = id + "/timer/status";
    client.publish(topic.c_str(), "OFF");
}

void updateTimer()
{
    if (millis() - startTime > 1000 * 60 * timerWork)
    {
        if (millis() - startTime > 1000 * 60 * (timerWork + timerBreak))
        {
            sinceBoot++;
            inRow++;
            return startTimer();
        }
        if (light)
        {
            // Pause
            int past = map(1000 * 60 * timerBreak - (millis() - (startTime + 1000 * 60 * timerWork)), 0, 1000 * 60 * timerWork, strip1.numPixels() - 1, -1);
            strip1.clear();
            if (past >= 0)
            {
                for (int i = 0; i <= past; i++)
                {
                    strip1.setPixelColor(strip1.numPixels() - 1 - i, strip1.Color(0, brightness, 0, 0)); // Green
                }
            }
            strip1.show();
        }
    }
    else
    {
        if (light)
        {
            // Arbeiten
            int past = map(1000 * 60 * timerWork - (millis() - startTime), 0, 1000 * 60 * timerWork, -1, strip1.numPixels() - 1);
            strip1.clear();
            if (past >= 0)
            {
                for (int i = 0; i <= past; i++)
                {
                    strip1.setPixelColor(i, strip1.Color(0, 0, 0, brightness)); // White
                }
            }
            strip1.show();
        }
    }
}


// Loop
void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();
    checkButton();
    if (pomodoro)
    {
        updateTimer();
    }
    else
    {
        getTime();
    }
}
