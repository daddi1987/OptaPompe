#pragma once
#include <Arduino.h>

// LED di stato dell'Opta (assegnazione dei pin in config.h)
//
//  LED_PIN_NTP    (LED_D0)  acceso se internet/NTP raggiungibile; se manca internet lampeggia
//                           (solo se la scheda non ha il LED blu LED_USER, altrimenti lampeggia quello)
//  LED_PIN_WEB    (LED_D1)  acceso a fine setup, quando la pagina web e' attiva
//  LED_PIN_PUMP1  (LED_D2)  lampeggia quando la pompa 1 e' in marcia, spento altrimenti
//  LED_PIN_PUMP2  (LED_D3)  lampeggia quando la pompa 2 e' in marcia, spento altrimenti
//
//  LED sotto il tasto RESET (dopo il setup):
//    verde fisso               tutto regolare
//    rosso lampeggiante        almeno un allarme attivo
//    giallo lampeggiante       mancanza rete sulla linea pompe (ha la precedenza sul rosso)
//
//  All'accensione tutti i LED sono spenti.
namespace Leds {
  void begin();        // spegne tutto e configura i pin
  void ntpFound();     // ora sincronizzata
  void setupDone();    // setup finito e pagina web attiva
  void update();       // da chiamare nel loop
}
