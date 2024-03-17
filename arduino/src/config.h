/**
 * @file config.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-15
 *
 * @copyright Copyright (c) 2024
 *
 *
 * For reference:
 * https://github.com/256dpi/arduino-mqtt/blob/master/examples/ArduinoEthernetShield/ArduinoEthernetShield.ino
 * https://github.com/lasselukkari/aWOT/blob/master/examples/Ethernet/Ethernet.ino
 * https://github.com/lasselukkari/aWOT/blob/master/examples/RequestContext/RequestContext.ino
 */

#include <pins_arduino.h>
#include <SPI.h>
#include "boards.h"
#include <Ethernet.h>
#include <Dhcp.h>
#include <MQTT.h>
#include <aWOT.h>
#include <ArduinoJson.h>
#include "default_constants.h"

byte ethernetMacAddress[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};

struct RestContext
{
  IPAddress ip;
  Request::MethodType method;
  char path[100];
};

enum Commands
{
  READ_DIGITAL,
  READ_ANALOG,
  WRITE_DIGITAL,
  WRITE_ANALOG,
} commands;

class StringsHelper
{
public:
  static String semiSplit(String data, char separator, int index)
  {
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
      if (data.charAt(i) == separator || i == maxIndex)
      {
        found++;
        strIndex[0] = strIndex[1] + 1;
        strIndex[1] = (i == maxIndex) ? i + 1 : i;
      }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
  }
};