#include <stdio.h>
#include "Log.hpp"
#include "../GEN/SomeIpXf_Cfg.h"
#include "SomeIpXf.h"
#include <string.h>

using namespace as;

Display_Type display, display2;

int main(int argc, char *argv[]) {
  static uint8_t buffer[1024 * 1024];
  static const char *gagues[] = {"Speed", "Tacho", "Fuel"};
  static const char *telltales[] = {"Airbag",  "TurnLeft", "TurnRight",
                                    "LowBeam", "HighBeam", "PosLamp"};

  int32_t length;
  display.gaugesLen = ARRAY_SIZE(gagues);
  for (size_t i = 0; i < display.gaugesLen; i++) {
    strcpy((char *)display.gauges[i].name, gagues[i]);
    display.gauges[i].nameLen = strlen(gagues[i]) + 1;
    display.gauges[i].degree = 0x1234 + i;
#if 0
    if (i & 0x01) {
      display.gauges[i].has_min = true;
      display.gauges[i].min = 0x12345670 + i;
    } else {
      display.gauges[i].has_min = false;
    }
#endif
  }

  display.telltalesLen = ARRAY_SIZE(telltales);
  for (size_t i = 0; i < display.telltalesLen; i++) {
    strcpy((char *)display.telltales[i].name, telltales[i]);
    display.telltales[i].nameLen = strlen(telltales[i]) + 1;
    display.telltales[i].on = i;
  }

  length = SomeIpXf_EncodeStruct(buffer, sizeof(buffer), &display, &SomeIpXf_StructDisplayDef);
  Log::hexdump(Logger::INFO, "display:", buffer, length);

  length = SomeIpXf_DecodeStruct(buffer, (uint32_t)length, &display2, &SomeIpXf_StructDisplayDef);

  return 0;
}
