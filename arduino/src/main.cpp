#include "config.h"
#include "device_config.h"
#include "state.h"
#include <avr/wdt.h>

Application restApp;
MQTTClient mqttClient;
// TODO: Verify how to change this with the configuration value
EthernetServer ethServer(DEFAULT_HTTP_SERVER_PORT);
EthernetClient mqttEthClient;
EthernetClient httpEthClient;

DeviceConfig deviceConfig;
DeviceConfigProvider deviceConfigProvider;

GlobalStateProvider stateProvider;

Scheduler tasksRunner;

void parseStateChanges();
void broadcastMQTTStatus();

Task tParseStateChanges(2000, TASK_FOREVER, &parseStateChanges);
Task tBroadcastMQTTStatus(30000, TASK_FOREVER, &broadcastMQTTStatus);

/**
 * This variable signals when the device needs
 * to be rebooted pragmatically.
 *
 */
bool rebootOnNextLoop = false;

void restFillContext(Request &req, Response &res)
{
  RestContext *ctx = (RestContext *)req.context;
  ctx->method = req.method();
  strlcpy(ctx->path, req.path(), strlen(req.path()));
}

void restStatus(Request &req, Response &response)
{
  response.set("Content-Type", "application/json");
  response.println(stateProvider.generateJsonState(deviceConfig, Ethernet.localIP()));
}

void restResetToDefault(Request &req, Response &response)
{
  deviceConfigProvider.resetToDefault();

  rebootOnNextLoop = true;

  response.set("Content-type", "text/plain");
  response.sendStatus(200);
}

void restReboot(Request &req, Response &response)
{
  rebootOnNextLoop = true;

  response.set("Content-type", "text/plain");
  response.sendStatus(200);
}

String handleCommand(String argument, Commands command)
{
  // Prevent cross initialization inside switch
  String data = "";
  int pinIndex = -1;
  int pinValue = -1;

  switch (command)
  {
  case Commands::READ_ANALOG:
    return String(analogRead(argument.toInt()));
    break;
  case Commands::READ_DIGITAL:
    return String(digitalRead(argument.toInt()));
    break;
  case Commands::WRITE_DIGITAL:
    data = StringsHelper::semiSplit(argument, ':', 0);

    if (data == "")
    {
      return String(F("ERROR: Invalid digital write pin. Assure the number is an integer"));
    }

    // Handle the builtin led
    if (data == F("LED_BUILTIN"))
    {
      pinIndex = LED_BUILTIN;
    }
    else
    {
      pinIndex = data.toInt();
    }

    data = StringsHelper::semiSplit(argument, ':', 1);

    if (data == "")
    {
      return String(F("ERROR: Invalid digital write value. Assure the number is either 0 or 1"));
    }

    pinValue = data.toInt();

    switch (pinValue)
    {
    case 0:
      digitalWrite(pinIndex, LOW);
      break;
    case 1:
      digitalWrite(pinIndex, HIGH);
      break;
    default:
      return String(F("ERROR: Invalid digital write value. Assure the number is either 0 or 1"));
      break;
    }

    break;
  case Commands::WRITE_ANALOG:
    data = StringsHelper::semiSplit(argument, ':', 0);

    if (data == "")
    {
      return String(F("ERROR: Invalid analog write pin. Assure the number is an integer"));
    }

    pinIndex = data.toInt();

    data = StringsHelper::semiSplit(argument, ':', 1);

    if (data == "")
    {
      return String(F("ERROR: Invalid analog write value. Assure the number is either 0 or 1"));
    }

    pinValue = data.toInt();

    analogWrite(pinIndex, pinValue);

    break;
  case Commands::REBOOT:
    rebootOnNextLoop = true;
    break;
  case Commands::RESET:
    deviceConfigProvider.resetToDefault();
    rebootOnNextLoop = true;
    break;
  default:
    return String(F("ERROR: Invalid command"));
    break;
  }

  return String(F("Success"));
}

/**
 * This function handles all the logic for parsing incoming commands to the device.
 *
 * @param topic The topic on which the message was sent
 * @param payload The content of the message
 */
