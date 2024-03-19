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
    JsonDocument json;
    String jsonData = "";

    void sendMqttStateChangeMessage(
        MQTTClient client,
        String mqttTopic,
        GlobalStateChangeType changeType,
        int pinId,
        int previousValue,
        int currentValue)
    {
        json.clear();

        json[F("p")] = pinId;
        json[F("pv")] = previousValue;
        json[F("cv")] = currentValue;

        switch (changeType)
        {
        case DIGITAL:
            json[F("t")] = 1;
            break;
        case ANALOG:
            json[F("t")] = 2;
            break;
        }

        jsonData = "";
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
};