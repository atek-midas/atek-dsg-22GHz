#pragma once
#include <Arduino.h>

// -----------------------------------------------------------------------------
// Remote Control Interface
// Defines the public API and SCPI command table structures used by the remote
// command parser. This header only contains declarations and type definitions.
// -----------------------------------------------------------------------------

// --- Generic output callback type (optional) ---
typedef void (*rc_write_fn)(const char* s);

// --- SCPI table types (available for external use if needed) ---
typedef void (*scpi_handler_t)(char* args, int is_query);
typedef struct {
  const char*    path;     // "FREQ", "POW:ATT", "*IDN", ...
  scpi_handler_t handler;
} scpi_cmd_t;

// --- Public interface API ---
void RC_Begin(void);                         // resets the line buffer
void RC_SetWrite(rc_write_fn fn);            // sets the output target; uses Serial if not provided
void RC_ProcessByte(uint8_t b);              // processes the line when '\r' or '\n' is received
void RC_HandleLine(char* line);              // directly processes a complete input line

// Keep visible if needed: parse + dispatch; normally called through RC_HandleLine
void RC_ParseAndExecute(char* cmd_uc, char* args, int is_query);
