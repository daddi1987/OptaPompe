#include "Clock.h"
#include "config.h"
#include "Wdt.h"
#include <mbed.h>
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

static const uint32_t MIN_VALID_EPOCH = 1700000000UL;  // ~nov 2023

bool clockValid() { return time(NULL) > (time_t)MIN_VALID_EPOCH; }

uint32_t clockNowUtc() { return clockValid() ? (uint32_t)time(NULL) : 0; }

static bool     g_online = false, g_checked = false;
static uint32_t g_lastCheck = 0;
bool     netOnline()     { return g_online; }
bool     netChecked()    { return g_checked; }
uint32_t netCheckAgeMs() { return millis() - g_lastCheck; }

static bool ntpQuery();

// Ogni sincronizzazione NTP e' anche la prova di raggiungibilita' di internet
bool clockNtpSync() {
  bool ok = ntpQuery();
  g_online = ok; g_checked = true; g_lastCheck = millis();
  return ok;
}

static bool ntpQuery() {
  EthernetUDP udp;
  if (!udp.begin(2390)) return false;

  uint8_t pkt[48] = {0};
  pkt[0] = 0xE3;  // LI=3, VN=4, Mode=3 (client)
  udp.beginPacket(NTP_SERVER, 123);
  udp.write(pkt, 48);
  udp.endPacket();

  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    wdtKick();
    if (udp.parsePacket() >= 48) {
      udp.read(pkt, 48);
      uint32_t secs = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) |
                      ((uint32_t)pkt[42] << 8) | pkt[43];
      udp.stop();
      if (secs < 2208988800UL) return false;
      set_time((time_t)(secs - 2208988800UL));   // RTC dell'Opta, in UTC
      return true;
    }
    delay(10);
  }
  udp.stop();
  return false;
}

// Ultima domenica del mese (marzo/ottobre hanno 31 giorni)
static int lastSunday(int year, int month0) {
  struct tm t = {};
  t.tm_year = year - 1900; t.tm_mon = month0; t.tm_mday = 31; t.tm_hour = 12;
  time_t x = mktime(&t);                 // TZ = UTC su Mbed
  struct tm r; gmtime_r(&x, &r);
  return 31 - r.tm_wday;
}

static bool isDstEu(uint32_t utc) {
  time_t x = utc; struct tm t; gmtime_r(&x, &t);
  int y = t.tm_year + 1900;
  struct tm s = {}; s.tm_year = y - 1900; s.tm_mon = 2;  s.tm_mday = lastSunday(y, 2);  s.tm_hour = 1;
  struct tm e = {}; e.tm_year = y - 1900; e.tm_mon = 9;  e.tm_mday = lastSunday(y, 9);  e.tm_hour = 1;
  return utc >= (uint32_t)mktime(&s) && utc < (uint32_t)mktime(&e);
}

void clockFormatLocal(uint32_t utc, char *buf, size_t len) {
  if (utc == 0) { snprintf(buf, len, "n/d"); return; }
  time_t loc = (time_t)utc + (isDstEu(utc) ? 7200 : 3600);
  struct tm t; gmtime_r(&loc, &t);
  snprintf(buf, len, "%02d/%02d/%04d %02d:%02d:%02d",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
}

// Numero del giorno locale (giorni dal 1/1/1970, ora italiana con ora legale)
uint32_t clockLocalDay(uint32_t utc) {
  return (uint32_t)(((time_t)utc + (isDstEu(utc) ? 7200 : 3600)) / 86400);
}
