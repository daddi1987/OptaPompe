#pragma once
// Rinnova il watchdog. Da chiamare dentro ogni attesa lunga (rete, NTP, SMTP).
// Non fa nulla finche' il watchdog non e' stato avviato in setup().
void wdtKick();
