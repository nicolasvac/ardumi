// Using ESP8266 ?
#if defined(ESP8266) || defined(ESP32)
#include "stdlib_noniso.h"
#endif

// Hardware data
#if defined(ESP8266)
#define HARDWARE "esp8266"
#elif defined(ESP32)
#define HARDWARE "esp32"
#else
#define HARDWARE "arduino"
#endif