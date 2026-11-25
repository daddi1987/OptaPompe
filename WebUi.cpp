#include "WebUi.h"
#include "config.h"
#include "Plant.h"
#include "EventLog.h"
#include "Stats.h"
#include "Clock.h"
#include "WebPage.h"
#include <PortentaEthernet.h>
#include <Ethernet.h>

static EthernetServer server(WEB_PORT);

// Invia tutto il buffer gestendo le scritture parziali (la scheda puo' accettarne meno del richiesto)
static bool sendAll(EthernetClient &c, const uint8_t *p, size_t n) {
  uint32_t t0 = millis();
  while (n) {
    if (!c.connected()) return false;
    size_t k = c.write(p, n < 512 ? n : 512);
    if (k == 0) {
      if (millis() - t0 > 3000) return false;   // client fermo: rinuncia
      delay(1);
      continue;
    }
    p += k; n -= k; t0 = millis();
  }
  return true;
}

// Accumula l'output in un buffer e lo invia a blocchi: poche scritture grandi
// invece di centinaia di piccole (lente e causa di loop lenti).
class Out {
 public:
  explicit Out(EthernetClient &cl) : c(cl), n(0), ok(true) {}
  void print(const char *s) {
    while (*s) { if (n == sizeof(buf)) flush(); buf[n++] = (uint8_t)*s++; }
  }
  void print(long v) { char t[16]; snprintf(t, sizeof(t), "%ld", v); print(t); }
  void flush() { if (n && ok) ok = sendAll(c, buf, n); n = 0; }
 private:
  EthernetClient &c;
  uint8_t buf[1024];
  size_t n;
  bool ok;
};

// ---- helpers ----------------------------------------------------------------

static bool readRequest(EthernetClient &c, char *path, size_t plen,
                        char *method, size_t mlen, bool &authOk, bool &confirm) {
  char line[160]; size_t n = 0;
  bool first = true; authOk = false; confirm = false; path[0] = 0; method[0] = 0;
  uint32_t t0 = millis();
  while (c.connected() && millis() - t0 < WEB_REQ_TIMEOUT_MS) {
    if (!c.available()) continue;
    char ch = c.read();
    if (ch == '\n') {
      line[n] = 0;
      if (first) {                                   // "GET /path HTTP/1.1"
        first = false;
        size_t ml = strcspn(line, " ");
        if (ml >= mlen) ml = mlen - 1;
        memcpy(method, line, ml); method[ml] = 0;
        char *p = strchr(line, ' ');
        if (!p) return false;
        p++;
        char *e = strchr(p, ' ');
        if (e) *e = 0;
        strncpy(path, p, plen - 1); path[plen - 1] = 0;
      } else if (n == 0 || (n == 1 && line[0] == '\r')) {
        return true;                                 // fine header
      } else if (strncasecmp(line, "X-Confirm: clear", 16) == 0) {
        confirm = true;                              // intestazione richiesta dall'azzeramento
      } else if (strncasecmp(line, "Authorization: Basic ", 21) == 0) {
        char *v = line + 21;
        size_t l = strlen(v);
        while (l && (v[l - 1] == '\r' || v[l - 1] == ' ')) v[--l] = 0;
        authOk = strcmp(v, WEB_AUTH_B64) == 0;
      }
      n = 0;
    } else if (n < sizeof(line) - 1) {
      line[n++] = ch;
    }
  }
  return false;
}

