#include "Stats.h"
#include "Clock.h"
#include <kvstore_global_api.h>

#define KV_KEY       "/kv/stats"
#define STATS_MAGIC  0x4F505354UL   // "OPST"

struct DayCount { uint16_t day; uint16_t n; };      // day = giorni dal 1/1/1970 (ora locale)

struct Store {
  uint32_t magic;
  uint32_t resetId;
  uint16_t undated;
  uint16_t head;        // prossima posizione di scrittura
  uint16_t count;
  uint16_t pad;
  DayCount d[STATS_DAYS];
};

static Store    st;
static bool     dirty = false;
static uint32_t lastSave = 0;

static void wipe() {
  memset(&st, 0, sizeof(st));
  st.magic   = STATS_MAGIC;
  st.resetId = LOG_RESET_ID;
}

static void saveNow() {
  if (kv_set(KV_KEY, &st, sizeof(st), 0) == 0) { dirty = false; lastSave = millis(); }
  else dirty = true;
}

namespace Stats {

bool begin() {
  size_t actual = 0;
  int rc = kv_get(KV_KEY, &st, sizeof(st), &actual);
  if (rc != 0 || actual != sizeof(st) || st.magic != STATS_MAGIC ||
      st.head >= STATS_DAYS || st.count > STATS_DAYS) {
    wipe();
    return false;
  }
  if (st.resetId != LOG_RESET_ID) { wipe(); saveNow(); return true; }
  return false;
}

void clear() { wipe(); saveNow(); }

void count() {
  uint32_t utc = clockNowUtc();
  if (utc == 0) {                                   // ora non valida: non si sa il giorno
    if (st.undated < 65535) st.undated++;
    dirty = true;
    return;
  }
  uint16_t day = (uint16_t)clockLocalDay(utc);
  for (uint16_t i = 0; i < st.count; i++) {         // il giorno e' gia' presente?
    DayCount &e = st.d[(st.head + STATS_DAYS - 1 - i) % STATS_DAYS];
    if (e.day == day) { if (e.n < 65535) e.n++; dirty = true; return; }
  }
  DayCount &e = st.d[st.head];                      // nuovo giorno (sovrascrive il piu' vecchio)
  e.day = day; e.n = 1;
  st.head = (st.head + 1) % STATS_DAYS;
  if (st.count < STATS_DAYS) st.count++;
  dirty = true;
}

void flush() {
  if (!dirty || millis() - lastSave < 5000UL) return;
  saveNow();
}

uint16_t undated() { return st.undated; }

bool get(uint16_t i, uint16_t &day, uint16_t &n) {
  if (i >= st.count) return false;
  const DayCount &e = st.d[(st.head + STATS_DAYS - st.count + i) % STATS_DAYS];
  day = e.day; n = e.n;
  return true;
}

}  // namespace Stats
