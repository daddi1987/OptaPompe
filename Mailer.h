#pragma once
#include <Arduino.h>

namespace Mailer {
  void queueAlarm(const char *name, bool active);  // mette in coda una notifica
  void process();                                  // da chiamare nel loop: invia la mail in coda
  uint8_t pending();
}
