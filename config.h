#pragma once
#include <Arduino.h>

// ============================================================
//  CONFIGURAZIONE - tutto quello che cambia da impianto a impianto
// ============================================================

// ---- Ingressi (Opta: I1..I8 = A0..A7) --------------------------------------
// Tipo di contatto di ciascun ingresso, riferito all'EVENTO che si vuole rilevare:
//   CONTACT_NO = a riposo nessun segnale; l'evento porta il segnale (24V = HIGH)
//   CONTACT_NC = a riposo il segnale c'e'; l'evento lo toglie (anche filo tagliato = evento, fail-safe)
// Per cambiare il fronte del segnale basta invertire NO <-> NC qui sotto.
//  I1/I2  Pompa 1/2 in marcia        evento = pompa in marcia
//  I3/I4  Termica pompa 1/2          evento = termica scattata
//  I5/I6  Galleggiante troppo pieno  evento = livello troppo pieno raggiunto
//  I7     Rete linea pompe           evento = rete assente
//  I8     libero
#define CONTACT_NO  0
#define CONTACT_NC  1
#define CONTACT_PUMP1_RUN  CONTACT_NO
#define CONTACT_PUMP2_RUN  CONTACT_NO
#define CONTACT_THERMAL1   CONTACT_NC
#define CONTACT_THERMAL2   CONTACT_NC
#define CONTACT_OVERFILL1  CONTACT_NO
#define CONTACT_OVERFILL2  CONTACT_NO
#define CONTACT_MAINS      CONTACT_NC
#define PIN_PUMP1_RUN    A0   // I1
#define PIN_PUMP2_RUN    A1   // I2
#define PIN_THERMAL1_OK  A2   // I3
#define PIN_THERMAL2_OK  A3   // I4
#define PIN_OVERFILL1    A4   // I5
#define PIN_OVERFILL2    A5   // I6
#define PIN_MAINS_OK     A6   // I7

// ---- Tempi (ms) -------------------------------------------------------------
#define DEBOUNCE_MS          500UL      // antirimbalzo ingressi digitali
#define MAINS_DELAY_MS      2000UL      // ritardo allarme mancanza rete
#define THERMAL_DELAY_MS    1000UL      // ritardo allarme termica
#define OVERFILL_DELAY_MS   2000UL      // ritardo allarme troppo pieno

// ---- Watchdog e diagnostica -------------------------------------------------
#define WDT_TIMEOUT_MS   25000UL   // riavvio automatico se il loop si blocca (max ~32 s)
#define DEBUG_HEARTBEAT  0         // 1 = stampa ogni 5 s i giri del loop e segnala i loop lenti
#define MAIL_DEBUG       1         // stampa sulla seriale i passi dell'invio e-mail

// ---- LED di stato dell'Opta ---------------------------------------------------
// LED_D0..LED_D3 (solo verdi). Assegnazione modificabile qui sotto.
// LED sotto il tasto RESET: verde (LED_BUILTIN) e rosso (LEDR); insieme danno il giallo.
// Livello per accendere un LED: HIGH, come negli esempi Finder/Arduino.
#define LED_LEVEL_ON       HIGH
#define LED_PIN_NTP        LED_D0     // acceso solo quando l'ora e' sincronizzata via NTP
#define LED_PIN_WEB        LED_D1     // acceso a fine setup, con la pagina web attiva
#define LED_PIN_PUMP1      LED_D2     // lampeggia con la pompa 1 in marcia
#define LED_PIN_PUMP2      LED_D3     // lampeggia con la pompa 2 in marcia
#define PIN_LED_GREEN      LED_BUILTIN
#define PIN_LED_RED        LEDR
#define LED_BLINK_MS       500UL      // mezzo periodo del lampeggio
// Internet assente: lampeggia il LED blu (LED_USER, presente solo nella variante Opta WiFi).
// Sulla variante senza LED blu lampeggia LED_PIN_NTP (LED_D0): acceso fisso = internet ok.
#ifdef LED_USER
  #define LED_PIN_NET      LED_USER
