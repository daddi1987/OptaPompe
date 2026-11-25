#include "Plant.h"
#include "config.h"
#include "Clock.h"
#include "EventLog.h"
#include "Mailer.h"
#include "Stats.h"

// ---------------- Ingresso digitale con antirimbalzo ----------------
struct DebIn {
  uint8_t  pin;
  bool     stable;
  bool     raw;
  uint32_t t;
  bool     nc;      // true = contatto NC (evento = nessun segnale)
  // Il valore restituito e' sempre "evento attivo": segnale presente per un contatto NO,
  // segnale assente per un contatto NC.
  bool level() const { return (digitalRead(pin) != 0) != nc; }
  void init(uint8_t p, bool isNc) { pin = p; nc = isNc; pinMode(p, INPUT); raw = stable = level(); t = millis(); }
  bool update() {
    bool r = level();
    if (r != raw) { raw = r; t = millis(); }
    if (raw != stable && millis() - t >= DEBOUNCE_MS) stable = raw;
    return stable;
  }
};

static DebIn inRun[2], inThermal[2], inOver[2], inMains;
static PlantState ps;
static AlarmState al[AL_COUNT];

// Timer di persistenza: l'allarme scatta solo se la condizione dura >= ms
struct Persist {
  uint32_t t0 = 0; bool on = false;
  bool eval(bool cond, uint32_t ms) {
    if (!cond) { on = false; return false; }
    if (!on)   { on = true; t0 = millis(); }
    return millis() - t0 >= ms;
  }
};
static Persist pMains, pThermal[2], pOver[2];

static const uint8_t alarmEvent[AL_COUNT] = {
  EV_AL_MAINS, EV_AL_THERMAL1, EV_AL_THERMAL2, EV_AL_OVERFILL1, EV_AL_OVERFILL2
};

const char *Plant::alarmName(AlarmId id) { return eventName(alarmEvent[id]); }

static void setAlarm(AlarmId id, bool active) {
  if (al[id].active == active) return;
  al[id].active = active;
  al[id].since  = active ? clockNowUtc() : 0;
  EventLog::add(alarmEvent[id], active);
  if (active) Stats::count();                 // statistiche: una nuova anomalia
  Mailer::queueAlarm(Plant::alarmName(id), active);
}

namespace Plant {

void begin() {
  inRun[0].init(PIN_PUMP1_RUN, CONTACT_PUMP1_RUN == CONTACT_NC);
  inRun[1].init(PIN_PUMP2_RUN, CONTACT_PUMP2_RUN == CONTACT_NC);
  inThermal[0].init(PIN_THERMAL1_OK, CONTACT_THERMAL1 == CONTACT_NC);
  inThermal[1].init(PIN_THERMAL2_OK, CONTACT_THERMAL2 == CONTACT_NC);
  inOver[0].init(PIN_OVERFILL1, CONTACT_OVERFILL1 == CONTACT_NC);
  inOver[1].init(PIN_OVERFILL2, CONTACT_OVERFILL2 == CONTACT_NC);
  inMains.init(PIN_MAINS_OK, CONTACT_MAINS == CONTACT_NC);

  memset(al, 0, sizeof(al));
  for (int i = 0; i < 2; i++) {       // stato iniziale = lettura attuale (nessun evento spurio)
    ps.pumpRun[i]   = inRun[i].stable;
    ps.thermalOk[i] = !inThermal[i].stable;
    ps.overfill[i]  = inOver[i].stable;
  }
  ps.mainsOk = !inMains.stable;
}

void update() {
  for (int i = 0; i < 2; i++) {
    bool prev = ps.pumpRun[i];
    ps.pumpRun[i]   = inRun[i].update();
    ps.thermalOk[i] = !inThermal[i].update();
    ps.overfill[i]  = inOver[i].update();
    if (ps.pumpRun[i] != prev) EventLog::add(EV_PUMP1_RUN + i, ps.pumpRun[i]);
  }
  ps.mainsOk = !inMains.update();

  setAlarm(AL_MAINS, pMains.eval(!ps.mainsOk, MAINS_DELAY_MS));
  for (int i = 0; i < 2; i++) {
    setAlarm((AlarmId)(AL_THERMAL1 + i),  pThermal[i].eval(!ps.thermalOk[i], THERMAL_DELAY_MS));
    setAlarm((AlarmId)(AL_OVERFILL1 + i), pOver[i].eval(ps.overfill[i], OVERFILL_DELAY_MS));
  }
}

const PlantState &state() { return ps; }
const AlarmState &alarm(AlarmId id) { return al[id]; }

}  // namespace Plant
