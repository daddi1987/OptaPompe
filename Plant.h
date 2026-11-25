#pragma once
#include <Arduino.h>

// Stato filtrato dell'impianto (dopo antirimbalzo). Indice 0 = pompa 1, 1 = pompa 2
struct PlantState {
  bool pumpRun[2];     // true = in marcia
  bool thermalOk[2];   // false = termica scattata
  bool overfill[2];    // true = galleggiante troppo pieno attivo
  bool mainsOk;        // false = mancanza rete linea pompe
};

enum AlarmId : uint8_t {
  AL_MAINS = 0,
  AL_THERMAL1,
  AL_THERMAL2,
  AL_OVERFILL1,
  AL_OVERFILL2,
  AL_COUNT
};

struct AlarmState {
  bool     active;
  uint32_t since;    // epoch UTC attivazione (0 se ora non valida)
};

namespace Plant {
  void begin();
  void update();                       // legge ingressi, valuta allarmi, scrive log
  const PlantState &state();
  const AlarmState &alarm(AlarmId id);
  const char *alarmName(AlarmId id);
}
