#pragma once
#include <Arduino.h>

// --- Genel yazıcı callback'i (opsiyonel) ---
typedef void (*rc_write_fn)(const char* s);

// --- SCPI tablo tipleri (dışarıdan gerekirse kullanılabilsin) ---
typedef void (*scpi_handler_t)(char* args, int is_query);
typedef struct {
  const char*    path;     // "FREQ", "POW:ATT", "*IDN", ...
  scpi_handler_t handler;
} scpi_cmd_t;

// --- Dış arayüz (kamu API) ---
void RC_Begin(void);                         // satır tamponunu sıfırlar
void RC_SetWrite(rc_write_fn fn);            // çıktı hedefini set et (yoksa Serial)
void RC_ProcessByte(uint8_t b);              // '\r' veya '\n' gelince satırı işler
void RC_HandleLine(char* line);              // elindeki tam satırı doğrudan işle

// İstersen görünür tut: parse + dispatch (çağıran normalde RC_HandleLine)
void RC_ParseAndExecute(char* cmd_uc, char* args, int is_query);
