#include "EventLog.h"
#include "Clock.h"
#include <kvstore_global_api.h>

#define KV_KEY    "/kv/evlog"
#define LOG_MAGIC 0x4F505443UL   // "OPTC" (cambia se cambia il formato dei dati salvati)

struct Store {
  uint32_t magic;
  uint32_t resetId;   // valore di LOG_RESET_ID con cui lo storico e' stato salvato
  uint16_t head;      // prossima posizione di scrittura
  uint16_t count;
  LogEntry e[LOG_SIZE];
};

static Store    st;
static bool     dirty = false;
static uint32_t lastSave = 0;

const char *eventName(uint8_t code) {
  switch (code) {
    case EV_BOOT:         return "Avvio PLC";
    case EV_LOG_CLEARED:  return "Storico azzerato";
    case EV_PUMP1_RUN:    return "Pompa 1 in marcia";
    case EV_PUMP2_RUN:    return "Pompa 2 in marcia";
    case EV_AL_MAINS:     return "Mancanza rete linea pompe";
    case EV_AL_THERMAL1:  return "Termica pompa 1";
    case EV_AL_THERMAL2:  return "Termica pompa 2";
    case EV_AL_OVERFILL1: return "Troppo pieno pompa 1";
    case EV_AL_OVERFILL2: return "Troppo pieno pompa 2";
    default:              return "?";
  }
}

static void wipe() {
  memset(&st, 0, sizeof(st));
  st.magic   = LOG_MAGIC;
  st.resetId = LOG_RESET_ID;
}

static void saveNow() {
  if (kv_set(KV_KEY, &st, sizeof(st), 0) == 0) { dirty = false; lastSave = millis(); }
  else dirty = true;                       // riprova dal flush()
}

namespace EventLog {

bool begin() {
  size_t actual = 0;
  int rc = kv_get(KV_KEY, &st, sizeof(st), &actual);
  if (rc != 0 || actual != sizeof(st) || st.magic != LOG_MAGIC ||
      st.head >= LOG_SIZE || st.count > LOG_SIZE) {
    wipe();                                // primo avvio o formato non valido
    return false;
  }
  if (st.resetId != LOG_RESET_ID) {        // richiesto l'azzeramento da configurazione
    wipe();
    saveNow();
    return true;
  }
  return false;
}

void clear() {
  wipe();
  saveNow();                               // subito: non resta nulla se manca l'alimentazione
}

void add(uint8_t code, bool active) {
  LogEntry &e = st.e[st.head];
  e.ts = clockNowUtc();
  e.code = code;
  e.active = active ? 1 : 0;
  e.reserved = 0;
  st.head = (st.head + 1) % LOG_SIZE;
  if (st.count < LOG_SIZE) st.count++;
  dirty = true;
}

// Scrive in flash al massimo ogni 5 s e solo se ci sono novita' (usura flash)
void flush() {
  if (!dirty || millis() - lastSave < 5000UL) return;
  saveNow();
}

uint16_t count() { return st.count; }

bool get(uint16_t i, LogEntry &out) {
  if (i >= st.count) return false;
  out = st.e[(st.head + LOG_SIZE - 1 - i) % LOG_SIZE];
  return true;
}

}  // namespace EventLog
