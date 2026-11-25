#pragma once
#include <Arduino.h>
#include <time.h>

bool     clockValid();                     // true se l'ora e' stata sincronizzata
uint32_t clockNowUtc();                    // epoch UTC (0 se non valida)
bool     clockNtpSync();                   // sincronizza via NTP, aggiorna l'RTC (serve anche da prova internet)
bool     netOnline();                      // esito dell'ultima prova NTP: true = internet raggiungibile
bool     netChecked();                      // true dopo la prima prova (riuscita o no)
uint32_t netCheckAgeMs();                  // ms dall'ultima prova
uint32_t clockLocalDay(uint32_t utc);      // numero del giorno (da 1/1/1970) in ora italiana
void     clockFormatLocal(uint32_t utc, char *buf, size_t len);  // "gg/mm/aaaa hh:mm:ss" ora italiana
