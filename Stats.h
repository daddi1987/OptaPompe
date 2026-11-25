#pragma once
#include <Arduino.h>
#include "config.h"

// Conteggio giornaliero delle anomalie (attivazioni di allarme), persistente in flash.
// Si conservano solo i giorni con almeno un'anomalia, in un buffer ciclico di STATS_DAYS.
namespace Stats {
  bool begin();                                       // true se azzerate da LOG_RESET_ID
  void clear();                                       // azzera subito (anche in flash)
  void count();                                       // una nuova anomalia oggi
  void flush();                                       // da chiamare nel loop: salva se serve
  uint16_t undated();                                 // anomalie contate senza ora valida
  bool get(uint16_t i, uint16_t &day, uint16_t &n);   // i = 0 -> giorno piu' vecchio
}