String processIncomingMessage(String topic, String payload)
{
  // Print some basic informations
  Serial.print(F("Received message from topic "));
  Serial.print(topic);
  Serial.print(F(" - content: "));
  Serial.println(payload);

  JsonDocument json;

  DeserializationError error = deserializeJson(json, payload);

  if (error)
  {
    return String(F("ERROR: Invalid json received"));
  }

  String command = json[F("command")];
  String arguments = json[F("arguments")];

  if (command == F("READ_DIGITAL"))
  {
    return handleCommand(arguments, Commands::READ_DIGITAL);
  }
  else if (command == F("READ_ANALOG"))
  {
    return handleCommand(arguments, Commands::READ_ANALOG);
  }
  else if (command == F("WRITE_DIGITAL"))
  {
    return handleCommand(arguments, Commands::WRITE_DIGITAL);
  }
  else if (command == F("WRITE_ANALOG"))
  {
    return handleCommand(arguments, Commands::WRITE_ANALOG);
  }
  else if (command == F("RESET"))
  {
    return handleCommand(arguments, Commands::RESET);
  }
  else if (command == F("REBOOT"))
  {
    return handleCommand(arguments, Commands::REBOOT);
  }
  else
  {
    return String(F("ERROR: Invalid command"));
  }
}

void mqttProcessMessage(String &topic, String &payload)
{
  processIncomingMessage(String(topic), String(payload));
}

void mqttAdvertisePresence()
{
  mqttClient.publish(
      "ardu-test/advertise",
      stateProvider.generateJsonAdvertise(deviceConfig, Ethernet.localIP()));
}

void mqttConnect()
{
  // Print some basic informations
  Serial.print("Connecting to MQTT Host ");
  Serial.println(deviceConfig.MQTT_SERVER_HOST);

  // Connect to the MQTT host with the specified client id
  int maxRetries = deviceConfig.MQTT_CONNECTION_RETRIES;

  while (!mqttClient.connect(deviceConfig.MQTT_DEVICE_ID.c_str()) && maxRetries > 0)
  {
    maxRetries--;

    Serial.print(F("ERROR: MQTT was unable to connect to "));
    Serial.println(deviceConfig.MQTT_SERVER_HOST);
    Serial.print(F("ERROR: MQTT code "));
    Serial.println(mqttClient.lastError());
    Serial.println(F("Retrying in 2s"));

    delay(2000);
  }

  if (mqttClient.connected())
  {
    Serial.print(F("MQTT successfully connected with client id "));
    Serial.println(deviceConfig.MQTT_DEVICE_ID);

    delay(500);

    // Subscribe to the necessary channels
    Serial.println(F("Subscribing to MQTT channels"));

    mqttClient.subscribe("ardu-test/receive");

    Serial.println(F("Successfully subscribed to MQTT channels"));

    // Advertise presence
    mqttAdvertisePresence();
  }
  else
  {
    Serial.print(F("ERROR: MQTT was unable to connect to "));
    Serial.print(deviceConfig.MQTT_SERVER_HOST);
    Serial.print(F(" in "));
    Serial.print(deviceConfig.MQTT_CONNECTION_RETRIES);
    Serial.println(F(" retries"));
  }
}

/**
 * This function reboots the device.
 *
 */
void reboot()
{
  Serial.println("--- REBOOT ---");

  rebootOnNextLoop = false;

  if (mqttClient.connected())
  {
    mqttClient.disconnect();
  }

  if (httpEthClient.available())
  {
    httpEthClient.flush();
    httpEthClient.stop();
  }

  ethServer.flush();

  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
}

