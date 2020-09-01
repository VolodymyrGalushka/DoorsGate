#include "main.h"
#include <FS.h>
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
bool                sensor_active{false};
bool                manually_switched{false};

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
    Serial.println("Wifi connected...");

    ntpUDP = new WiFiUDP();
    timeClient = new NTPClient(*ntpUDP);
    timeClient->begin();
    timeClient->setTimeOffset(7*60*60);
    timeClient->forceUpdate();
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
    
    Serial.printf("Hour: %d, minute: %d\n", timeClient->getHours(), timeClient->getMinutes());
    Serial.printf("Day: %d\n", timeClient->getDay());

    //process start
    if(ts.trigger_on)
    {
      if(should_open())
      {
        if(!sensor_active && !manually_switched)
        {
            digitalWrite(relay_switch_pin, HIGH);
            sensor_active = true;
            Blynk.virtualWrite(V2, static_cast<int>(sensor_active)); //todo update on timer
        }
      }
      else
      {
        if(sensor_active && !manually_switched)
          {
              digitalWrite(relay_switch_pin, LOW);
              sensor_active = false;
              Blynk.virtualWrite(V2, static_cast<int>(sensor_active)); //todo update on timer
          }
      }
    }

    Blynk.run();

    delay(1000);
    // put your main code here, to run repeatedly:
    
}


//-----------------------------------------------------------------------------------------------------------
BLYNK_CONNECTED()
{
    Blynk.virtualWrite(V2, static_cast<int>(sensor_active));
    Blynk.syncAll();
}

//-----------------------------------------------------------------------------------------------------------
BLYNK_WRITE(V1) 
{
  TimeInputParam t(param);

  if(t.hasStartTime() && t.hasStopTime())
  {
      ts.trigger_on = true;
  }
  else
  {
      ts.trigger_on = false;
      save_trigger_settings();
      reset_state();
      return;
  }

  if (t.hasStartTime())
  {
    Serial.println(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
    ts.start_hour = t.getStartHour();
    ts.start_minute = t.getStartMinute();
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

  ts.trigger_on = (days_off_count == 7) ? false : true;

  save_trigger_settings();

  reset_state();
  
  Serial.println();
}


//-----------------------------------------------------------------------------------------------------------
BLYNK_WRITE(V2)
{
    int state = param.asInt();
    digitalWrite(relay_switch_pin, state);
    if(should_open())
    {
      manually_switched = (state == LOW) ? true : false;
    }
    else
    {
      manually_switched = (state == HIGH) ? true : false;
    }
    Serial.printf("Manually switched: %d\n", manually_switched);
    sensor_active = (state) ? true : false;
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
    obj["trigger_on"] = static_cast<int>(ts.trigger_on);
    auto week_array = doc.createNestedArray("week");
    Serial.println("Serializing week");
    for(auto i = 0; i < 7; i++)
    {
      week_array.add(ts.week_days[i]);
    }
    Serial.println("Week serialized");
    SPIFFS.begin();
    Serial.println("Opening file");
    File settings_file = SPIFFS.open("trigger_settings.json", "w");
    Serial.println("Serializing json");
    serializeJson(doc, settings_file);
    Serial.println("Ending...");
    SPIFFS.end();
}


//-----------------------------------------------------------------------------------------------------------
void load_trigger_settings() 
{
    DynamicJsonDocument doc(2048);
    SPIFFS.begin();
    File settings_file = SPIFFS.open("trigger_settings.json", "r");
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
    SPIFFS.end();

    ts.start_hour = doc["start_hour"];
    ts.start_minute = doc["start_minute"];
    ts.stop_hour = doc["stop_hour"];
    ts.stop_minute = doc["stop_minute"];
    ts.trigger_on = static_cast<bool>(doc["trigger_on"]);    

    uint8_t i = 0;
    for(JsonVariant day : doc["week"].as<JsonArray>())
    {
      ts.week_days[i] = day.as<bool>();
      i++;
    }

    Serial.println("Loaded settings.");
    Serial.printf("Start: %d:%d. Stop: %d:%d\n", ts.start_hour, ts.start_minute, ts.stop_hour, ts.stop_minute);
    Serial.printf("Trigger_active - %d\n", ts.trigger_on);
    for(auto i = 0; i < 7; i++)
    {
      Serial.printf("Day: %d = %d\n", i, ts.week_days[i]);
    }
    Serial.println();
}


//-----------------------------------------------------------------------------------------------------------
bool should_open()
{
    auto current_hour = timeClient->getHours();
    auto current_minute = timeClient->getMinutes();
    auto till_start = ts.start_hour*60 + ts.start_minute;
    auto till_now = current_hour*60 + current_minute;
    auto till_stop = ts.stop_hour*60 + ts.stop_minute;

    if(ts.week_days[timeClient->getDay()])
    {
      if(till_stop > till_start)
      {
          if(till_now >= till_start && till_now < till_stop)
            return true;
          else
            return false;
      }
      else
      {
          return false;
      }
    }
    else
    {
      return false;
    }
}


//-----------------------------------------------------------------------------------------------------------
void reset_state() 
{
      manually_switched = false;
      digitalWrite(relay_switch_pin, LOW);
      sensor_active = false;
      Blynk.virtualWrite(V2, static_cast<int>(sensor_active));
}


//-----------------------------------------------------------------------------------------------------------
bool should_open_advanced() 
{
    auto current_hour = timeClient->getHours();
    auto current_minute = timeClient->getMinutes();
    auto till_start = ts.start_hour*60 + ts.start_minute;
    auto till_now = current_hour*60 + current_minute;
    auto till_stop = ts.stop_hour*60 + ts.stop_minute;

    if(ts.week_days[timeClient->getDay()])
    {
      if(till_stop < till_start)
      {   
          if(till_now >= till_stop && till_now < till_start)
            return false;
          else
            return true;
      }
      else if(till_stop > till_start)
      {
          if(till_now >= till_start && till_now < till_stop)
            return true;
          else
            return false;
      }
    }
    else
    {
      return false;
    }
}
