#include "main.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <BlynkSimpleEsp8266.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <NTPClient.h>
#include <ArduinoJson.h>


char auth[] = "AEVP1i4PpaT6vapjw4Gz0mNRg5kawk-X";

const uint8_t       relay_switch_pin{4};

constexpr uint64_t  update_time_interval{1000*60};
uint64_t            last_update{0};

NTPClient*          timeClient = nullptr;
WiFiUDP*            ntpUDP = nullptr;

TriggerSettings     ts;
bool                sensor_active{true};


//-----------------------------------------------------------------------------------------------------------
void setup() 
{
    // put your setup code here, to run once:
    Serial.begin(115200);

    pinMode(relay_switch_pin, OUTPUT);

    load_trigger_settings();

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset saved settings
    //wifiManager.resetSettings();
    
    //set custom ip for portal
    //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect("Dooooors");
    
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    ntpUDP = new WiFiUDP();
    timeClient = new NTPClient(*ntpUDP);
    timeClient->begin();
    timeClient->setTimeOffset(7*60*60);
    //timeClient->setUpdateInterval(1000*60);

    

    Blynk.config(auth);
}


//-----------------------------------------------------------------------------------------------------------
void loop() 
{
    if(millis() - last_update > update_time_interval)
    { 
      last_update = millis();
      timeClient->update();
      Serial.println("Time synced!");
    }
    
    Serial.println(timeClient->getFormattedTime());

    //process start
    if(ts.start_trigger_on)
    {
      if(ts.week_days[timeClient->getDay()])
      {
        if(ts.start_hour == timeClient->getHours())
        {
          if(ts.start_minute == timeClient->getMinutes())
          {
              if(!sensor_active)
              {
                  digitalWrite(relay_switch_pin, HIGH);
                  sensor_active = true;
              }
          }
        }
      }
    }

    //process stop
    if(ts.stop_trigger_on)
    {
      if(ts.week_days[timeClient->getDay()])
      {
        if(ts.stop_hour == timeClient->getHours())
        {
          if(ts.stop_minute == timeClient->getMinutes())
          {
              if(sensor_active)
              {
                  digitalWrite(relay_switch_pin, LOW);
                  sensor_active = false;
              }
          }
        }
      }
    }

    Blynk.run();

    delay(5000);
    // put your main code here, to run repeatedly:
    
}


//-----------------------------------------------------------------------------------------------------------
BLYNK_WRITE(V1) 
{
  TimeInputParam t(param);

  // Process start time

  if (t.hasStartTime())
  {
    Serial.println(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
    ts.start_hour = t.getStartHour();
    ts.start_minute = t.getStartMinute();
    ts.start_trigger_on = true;
  }
  else
  {
    //turn off start trigger;
    ts.start_trigger_on = false;
  }

  // Process stop time

  if (t.hasStopTime())
  {
    Serial.println(String("Stop: ") +
                   t.getStopHour() + ":" +
                   t.getStopMinute() + ":" +
                   t.getStopSecond());
    ts.stop_hour = t.getStopHour();
    ts.stop_minute = t.getStopMinute();
    ts.stop_trigger_on = true;
  }
  else
  {
      //turn off stop strigger
    ts.stop_trigger_on = false;
  }

  // Process timezone
  // Timezone is already added to start/stop time

  Serial.println(String("Time zone: ") + t.getTZ());

  // Get timezone offset (in seconds)
  Serial.println(String("Time zone offset: ") + t.getTZ_Offset());

  // Process weekdays (1. Mon, 2. Tue, 3. Wed, ...)
  int days_off_count{0};
  for (int i = 1; i <= 7; i++) 
  {
    if (t.isWeekdaySelected(i)) 
    {
      Serial.println(String("Day ") + i + " is selected");
      ts.week_days[i % 7] = true;
    }
    else
    {
      ts.week_days[i % 7] = false;
      days_off_count++;
    }
  }

  ts.start_trigger_on = (days_off_count == 7) ? false : true;
  ts.stop_trigger_on = (days_off_count == 7) ? false : true;

  save_trigger_settings();
  
  Serial.println();
}


//-----------------------------------------------------------------------------------------------------------
BLYNK_WRITE(V2)
{
    int state = param.asInt();
    digitalWrite(relay_switch_pin, state);
    sensor_active = (state) ? true : false;
    Serial.printf("Manual activation: %d", state);
}


//-----------------------------------------------------------------------------------------------------------
void save_trigger_settings() 
{
    DynamicJsonDocument doc(2048);
    JsonObject obj = doc.to<JsonObject>();
    obj["start_hour"] = ts.start_hour;
    obj["start_minute"] = ts.start_minute;
    obj["stop_hour"] = ts.stop_hour;
    obj["stop_minute"] = ts.stop_minute;
    obj["start_trigger_on"] = static_cast<int>(ts.start_trigger_on);
    obj["stop_trigger_on"] = static_cast<int>(ts.stop_trigger_on);
    auto week_array = doc.createNestedArray("week");
    Serial.println("Serializing week");
    for(auto i = 0; i < 7; i++)
    {
      week_array.add(ts.week_days[i]);
    }
    Serial.println("Week serialized");
    LittleFS.begin();
    Serial.println("Opening file");
    File settings_file = LittleFS.open("trigger_settings.json", "w");
    Serial.println("Serializing json");
    serializeJson(doc, settings_file);
    Serial.println("Ending...");
    LittleFS.end();
}


//-----------------------------------------------------------------------------------------------------------
void load_trigger_settings() 
{
    DynamicJsonDocument doc(2048);
    LittleFS.begin();
    File settings_file = LittleFS.open("trigger_settings.json", "r");
    auto err = deserializeJson(doc, settings_file);
    switch (err.code()) 
    {
        case DeserializationError::Ok:
            Serial.println("Deserialization succeeded");
            break;
        case DeserializationError::InvalidInput:
            Serial.println("Invalid input!");
            break;
        case DeserializationError::NoMemory:
            Serial.println("Not enough memory");
            break;
        default:
            Serial.println("Deserialization failed");
            break;
    }
    LittleFS.end();

    ts.start_hour = doc["start_hour"];
    ts.start_minute = doc["start_minute"];
    ts.stop_hour = doc["stop_hour"];
    ts.stop_minute = doc["stop_minute"];
    ts.start_trigger_on = static_cast<bool>(doc["start_trigger_on"]);
    ts.stop_trigger_on = static_cast<bool>(doc["stop_trigger_on"]);
    

    uint8_t i = 0;
    for(JsonVariant day : doc["week"].as<JsonArray>())
    {
      ts.week_days[i] = day.as<bool>();
      i++;
    }

    Serial.println("Loaded settings.");
    Serial.printf("Start: %d:%d. Stop: %d:%d\n", ts.start_hour, ts.start_minute, ts.stop_hour, ts.stop_minute);
    Serial.printf("Start_trig - %d, Stop_trig - %d\n", ts.start_trigger_on, ts.stop_trigger_on);
    for(auto i = 0; i < 7; i++)
    {
      Serial.printf("Day: %d = %d\n", i, ts.week_days[i]);
    }
    Serial.println();
}