void setup()
{
  // Enable the builtin led
  pinMode(LED_BUILTIN, OUTPUT);

  // Open the serial connection
  Serial.begin(SERIAL_CONNECTION_SPEED);

  // Wait for serial connection to initialize
  while (!Serial)
  {
  }

  Serial.println(F("Serial initialized!"));

  // Print ardumi common informations
  Serial.print(F("Software build numer: "));
  Serial.println(VERSION);

  // Read device configuration from EEPROM
  Serial.println(F("Reading device configuration"));
  deviceConfig = deviceConfigProvider.readFromEEprom();
  Serial.println(F("Configuration loaded successfully"));

  Serial.print(F("Device unique id: "));
  Serial.println(deviceConfig.DEVICE_UNIQUE_ID);
  Serial.print(F("Device configuration version: "));
  Serial.println(deviceConfig.DEVICE_CONFIG_VERSION);

  // Initialize ethernet with DHCP
  delay(500);
  Serial.println(F("Initializing ethernet with dhcp"));

  if (Ethernet.begin(ethernetMacAddress) == 0)
  {
    Serial.println(F("ERROR: Failed to configure Ethernet using DHCP"));

    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println(F("ERROR: Ethernet shield was not found."));
    }
    else if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println(F("ERROR: Ethernet cable is not connected."));
    }

    // We cannot continue. So we will stop right here.
    Serial.println(F("Device will now reboot in 60 seconds."));
    delay(60000);
    reboot();
  }

  Serial.print(F("Ethernet initialized correctly. Device ip address is: "));
  Serial.println(Ethernet.localIP());

  // Initialize the web server
  Serial.println(F("Initializing the web server"));

  restApp.use(&restFillContext);
  restApp.get("/status", &restStatus);
  restApp.post("/reboot", &restReboot);
  restApp.post("/reset-to-default", &restResetToDefault);
  ethServer.begin();

  Serial.print(F("Web server initialized correctly on port "));
  Serial.println(deviceConfig.HTTP_SERVER_PORT);

  // Initialize the MQTT connection
  Serial.print(F("Initializing the MQTT connection to "));
  Serial.println(deviceConfig.MQTT_SERVER_HOST);

  mqttClient.begin(deviceConfig.MQTT_SERVER_HOST.c_str(), mqttEthClient);
  mqttClient.setKeepAlive(deviceConfig.MQTT_KEEPALIVE);
  mqttClient.setCleanSession(true);
  mqttClient.setTimeout(deviceConfig.MQTT_TIMEOUT);
  mqttClient.dropOverflow(true);
  mqttClient.onMessage(mqttProcessMessage);

  Serial.println(F("MQTT connection initialized"));

  tasksRunner.addTask(tParseStateChanges);
  tParseStateChanges.enable();
  tasksRunner.addTask(tBroadcastMQTTStatus);
  tBroadcastMQTTStatus.enable();
}

void parseStateChanges()
{
  // Parse all the state changes
  // stateProvider.computeStateChanges(mqttClient, deviceConfig);
}

void broadcastMQTTStatus()
{
  mqttClient.publish("ardu-test/status", stateProvider.generateJsonState(deviceConfig, Ethernet.localIP()));
}

void loop()
{
  // Check if we need to reboot the device
  if (rebootOnNextLoop == true)
  {
    reboot();
  }

  // Check if there are tasks that need to be runned
  tasksRunner.execute();

  // Check if we have any serial message incoming
  if (Serial.available() > 0)
  {
    String serialData = Serial.readString();
    Serial.println(processIncomingMessage("SERIAL", serialData).c_str());
  }

  // Check if we need to renew the DHCP address
  switch (Ethernet.maintain())
  {
  case DHCP_CHECK_NONE:
    // Dont do anything
    break;
  case DHCP_CHECK_RENEW_OK:
    Serial.println(F("Ethernet DHCP renewed correctly"));
    break;
  case DHCP_CHECK_REBIND_OK:
    Serial.print(F("Ethernet DHCP changed ip: "));
    Serial.println(Ethernet.localIP());
    break;
  default:
    // Something went wrong with the renewal of DHCP
    // Because we cannot run the software without Ethernet,
    // we will try to reboot the device to see if something changes.
    Serial.println(F("ERROR: DHCP renewal failed. Cannot continue running"));
    Serial.println(F("The device will now reboot in 60 seconds"));
    delay(60000);
    reboot();
    break;
  }

  // Receive any HTTP incoming requests
  httpEthClient = ethServer.available();

  if (httpEthClient.connected())
  {
    // Initialize a context in order to handle the current request
    RestContext httpEthContext = {
      ip : httpEthClient.remoteIP(),
    };

    Serial.print(F("Received an HTTP request from "));
    Serial.print(httpEthContext.ip);
    Serial.print(F(" for path "));
    Serial.print(httpEthContext.path);
    Serial.println(F(" - processing now"));

    // Process the request
    restApp.process(&httpEthClient, &httpEthContext);

    Serial.println(F("Done processing HTTP request"));

    // Close the connection
    httpEthClient.stop();
  }

  // Connect the MQTT client in case we are not connected
  if (!mqttClient.connected())
  {
    Serial.println(F("MQTT is not connected. Initializing MQTT connection process"));
    mqttConnect();
  }

  // Receive any MQTT incoming messages
  mqttClient.loop();
}