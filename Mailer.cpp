#include "Mailer.h"
#include "config.h"
#include "Clock.h"
#include "Wdt.h"
#include "secrets.h"
#include <PortentaEthernet.h>
#include <Ethernet.h>

// Gmail via SMTP su TLS diretto (porta 465) con AUTH LOGIN e password per app.
// TLS fornito da ESP_SSLClient sopra a EthernetClient.
// L'invio avviene nel loop principale e blocca per circa 1,5 s a mail (misurato).

#define QUEUE_LEN 8
struct Msg { char subject[64]; char body[160]; };
static Msg      q[QUEUE_LEN];
static uint8_t  qHead = 0, qCount = 0;
// Pausa tra gli invii. Il confronto e' sempre "tempo trascorso >= attesa" (aritmetica senza segno)
// e la pausa viene azzerata appena finita: resta corretto anche dopo mesi di funzionamento
// e quando millis() ricomincia da zero.
static bool     waiting = false;
static uint32_t waitStart = 0, waitMs = 0;

#if MAIL_ENABLED

#include <ESP_SSLClient.h>
#if !MAIL_TLS_INSECURE
#include "certs.h"
#endif

static EthernetClient baseClient;
static ESP_SSLClient  tls;
static bool           tlsReady = false;

static int         lastCode = -1;
static const char *lastStep = "";

static void b64(const char *in, char *out, size_t outLen) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in), o = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)(uint8_t)in[i] << 16;
    if (i + 1 < n) v |= (uint32_t)(uint8_t)in[i + 1] << 8;
    if (i + 2 < n) v |= (uint8_t)in[i + 2];
    if (o + 5 > outLen) break;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
}

// La password per app viene mostrata da Google a gruppi di 4 caratteri: togli gli spazi
static void passNoSpaces(char *out, size_t n) {
  size_t o = 0;
  for (const char *p = SMTP_PASS; *p && o + 1 < n; p++) if (*p != ' ') out[o++] = *p;
  out[o] = 0;
}

// Legge una risposta SMTP (anche multi-riga) e ritorna il codice numerico
static int smtpReply(Client &c) {
  uint32_t t0 = millis();
  char line[128]; size_t n = 0;
  while (millis() - t0 < 8000) {
    wdtKick();
    while (c.available()) {
      char ch = c.read();
      if (ch == '\n') {
        line[n] = 0;
        if (n >= 4 && line[3] == ' ') return atoi(line);   // ultima riga
        n = 0;
      } else if (ch != '\r' && n < sizeof(line) - 1) {
        line[n++] = ch;
      }
      t0 = millis();
    }
    if (!c.connected() && !c.available()) break;
  }
  return -1;
}

static bool smtpCmd(Client &c, const char *cmd, int expect, const char *label) {
  c.print(cmd); c.print("\r\n");
  lastStep = label;
  lastCode = smtpReply(c);
#if MAIL_DEBUG
  Serial.print("MAIL: "); Serial.print(label); Serial.print(" -> "); Serial.println(lastCode);
#endif
  return lastCode == expect;
}