#endif

// ---- Rete -------------------------------------------------------------------
#define NET_USE_DHCP   false
#define NET_IP         IPAddress(192, 168, 1, 50)
#define NET_DNS        IPAddress(192, 168, 1, 1)
#define NET_GATEWAY    IPAddress(192, 168, 1, 1)
#define NET_SUBNET     IPAddress(255, 255, 255, 0)

#define NTP_SERVER        "pool.ntp.org"   // meglio un NTP interno del cliente
#define NET_CHECK_MS      (10UL * 60UL * 1000UL)   // con internet: riprova NTP ogni 10 minuti (rileva anche la caduta)
#define NET_RETRY_MS      30000UL                  // senza internet: riprova NTP ogni 30 s
#define MAIL_PROBE_MS     30000UL                  // prima di una mail, se l'ultima prova e' piu' vecchia di cosi', ripete la prova

// ---- Web UI -----------------------------------------------------------------
#define WEB_PORT 80
// Attesa massima della richiesta dal browser (le connessioni vuote bloccano il loop)
#define WEB_REQ_TIMEOUT_MS 250UL
// Basic Auth: base64("utente:password"). Questo e' "admin:admin": DA CAMBIARE.
// Per generarlo:  echo -n 'utente:password' | base64
#define WEB_AUTH_B64 "YWRtaW46YWRtaW4="

// ---- E-mail (Gmail, SMTP su TLS diretto, libreria ESP_SSLClient) ---------------
// Indirizzo Gmail e password per app stanno in secrets.h.
// Libreria da installare (Gestione librerie): "ESP_SSLClient" di Mobizt.
// DNS e gateway di NET_DNS / NET_GATEWAY devono permettere di raggiungere internet.
#define SMTP_HOST      "smtp.gmail.com"
#define SMTP_PORT      465
#define MAIL_FROM      SMTP_USER                   // Gmail accetta solo il proprio indirizzo
#define MAIL_TO        "davide.zuanon@outlook.it"   // piu' destinatari separati da virgola
#define MAIL_HELO      "opta-pompe"
#define MAIL_RETRY_MS  60000UL

// Metti 1 per attivare le mail. Con 0 nulla viene accodato, inviato ne' compilato
// (la libreria ESP_SSLClient non viene nemmeno inclusa: compilazione piu' rapida).
#define MAIL_ENABLED       1
// Pausa minima tra due mail consecutive: lascia girare il loop (ingressi e pagina web)
#define MAIL_GAP_MS        3000UL
// 1 = accoda una mail di prova a ogni avvio (solo per il collaudo iniziale)
#define MAIL_TEST_AT_BOOT  0

// 1 = TLS senza verifica del certificato (come nella prova che ha funzionato).
// 0 = verifica con la radice di Google: crea un file certs.h con
//       #define MAIL_ROOT_CA R"CERT(-----BEGIN CERTIFICATE----- ... -----END CERTIFICATE-----)CERT"
//     (GTS Root R1, dal sito Google Trust Services). Richiede l'ora NTP valida.
//     NOTA: questa modalita' non e' stata provata.
#define MAIL_TLS_INSECURE  1

// ---- Storico ----------------------------------------------------------------
#define LOG_SIZE 128          // eventi in buffer circolare (persistenti in flash)

// Statistiche: giorni con almeno un'anomalia conservati in flash (buffer ciclico).
// Si contano le ATTIVAZIONI degli allarmi per giorno (ora italiana).
#define STATS_DAYS 90

// Azzeramento dello storico (eventi E statistiche giornaliere) all'avvio: per cancellarlo cambia questo numero
// (1 -> 2 -> 3 ...) e ricarica lo sketch. Viene cancellato UNA sola volta: con lo
// stesso numero i riavvii successivi (anche quelli del watchdog) conservano lo storico.
// L'azzeramento e' tracciato da una riga "Storico azzerato" con data e ora.
#define LOG_RESET_ID 2
