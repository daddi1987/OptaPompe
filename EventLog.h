#pragma once
#include <Arduino.h>
#include "config.h"

// Codici evento
enum EventCode : uint8_t {
  EV_BOOT = 0,
  EV_LOG_CLEARED,         // info: storico azzerato
  EV_PUMP1_RUN,           // info: pompa 1 partita / fermata (active = 1/0)
  EV_PUMP2_RUN,           // info: pompa 2
  EV_AL_MAINS,            // da qui in poi allarmi: active = 1 attivazione, 0 rientro
  EV_AL_THERMAL1,
  EV_AL_THERMAL2,
  EV_AL_OVERFILL1,
  EV_AL_OVERFILL2,
  EV_COUNT
};

const char *eventName(uint8_t code);

struct LogEntry {
  uint32_t ts;       // epoch UTC (0 = ora non disponibile)
  uint8_t  code;     // EventCode
  uint8_t  active;   // 1 = attivazione, 0 = rientro
  uint16_t reserved;
};

namespace EventLog {
  bool begin();                         // carica lo storico; true se azzerato da LOG_RESET_ID
  void clear();                         // azzera subito lo storico (anche in flash)
  void add(uint8_t code, bool active);  // aggiunge un evento (timestamp automatico)
  void flush();                         // da chiamare nel loop: salva in flash se serve
  uint16_t count();
  bool get(uint16_t i, LogEntry &out);  // i = 0 -> evento piu' recente
}