static bool sendMail(const Msg &m) {
  if (!tlsReady) {
    tls.setClient(&baseClient);
#if MAIL_TLS_INSECURE
    tls.setInsecure();
#else
    tls.setCACert(MAIL_ROOT_CA);
    tls.setX509Time(time(NULL));
#endif
    tls.setBufferSizes(2048, 1024);
    tlsReady = true;
  }
  Client &c = tls;

  lastStep = "connessione TLS"; lastCode = -1;
  if (!tls.connect(SMTP_HOST, SMTP_PORT)) { tls.stop(); return false; }

  lastStep = "banner"; lastCode = smtpReply(c);
  bool ok = lastCode == 220;
  ok = ok && smtpCmd(c, "EHLO " MAIL_HELO, 250, "EHLO");

  char tmp[128], pw[64];
  ok = ok && smtpCmd(c, "AUTH LOGIN", 334, "AUTH");
  if (ok) { b64(SMTP_USER, tmp, sizeof(tmp)); ok = smtpCmd(c, tmp, 334, "AUTH utente"); }
  if (ok) { passNoSpaces(pw, sizeof(pw)); b64(pw, tmp, sizeof(tmp)); ok = smtpCmd(c, tmp, 235, "AUTH password"); }
  memset(pw, 0, sizeof(pw)); memset(tmp, 0, sizeof(tmp));

  ok = ok && smtpCmd(c, "MAIL FROM:<" MAIL_FROM ">", 250, "MAIL FROM");

  if (ok) {                                     // piu' destinatari separati da virgola
    char list[sizeof(MAIL_TO)];
    strcpy(list, MAIL_TO);
    int accepted = 0;
    for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
      while (*tok == ' ') tok++;
      if (!*tok) continue;
      snprintf(tmp, sizeof(tmp), "RCPT TO:<%s>", tok);
      if (smtpCmd(c, tmp, 250, "RCPT TO")) accepted++;
    }
    ok = accepted > 0;
  }

  ok = ok && smtpCmd(c, "DATA", 354, "DATA");
  if (ok) {
    c.print("From: " MAIL_FROM "\r\n");
    c.print("To: " MAIL_TO "\r\n");
    c.print("Subject: "); c.print(m.subject); c.print("\r\n");
    c.print("Content-Type: text/plain; charset=utf-8\r\n\r\n");
    c.print(m.body); c.print("\r\n.\r\n");
    lastStep = "fine messaggio"; lastCode = smtpReply(c);
    ok = lastCode == 250;
  }
  if (c.connected()) c.print("QUIT\r\n");
  c.stop();
  return ok;
}

static void workOnce() {
  if (waiting && millis() - waitStart >= waitMs) waiting = false;
  if (qCount == 0 || waiting) return;
  if (!netOnline()) return;                    // internet assente: le mail restano in coda, nessun tentativo
  if (netCheckAgeMs() > MAIL_PROBE_MS && !clockNtpSync()) {   // verifica rapida prima del TLS
    Serial.println("MAIL: internet assente, invio sospeso");
    return;
  }

  Serial.println("MAIL: invio in corso...");
  uint32_t t0 = millis();
  bool sent = sendMail(q[qHead]);
  uint32_t dt = millis() - t0;

  if (sent) {
    qHead = (qHead + 1) % QUEUE_LEN;
    qCount--;
    waiting = true; waitStart = millis(); waitMs = MAIL_GAP_MS;   // pausa prima della prossima
    Serial.print("MAIL: inviata ("); Serial.print((int)dt); Serial.println(" ms)");
  } else {
    waiting = true; waitStart = millis(); waitMs = MAIL_RETRY_MS; // riprova piu' tardi, resta in coda
    Serial.print("MAIL: errore a '"); Serial.print(lastStep);
    Serial.print("' codice "); Serial.print(lastCode);
    Serial.print(" ("); Serial.print((int)dt); Serial.println(" ms)");
  }
}

#endif  // MAIL_ENABLED

namespace Mailer {

void queueAlarm(const char *name, bool active) {
#if !MAIL_ENABLED
  (void)name; (void)active;
#else
  if (!netOnline()) return;             // internet assente: evento solo nello storico, nessuna e-mail
  if (qCount >= QUEUE_LEN) return;      // coda piena: scarta (resta nello storico)
  char ts[24]; clockFormatLocal(clockNowUtc(), ts, sizeof(ts));
  Msg &m = q[(qHead + qCount) % QUEUE_LEN];
  snprintf(m.subject, sizeof(m.subject), "[Pompe] %s: %s", active ? "ALLARME" : "RIENTRO", name);
  snprintf(m.body, sizeof(m.body), "%s\r\nEvento: %s\r\nData/ora: %s\r\n",
           active ? "Allarme attivato" : "Allarme rientrato", name, ts);
  qCount++;
#endif
}

void process() {
#if MAIL_ENABLED
  workOnce();
#endif
}

uint8_t pending() { return qCount; }

}  // namespace Mailer
