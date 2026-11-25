#include "Leds.h"
#include "config.h"
#include "Plant.h"
#include "Clock.h"

enum { I_NTP, I_WEB, I_PUMP1, I_PUMP2, I_GREEN, I_RED,
#ifdef LED_PIN_NET
       I_NET,
#endif
       I_COUNT };
static const uint8_t pins[I_COUNT] = { LED_PIN_NTP, LED_PIN_WEB, LED_PIN_PUMP1, LED_PIN_PUMP2,
                                       PIN_LED_GREEN, PIN_LED_RED
#ifdef LED_PIN_NET
                                       , LED_PIN_NET
#endif
                                       };
static int8_t cache[I_COUNT];              // ultimo livello scritto (-1 = mai scritto)
static bool   ntpIsOk = false, setupIsDone = false;

static void put(uint8_t i, bool on) {
  if (cache[i] == (int8_t)on) return;      // scrive solo quando cambia
  cache[i] = on;
  digitalWrite(pins[i], on ? LED_LEVEL_ON : (LED_LEVEL_ON == HIGH ? LOW : HIGH));
}

namespace Leds {

void begin() {
  for (uint8_t i = 0; i < I_COUNT; i++) { pinMode(pins[i], OUTPUT); cache[i] = -1; }
  ntpIsOk = false; setupIsDone = false;
  update();                                // scrive "spento" su tutti
}

void ntpFound()  { ntpIsOk = true; update(); }

void setupDone() { setupIsDone = true; update(); }

void update() {
  bool blink = (millis() / LED_BLINK_MS) % 2 == 0;

  // Internet: acceso fisso se raggiungibile, lampeggio se assente (dopo la prima prova)
  bool offline = netChecked() && !netOnline();
#ifdef LED_PIN_NET
  put(I_NTP, netOnline());
  put(I_NET, offline && blink);               // LED blu lampeggiante
#else
  put(I_NTP, netOnline() || (offline && blink));
#endif
  put(I_WEB, setupIsDone);

  bool p1 = false, p2 = false;
  bool green = false, red = false;
  if (setupIsDone) {
    const PlantState &s = Plant::state();
    p1 = s.pumpRun[0] && blink;                    // pompa 1 in marcia: lampeggia
    p2 = s.pumpRun[1] && blink;                    // pompa 2 in marcia: lampeggia

    bool any = false;
    for (uint8_t i = 0; i < AL_COUNT; i++) if (Plant::alarm((AlarmId)i).active) any = true;
    if (Plant::alarm(AL_MAINS).active) { green = blink; red = blink; }   // giallo lampeggiante
    else if (any)                      { red = blink; }                  // rosso lampeggiante
    else                               { green = true; }                 // verde fisso
  }
  put(I_PUMP1, p1);
  put(I_PUMP2, p2);
  put(I_GREEN, green);
  put(I_RED, red);
}

}  // namespace Leds