static void sendHeader(Out &o, const char *type, long contentLength = -1) {
  o.print("HTTP/1.1 200 OK\r\nContent-Type: "); o.print(type);
  if (contentLength >= 0) { o.print("\r\nContent-Length: "); o.print(contentLength); }
  o.print("\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
}

static void sendStatus(Out &o) {
  const PlantState &s = Plant::state();
  char ts[24]; clockFormatLocal(clockNowUtc(), ts, sizeof(ts));
  sendHeader(o, "application/json");
  o.print("{\"time\":\""); o.print(ts); o.print("\"");
  o.print(",\"online\":"); o.print(netOnline());
  o.print(",\"mainsOk\":"); o.print(s.mainsOk);
  o.print(",\"pumps\":[");
  for (int i = 0; i < 2; i++) {
    if (i) o.print(",");
    o.print("{\"run\":");       o.print(s.pumpRun[i]);
    o.print(",\"thermalOk\":"); o.print(s.thermalOk[i]);
    o.print(",\"overfill\":");  o.print(s.overfill[i]);
    o.print("}");
  }
  o.print("],\"alarms\":[");
  for (uint8_t i = 0; i < AL_COUNT; i++) {
    const AlarmState &a = Plant::alarm((AlarmId)i);
    char since[24]; clockFormatLocal(a.since, since, sizeof(since));
    if (i) o.print(",");
    o.print("{\"name\":\""); o.print(Plant::alarmName((AlarmId)i));
    o.print("\",\"active\":"); o.print(a.active);
    o.print(",\"since\":\""); o.print(since); o.print("\"}");
  }
  o.print("]}");
}

static void sendEvents(Out &o) {
  sendHeader(o, "application/json");
  o.print("[");
  LogEntry e;
  for (uint16_t i = 0; EventLog::get(i, e); i++) {
    char ts[24]; clockFormatLocal(e.ts, ts, sizeof(ts));
    bool isAlarm = e.code >= EV_AL_MAINS;
    bool isPump  = e.code == EV_PUMP1_RUN || e.code == EV_PUMP2_RUN;
    const char *what = isAlarm ? (e.active ? "ATTIVAZIONE" : "rientro")
                     : (isPump ? (e.active ? "start" : "stop") : "");
    if (i) o.print(",");
    o.print("{\"time\":\""); o.print(ts);
    o.print("\",\"name\":\""); o.print(eventName(e.code));
    o.print("\",\"kind\":\""); o.print(isAlarm ? "alarm" : "info");
    o.print("\",\"on\":"); o.print(e.active);
    o.print(",\"what\":\""); o.print(what); o.print("\"}");
  }
  o.print("]");
}

static void sendStats(Out &o) {
  sendHeader(o, "application/json");
  uint32_t u = clockNowUtc();
  o.print("{\"today\":"); o.print((long)(u ? clockLocalDay(u) : 0));
  o.print(",\"undated\":"); o.print((long)Stats::undated());
  o.print(",\"days\":[");
  uint16_t day, n;
  for (uint16_t i = 0; Stats::get(i, day, n); i++) {
    if (i) o.print(",");
    o.print("["); o.print((long)day); o.print(","); o.print((long)n); o.print("]");
  }
  o.print("]}");
}

namespace WebUi {

void begin() { server.begin(); }

void handle() {
  EthernetClient c = server.available();
  if (!c) return;

  uint32_t t0 = millis();
  char path[48], method[8]; bool auth, confirm;
  if (readRequest(c, path, sizeof(path), method, sizeof(method), auth, confirm)) {
    Out o(c);
    if (!auth) {
      o.print("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Pompe\"\r\n"
              "Connection: close\r\nContent-Length: 0\r\n\r\n");
    } else if (strcmp(path, "/api/status") == 0) {
      sendStatus(o);
    } else if (strcmp(path, "/api/events") == 0) {
      sendEvents(o);
    } else if (strcmp(path, "/api/stats") == 0) {
      sendStats(o);
    } else if (strcmp(path, "/api/clear") == 0) {
      if (strcmp(method, "POST") == 0 && confirm) {   // solo POST con l'intestazione X-Confirm
        EventLog::clear();
        Stats::clear();                               // anche le statistiche giornaliere
        EventLog::add(EV_LOG_CLEARED, true);
        sendHeader(o, "application/json");
        o.print("{\"ok\":1}");
      } else {
        o.print("HTTP/1.1 405 Method Not Allowed\r\nAllow: POST\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
      }
    } else if (strcmp(path, "/") == 0) {
      size_t len = strlen(PAGE);
      sendHeader(o, "text/html; charset=utf-8", (long)len);
      o.flush();
      sendAll(c, (const uint8_t *)PAGE, len);
    } else {
      o.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    }
    o.flush();
  }
  c.flush();          // lascia partire i dati prima di chiudere
  delay(5);
  c.stop();

  uint32_t dt = millis() - t0;
  if (dt > 500) {
    Serial.print("WEB: richiesta lenta ("); Serial.print((int)dt);
    Serial.print(" ms) "); Serial.println(path);
  }
}

}  // namespace WebUi
