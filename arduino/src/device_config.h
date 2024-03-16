#include <Arduino.h>
#include <ArduinoUniqueID.h>
#include <EEPROM.h>
#include <StreamUtils.h>
#include <ArduinoJson.h>
#include "default_constants.h"

struct DeviceConfig
{
    String DEVICE_UNIQUE_ID;
    int DEVICE_CONFIG_VERSION;
    int HTTP_SERVER_PORT;
    String MQTT_SERVER_HOST;
    String MQTT_DEVICE_ID;
    int MQTT_KEEPALIVE;
    int MQTT_TIMEOUT;
    int MQTT_CONNECTION_RETRIES;
};

class DeviceConfigProvider
{
private:
    String getUniqueId()
    {
        String uniqueId = "";

        for (size_t i = 0; i < UniqueIDsize; i++)
        {
            if (UniqueID[i] < 0x10)
            {
                uniqueId += "0";
            }

            uniqueId += String(UniqueID[i], HEX);
        }

        return uniqueId;
    };

    void clearEEPROM(int start, int end)
    {
        Serial.print(F("Erasing EEPROM from "));
        Serial.print(start);
        Serial.print(F(" to "));
        Serial.println(end);

        for (int i = start; i <= end; i++)
        {
            EEPROM.write(i, 0);
        }
    };

    DeviceConfig getDefaultConfig()
    {
        DeviceConfig defaultConfig = {
            .DEVICE_UNIQUE_ID = getUniqueId(),
            .DEVICE_CONFIG_VERSION = CONFIG_VERSION,
            .HTTP_SERVER_PORT = DEFAULT_HTTP_SERVER_PORT,
            .MQTT_SERVER_HOST = DEFAULT_MQTT_SERVER_HOST,
            .MQTT_DEVICE_ID = getUniqueId(),
            .MQTT_KEEPALIVE = DEFAULT_MQTT_KEEPALIVE,
            .MQTT_TIMEOUT = DEFAULT_MQTT_TIMEOUT,
            .MQTT_CONNECTION_RETRIES = DEFAULT_MQTT_CONNECTION_RETRIES,
        };

        return defaultConfig;
    };

public:
    /**
     * This function tries to read the device configuration from the EEPROM.
     * If some error occurs, it resets everything to the default configuration.
     * It returns the fresh configuration from the EEPROM.
     *
     * @return DeviceConfig
     */
    DeviceConfig readFromEEprom()
    {
        EEPROM.begin();

        JsonDocument jsonConfig;

        // Read the EEPROM content
        EepromStream eepromStream(EEPROM_START, EEPROM_END);
        DeserializationError error = deserializeJson(jsonConfig, eepromStream);

        EEPROM.end();

        // Verify that we have been successfull in reading the configuration.
        // If we have not, we save the default configuration.
        if (error)
        {
            Serial.println(F("Unable to read from EEPROM. Overwriting with default configuration."));

            DeviceConfig defaultConfig = getDefaultConfig();

            saveConfig(defaultConfig);

            return defaultConfig;
        }
        else
        {
            Serial.println(F("Correctly read from EEPROM. Parsing configuration."));

            DeviceConfig parsedConfig = {
                .DEVICE_UNIQUE_ID = jsonConfig[F("device")][F("id")],
                .DEVICE_CONFIG_VERSION = jsonConfig[F("device")][F("cf-version")],
                .HTTP_SERVER_PORT = jsonConfig[F("http")][F("port")],
                .MQTT_SERVER_HOST = jsonConfig[F("mqtt")][F("host")],
                .MQTT_DEVICE_ID = jsonConfig[F("mqtt")][F("id")],
                .MQTT_KEEPALIVE = jsonConfig[F("mqtt")][F("keepalive")],
                .MQTT_TIMEOUT = jsonConfig[F("mqtt")][F("timeout")],
                .MQTT_CONNECTION_RETRIES = jsonConfig[F("mqtt")][F("conn_retries")],
            };

            return parsedConfig;
        }
    };

    /**
     * This function overrites everything inside the EEPROM (from EEPROM_START to EEPROM_END)
     * and saves the config passed as a parameter.
     *
     * @param newConfig
     */
    void saveConfig(DeviceConfig newConfig)
    {
        Serial.println(F("Saving a new configuration to EEPROM"));

        // Clear the content of the EEPROM
        clearEEPROM(EEPROM_START, EEPROM_END);

        JsonDocument jsonConfig;

        jsonConfig[F("device")][F("id")] = newConfig.DEVICE_UNIQUE_ID;
        jsonConfig[F("device")][F("cf-version")] = newConfig.DEVICE_CONFIG_VERSION;

        jsonConfig[F("http")][F("port")] = newConfig.HTTP_SERVER_PORT;

        jsonConfig[F("mqtt")][F("host")] = newConfig.MQTT_SERVER_HOST;
        jsonConfig[F("mqtt")][F("id")] = newConfig.DEVICE_UNIQUE_ID;
        jsonConfig[F("mqtt")][F("keepalive")] = newConfig.MQTT_KEEPALIVE;
        jsonConfig[F("mqtt")][F("timeout")] = newConfig.MQTT_TIMEOUT;
        jsonConfig[F("mqtt")][F("conn_retries")] = newConfig.MQTT_CONNECTION_RETRIES;

        // Write inside the EEPROM
        EEPROM.begin();

        EepromStream eepromStream(EEPROM_START, EEPROM_END);
        serializeJson(jsonConfig, eepromStream);

        EEPROM.end();

        Serial.println(F("Successfully saved configuration in EEPROM"));
    };

    /**
     * This function allows to reset the EEPROM inside the device.
     * IT WONT REBOOT THE DEVICE!
     *
     */
    void resetToDefault()
    {
        saveConfig(getDefaultConfig());
    }
};