/*
  OptaPompe - monitoraggio stazione di sollevamento (Finder Opta, Arduino IDE)

  Funzioni:
   - 2 pompe: stato (in marcia / ferma), termica, galleggiante troppo pieno
   - allarme mancanza rete sulla linea delle pompe
   - allarmi con ritardi e antirimbalzo
   - storico eventi persistente in flash (data/ora via NTP, attivazione e rientro)
   - interfaccia web (stato, allarmi, storico)
   - notifiche e-mail via Gmail (SMTP su TLS)

  Core richiesto: "Arduino Mbed OS Opta Boards"
  Libreria richiesta per le e-mail: "ESP_SSLClient" (Mobizt)
*/

#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <mbed.h>

#include "config.h"
#include "Clock.h"
#include "EventLog.h"
#include "Plant.h"
#include "Mailer.h"
#include "WebUi.h"
#include "Wdt.h"
#include "Leds.h"
#include "Stats.h"

static uint32_t lastNtp = 0;
static mbed::Watchdog &wdt = mbed::Watchdog::get_instance();
static bool wdtOn = false;
void wdtKick() { if (wdtOn) wdt.kick(); }

static void startEthernet() {
#if NET_USE_DHCP
  Ethernet.begin();
#else
  Ethernet.begin(NET_IP, NET_DNS, NET_GATEWAY, NET_SUBNET);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOT");
  Leds::begin();                  // tutti i LED spenti fino a NTP / fine setup

  bool logWiped = EventLog::begin();   // true se LOG_RESET_ID ha azzerato lo storico
  Stats::begin();                      // statistiche giornaliere (azzerate con lo stesso LOG_RESET_ID)
  Plant::begin();

  startEthernet();
  if (clockNtpSync()) { Serial.println("NTP ok"); Leds::ntpFound(); }   // LED NTP
  else Serial.println("NTP non disponibile");
  lastNtp = millis();

  if (logWiped) EventLog::add(EV_LOG_CLEARED, true);   // traccia l'azzeramento (con l'ora)
  EventLog::add(EV_BOOT, true);   // dopo NTP, cosi' ha l'ora corretta
  WebUi::begin();
  Leds::setupDone();              // LED web: setup finito e pagina web attiva
#if MAIL_TEST_AT_BOOT
  Mailer::queueAlarm("Test invio mail", true);   // viene scartato se internet manca
#endif

  Serial.print("IP: "); Serial.println(Ethernet.localIP());

  wdt.start(WDT_TIMEOUT_MS);      // riavvio automatico se il loop si blocca
  wdtOn = true;
}

void loop() {
#if DEBUG_HEARTBEAT
  static uint32_t lastLoop = 0, hbT = 0, hbN = 0;
  uint32_t now = millis();
  if (lastLoop && now - lastLoop > 1000) {
    Serial.print("Loop lento (ms): "); Serial.println((int)(now - lastLoop));
  }
  lastLoop = now;
  hbN++;
  if (now - hbT >= 5000) {
    Serial.print("HB giri/5s: "); Serial.println((int)hbN);
    hbN = 0; hbT = now;
  }
#endif

  wdtKick();

  Plant::update();
  Leds::update();
  WebUi::handle();
  Mailer::process();
  EventLog::flush();
  Stats::flush();

  // Mantiene il DHCP (se usato) e risincronizza l'ora periodicamente
  Ethernet.maintain();
  if (millis() - lastNtp > (netOnline() ? NET_CHECK_MS : NET_RETRY_MS)) {
    if (clockNtpSync()) Leds::ntpFound();
    lastNtp = millis();
  }
}
