#pragma once

#include <ArduinoJson.h>
#include "device_config.h"
#include <MemoryFree.h>

struct GlobalState_t
{
    int digitalPinsValues[NUM_DIGITAL_PINS];
};

enum GlobalStateChangeType
{
    DIGITAL,
    ANALOG,
};

class GlobalStateProvider
{
private:
    GlobalState_t state;

    void sendMqttStateChangeMessage(
        MQTTClient client,
        String mqttTopic,
        GlobalStateChangeType changeType,
        int pinId,
        int previousValue,
        int currentValue)
    {
        JsonDocument json;

        json[F("pin")] = pinId;
        json[F("previous")] = previousValue;
        json[F("current")] = currentValue;

        switch (changeType)
        {
        case DIGITAL:
            json[F("type")] = 1;
            break;
        case ANALOG:
            json[F("t")] = 2;
            break;
        }

        String jsonData = "";
        serializeJson(json, jsonData);

        client.publish(mqttTopic, jsonData);
    }

public:
    /**
     * Initializes the state with empty values to avoid null errors
     *
     */
    GlobalStateProvider()
    {
        for (int i = 0; i < NUM_DIGITAL_PINS; i++)
        {
            state.digitalPinsValues[i] = 0;
        }
    }

    /**
     * This functions checks for any state changes, and sends
     * a massage for each difference in the state.
     *
     * @param mqtt
     */
    void computeStateChanges(MQTTClient mqtt, DeviceConfig config)
    {
        // Check for changes in the pins
        for (int i = 30; i < NUM_DIGITAL_PINS - NUM_ANALOG_INPUTS; i++)
        {
            int pinPreviousValue = state.digitalPinsValues[i];
            int pinCurrentValue = digitalRead(i);

            Serial.println(freeMemory());

            if (pinCurrentValue != pinPreviousValue)
            {
                sendMqttStateChangeMessage(
                    mqtt,
                    "ardu-test/publish",
                    GlobalStateChangeType::DIGITAL,
                    i,
                    state.digitalPinsValues[i],
                    pinCurrentValue);
            }

            state.digitalPinsValues[i] = pinCurrentValue;
        }
    }

    String generateJsonState(DeviceConfig deviceConfig, IPAddress localIp)
    {
        JsonDocument json;

        json[F("device")][F("free_memory")] = freeMemory();
        json[F("device")][F("id")] = deviceConfig.DEVICE_UNIQUE_ID;
        json[F("device")][F("version")] = VERSION;
        json[F("device")][F("free_memory")] = freeMemory();

        json[F("http")][F("ip")] = localIp;
        json[F("http")][F("port")] = deviceConfig.HTTP_SERVER_PORT;

        json[F("mqtt")][F("host")] = deviceConfig.MQTT_SERVER_HOST;
        json[F("mqtt")][F("id")] = deviceConfig.MQTT_DEVICE_ID;
        json[F("mqtt")][F("keepalive")] = deviceConfig.MQTT_KEEPALIVE;
        json[F("mqtt")][F("timeout")] = deviceConfig.MQTT_TIMEOUT;

        for (int i = 0; i < NUM_DIGITAL_PINS; i++)
        {
            json[F("digital")][F("values")][i] = state.digitalPinsValues[i];
        }

        String jsonData = "";
        serializeJson(json, jsonData);
        return jsonData;
    }

    String generateJsonAdvertise(DeviceConfig deviceConfig, IPAddress localIp)
    {
        JsonDocument json;

        json[F("id")] = deviceConfig.DEVICE_UNIQUE_ID;
        json[F("fw_version")] = VERSION;
        json[F("cf_version")] = deviceConfig.DEVICE_CONFIG_VERSION;
        json[F("serial_speed")] = SERIAL_CONNECTION_SPEED;
        json[F("ip")] = localIp;
        json[F("http_port")] = deviceConfig.HTTP_SERVER_PORT;

        String jsonData = "";
        serializeJson(json, jsonData);
        return jsonData;
    }
};