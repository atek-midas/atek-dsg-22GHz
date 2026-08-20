#include "RemoteControl.h"
#include "Lmx2820.h"
#include "MCP23S17.h"
#include "ADC78H90.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "display.h" 

// ============================================================================
// DSG 22.6 GHz - Professional SCPI Command Set and USB-CDC Communication Infrastructure
// ============================================================================
// SCPI-like Command Usage Examples (send over USB-CDC; responses shown after ->)
// ----------------------------------------------------------------------------
// *IDN?                     -> ATEK,DSG-22.6GHz,r1.0
// *RST                      -> 0                      (reset to safe defaults)
//                                                     (freq=1GHz, att=0, lo=0, filt=OFF, outp=OFF)
//
// :FREQ 1.234GHz            -> 0                      (set PLL frequency; returns 0 if accepted)
// :FREQ?                    -> 1234000000             (returns frequency in Hz)
//
// :POW:ATT 12               -> 0                      (set global attenuator 0..31 dB)
// :POW:ATT?                 -> 12                     (returns last set attenuation)
//
// :POW:OUTA 5               -> 0                      (set OUTA power 0..7)
// :POW:OUTA?                -> 5                      (returns last set LO power)
//
// :FILT ON                  -> 1                      (enable filter; returns 1 if ON)
// :FILT OFF                 -> 0                      (disable filter; returns 0 if OFF)
// :FILT?                    -> 1 or 0                 (returns filter state)
//
// :OUTP ON                  -> 1                      (enable RF output path)
// :OUTP OFF                 -> 0                      (disable RF output path)
// :OUTP?                    -> 1 or 0                 (returns RF output state)
//
// :ADC:READ 4               -> 41.500000              (float reading from channel 4)
// ============================================================================

// --- EXTERNAL LINKS FOR DISPLAY (display.cpp) VARIABLES AND FUNCTIONS ---
extern String StartValueForSweepMenu;
extern String StartUnitForSweepMenu;
extern String StopValueForSweepMenu;
extern String StopUnitForSweepMenu;
extern String StepValueForSweepMenu;
extern String StepUnitForSweepMenu;
extern String DwellValueForSweepMenu;
extern String AmpValueSweepForSweepMenu;
extern String StepTypeValueForSweepMenu;
extern String CountValueForSweepMenu;
extern uint32_t sweepCycleCount;
extern void SetTypeOnSweepMenu(String type);
extern void SetAmpAmpOnSweepMenu(String value);


extern bool isSweepRunning;
extern double currentHz; // To restart the sweep from the beginning
extern MenuState currentMenu;

extern void drawSweepMenu();
extern void drawMainMenu(); // Function used to redraw the CW screen
extern void SetAmpUnitOnMainMenu();
extern void SetAmpOnMainMenu(String value);
extern void SetRfOnOff(bool value); // To safely turn the hardware on/off

// Function links that update only the UI boxes without straining the screen:
extern void SetStartFreqOnSweepMenu(String value);
extern void SetStopFreqOnSweepMenu(String value);
extern void SetStepFreqOnSweepMenu(String value);
extern void SetDwellFreqOnSweepMenu(String value);
extern void SetAmpAmpOnSweepMenu(String value);

// --- EXTERNAL DEFINITIONS REQUIRED FOR CALIBRATION AND POWER CONTROL ---
struct CalibData {
    uint16_t freq_MHz;
    int8_t att6_on;
    int8_t att3_on;
    int8_t att_n3_on;
    int8_t att6_off;
    int8_t att3_off;
    int8_t att_n3_off;
};

extern CalibData calibTable[];
extern uint16_t calibCount;
extern void ClearCalibrationRAM();
extern void SaveCalibrationToNVS();
extern bool AddCalibrationPoint(uint16_t f, int8_t a1, int8_t a2, int8_t a3, int8_t a4, int8_t a5, int8_t a6);

// Access to the device's own display-state variables
extern String AmpValueForMainMenu;
extern String enteredAmpValue;
extern String currentAmplitude;

// Frequency tracking to prevent a 0 MHz fallback condition
static double g_last_freq_hz = 1000e6;



static void h_idn (char*, int);    
static void h_rst (char*, int);
static void h_freq(char*, int);
static void h_pow_att(char*, int);
static void h_outa_pwr(char*, int);
static void h_outb_pwr(char*, int);
static void h_out_a_and_b_pwr(char*, int);
static void h_filt(char*, int);
static void h_outp(char*, int);
static void h_pll_write(char*, int);
static void h_pll_read (char*, int);
static void h_io_write (char*, int);
static void h_io_read  (char*, int);
static void h_adc_read (char*, int);
static void h_pow_lev(char*, int);
static void h_cal_clear(char*, int);
static void h_cal_data(char*, int);
static void h_cal_save(char*, int);
static void h_sync(char*, int);

// --- DISPLAY CONTROL COMMAND ---
static void h_disp_menu(char*, int);

// --- SWEEP COMMANDS ---
static void h_sweep_start(char*, int);
static void h_sweep_stop(char*, int);
static void h_sweep_step(char*, int);
static void h_sweep_dwell(char*, int);
static void h_sweep_count(char*, int);
static void h_sweep_pow(char*, int);
static void h_sweep_type(char*, int);
static void h_sweep_init(char*, int);
static void h_sweep_abort(char*, int);

static int parse_onoff(const char *s, int *ok);
static void rc_write(const char *s);
static void rc_writeln(const char *s);

// ====== SCPI command signature and table ======
typedef void (*scpi_handler_t)(char *args, int is_query);

// === Command Table ===
static const scpi_cmd_t scpi_table[] = {
  { "*IDN",        h_idn       },
  { "*RST",        h_rst       },
  { "FREQ",        h_freq      },
  { "POW:ATT",     h_pow_att   },
  { "POW:OUTA",    h_outa_pwr  },
  { "POW:OUTB",    h_outb_pwr  },
  { "POW:LO",      h_out_a_and_b_pwr  },
  { "FILT",        h_filt      },
  { "OUTP",        h_outp      },
  { "PLL:WRITE",   h_pll_write },
  { "PLL:READ",    h_pll_read  },
  { "IOEXP:WRITE", h_io_write  },
  { "IOEXP:READ",  h_io_read   },
  { "ADC:READ",    h_adc_read  },
  { "GET:TEMP",    h_adc_read  },
  { "GET:CURRENT", h_adc_read  },
  { "GET:VOLTAGE", h_adc_read  },
  { "GET:LD",      h_adc_read  },
  { "DISP:MENU",   h_disp_menu },
  { "SWEEP:STAR",  h_sweep_start },
  { "SWEEP:STOP",  h_sweep_stop  },
  { "SWEEP:STEP",  h_sweep_step  },
  { "SWEEP:DWEL",  h_sweep_dwell },
  { "SWEEP:COUN",  h_sweep_count },
  { "SWEEP:POW",   h_sweep_pow   },
  { "SWEEP:TYPE",  h_sweep_type  },
  { "SWEEP:INIT",  h_sweep_init  },
  { "SWEEP:ABOR",  h_sweep_abort },
  
  // --- CUSTOM POWER AND CALIBRATION COMMANDS (WITHOUT A LEADING ':') ---
  { "POW:LEV",       h_pow_lev     },
  { "SWEEP:POW:LEV", h_pow_lev     }, // Route the Sweep screen power setting to the same power-control handler
  { "CAL:CLEAR",     h_cal_clear   },
  { "CAL:DATA",      h_cal_data    },
  { "CAL:SAVE",      h_cal_save    },
  { "SYNC",          h_sync        }  // Synchronization command
};

static const size_t scpi_count = sizeof(scpi_table)/sizeof(scpi_table[0]);

// ====== Dispatcher ======
void RC_ParseAndExecute(char *cmd_uc, char *args, int is_query) {
  if (cmd_uc[0]==':') cmd_uc++;

  size_t L = strlen(cmd_uc);
  if (L && cmd_uc[L-1]=='?') { cmd_uc[L-1]=0; is_query = 1; }

  for (size_t i=0; i<scpi_count; i++) {
    if (!strcmp(cmd_uc, scpi_table[i].path)) {
      scpi_table[i].handler(args, is_query); 
      return;
    }
  }
  rc_write("-100,Unknown command: ");
  rc_writeln(cmd_uc);
}

// ====== Write helper ======
static rc_write_fn g_writer = nullptr;
static void rc_write(const char *s) {
  if (g_writer) { g_writer(s); }
  else          { Serial.print(s); }
}

// ====== Simple helpers ======
static void normalize_arg(char *s) {
  if (!s) return;
  char *d = s;
  for (char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c <= 32) continue;
    *d++ = *p;
  }
  *d = '\0';
}

static void rc_writeln(const char *s) { rc_write(s); rc_write("\r\n"); }

static void upper_trim(char *s) {
  size_t n = strlen(s);
  size_t i = 0; while (i<n && (s[i]==' '||s[i]=='\t')) i++;
  if (i>0) memmove(s, s+i, n-i+1), n -= i;
  while (n>0 && (s[n-1]==' '||s[n-1]=='\t')) s[--n] = 0;
  for (size_t k=0; k<n; ++k) s[k] = (char)toupper((unsigned char)s[k]);
}

// Converts the Hz value to the MHz string format expected by the device UI
static String format_mhz(double hz) {
    String s = String(hz / 1e6, 6); 
    while(s.length() > 0 && s.endsWith("0")) s.remove(s.length()-1);
    if(s.length() > 0 && s.endsWith(".")) s.remove(s.length()-1);
    if(s.length() == 0) s = "0";
    return s;
}

static int g_filter_cached = -1;  // -1: unknown, 0/1: OFF/ON

static void h_filt(char *args, int q) {
  if (q) {
    if (g_filter_cached >= 0) {
      rc_writeln(g_filter_cached ? "1" : "0");
    } else {
      rc_writeln("-303,No cached value");
    }
    return;
  }

  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
  int ok = 0;
  int on = parse_onoff(args, &ok);   
  if (!ok) { rc_writeln("-104,Data type error"); return; }

  ApplyFilter(on != 0);
  g_filter_cached = on ? 1 : 0;
  // --- Recalculate the attenuator whenever the filter state changes ---
  char cmdBuf[32];
  sprintf(cmdBuf, "POW:LEV %s", AmpValueForMainMenu.c_str());
  RC_HandleLine(cmdBuf);
  // --------------------------------------------------------------------------
  rc_writeln(on ? "1" : "0");
}

static double parse_freq_to_hz(const char *token, int *ok) {
    *ok = 0;
    if (!token || !*token) return 0.0;

    char buf[64]; memset(buf, 0, sizeof(buf));
    strncpy(buf, token, sizeof(buf)-1);

    size_t len = strlen(buf);
    while (len && (isspace((unsigned char)buf[len-1]) || buf[len-1]=='\r' || buf[len-1]=='\n'))
        buf[--len] = 0;
    while (*buf && isspace((unsigned char)*buf)) {
        memmove(buf, buf+1, strlen(buf));
        len--;
    }

    for (size_t i=0; i<len; i++)
        buf[i] = (char)toupper((unsigned char)buf[i]);

    double mult = 1.0;
    if (len > 2 && strstr(buf, "GHZ") == buf+len-3) {
        mult = 1e9; buf[len-3] = 0;
    }
    else if (len > 2 && strstr(buf, "MHZ") == buf+len-3) {
        mult = 1e6; buf[len-3] = 0;
    }
    else if (len > 2 && strstr(buf, "KHZ") == buf+len-3) {
        mult = 1e3; buf[len-3] = 0;
    }
    else if (len > 1 && buf[len-1]=='G') {
        mult = 1e9; buf[len-1] = 0;
    }
    else if (len > 1 && buf[len-1]=='M') {
        mult = 1e6; buf[len-1] = 0;
    }
    else if (len > 1 && buf[len-1]=='K') {
        mult = 1e3; buf[len-1] = 0;
    }
    else if (len > 1 && strstr(buf, "HZ") == buf+len-2) {
        mult = 1.0; buf[len-2] = 0;
    }

    char *endp = nullptr;
    double val = strtod(buf, &endp);
    if (!endp || *endp!=0) return 0.0;

    *ok = 1;
    return val * mult;
}

static int parse_onoff(const char *s, int *ok) {
  *ok = 1;
  if (!s) { *ok=0; return 0; }
  char u[16]={0};
  for (int i=0; s[i] && i<15; ++i) u[i] = (char)toupper((unsigned char)s[i]);
  if (!strcmp(u,"ON") || !strcmp(u,"1"))  return 1;
  if (!strcmp(u,"OFF")|| !strcmp(u,"0"))  return 0;
  *ok=0; return 0;
}

static int split_params(char *args, char *outv[], int maxv) {
  int c = 0;
  char *p = args;
  while (p && *p && c<maxv) {
    while (*p==' '||*p=='\t') p++;
    if (!*p) break;
    outv[c++] = p;
    normalize_arg(outv[c-1]); 
    char *comma = strchr(p, ',');
    if (!comma) break;
    *comma = 0;
    p = comma + 1;
  }
  return c;
}

// === Handlers ===
static void h_idn (char *args, int q) {
  (void)args; (void)q;
  rc_writeln("ATEK,DSG-22.6GHz,r1.0");
}

static void h_rst (char *args, int q) {
  (void)args; (void)q;
  rc_writeln("Not Implemented yet!");
}

static int g_outa_pwr_cached = -1;  
static void h_outa_pwr(char *args, int q) {
  if (q) {
    if (g_outa_pwr_cached >= 0) {
      char out[8]; snprintf(out, sizeof(out), "%d", g_outa_pwr_cached); rc_writeln(out);
    } else { rc_writeln("-303,No cached value"); }
    return;
  }
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
  while (*args==' ' || *args=='\t') ++args;
  char *endp = nullptr;
  long v = strtol(args, &endp, 10);
  if (endp && *endp!=0) { rc_writeln("-104,Data type error"); return; }
  if (v < 0 || v > 7)   { rc_writeln("-222,Data out of range"); return; }

  Lmx2820SetOUTA_PWR((uint8_t)v);
  g_outa_pwr_cached = (int)v;
  rc_writeln("0");
}

static int g_outb_pwr_cached = -1;  
static void h_outb_pwr(char *args, int q) {
  if (q) {
    if (g_outb_pwr_cached >= 0) {
      char out[8]; snprintf(out, sizeof(out), "%d", g_outb_pwr_cached); rc_writeln(out);
    } else { rc_writeln("-303,No cached value"); }
    return;
  }
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
  while (*args==' ' || *args=='\t') ++args;
  char *endp = nullptr;
  long v = strtol(args, &endp, 10);
  if (endp && *endp!=0) { rc_writeln("-104,Data type error"); return; }
  if (v < 0 || v > 7)   { rc_writeln("-222,Data out of range"); return; }

  Lmx2820SetOUTB_PWR((uint8_t)v);
  g_outb_pwr_cached = (int)v;
  rc_writeln("0");
}

static void h_out_a_and_b_pwr(char *args, int q) {
  h_outa_pwr(args, q); 
  h_outb_pwr(args, q);
}

static void h_freq(char *args, int q) {
  if (q) { rc_writeln("0"); return; }
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }

  int ok=0; 
  double hz = parse_freq_to_hz(args, &ok);
  if (!ok || hz<=0)    { rc_writeln("-104,Data type error"); return; }

  double fMHz = hz / 1e6;     
  SetFilterBand(fMHz);
  ApplyFrequency(hz);
  
  // Store the last valid frequency to keep the active RF state consistent
  g_last_freq_hz = hz; 
  
  rc_writeln("0");
}

static int g_att_cached = -1;  
static void h_pow_att(char *args, int q) {
  if (q) {
    if (g_att_cached >= 0) {
      char out[8]; snprintf(out, sizeof(out), "%d", g_att_cached); rc_writeln(out);
    } else { rc_writeln("-303,No cached value"); }
    return;
  }
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
  while (*args==' ' || *args=='\t') ++args;

  char *endp = nullptr;
  long v = strtol(args, &endp, 10);
  if (endp && *endp!=0) { rc_writeln("-104,Data type error"); return; }
  if (v < 0 || v > 31)  { rc_writeln("-222,Data out of range"); return; }

  SetAttenuator((uint8_t)v);
  g_att_cached = (int)v;
  rc_writeln("0");
}

// --- SYNCHRONIZED LINEAR POWER-CONTROL ENGINE ---
static void h_pow_lev(char *args, int q) {
  if (q) { rc_writeln("0"); return; }
  float targetDBm = atof(args);

  // 1. Select the LO power level and matching calibration reference column
  uint8_t selectedLO;
  float refPower;

  if (targetDBm >= 6.0) {
      selectedLO = 7;      
      refPower = 6.0;      
  } 
  else if (targetDBm >= -2.0) {
      selectedLO = 2;      
      refPower = 3.0;      
  } 
  else {
      selectedLO = 0;      
      refPower = -3.0;     
  }

  // Configure both LMX chip outputs (OUTA/OUTB)
  extern void Lmx2820SetOUTA_PWR(uint8_t OUTA_PWR_i);
  extern void Lmx2820SetOUTB_PWR(uint8_t OUTB_PWR_i);
  Lmx2820SetOUTA_PWR(selectedLO);
  Lmx2820SetOUTB_PWR(selectedLO);

  // 2. Mathematical interpolation using the active frequency shown on the display
  extern String FreqValueForMainMenu;
  extern String FreqUnitForMainMenu;
  
  double active_freq_hz = FreqValueForMainMenu.toDouble();
  if (FreqUnitForMainMenu == "GHz") active_freq_hz *= 1e9;
  else if (FreqUnitForMainMenu == "MHz") active_freq_hz *= 1e6;
  else if (FreqUnitForMainMenu == "KHz") active_freq_hz *= 1e3;
  
  uint16_t freqMHz = (uint16_t)(active_freq_hz / 1e6);

  extern bool FilterStatus;
  
  // Keep these variables outside the conditional scope to avoid scope-related errors
  float finalAtt = 31.0; 
  float baseAtt = 31.0;
  float powerDiff = 0.0;
  
  if (calibCount > 0) {
    int idx_low = 0;
    int idx_high = calibCount - 1;
    
    // Locate the two calibration rows that bound the active frequency
    for(int i = 0; i < calibCount - 1; i++) {
        if(freqMHz >= calibTable[i].freq_MHz && freqMHz <= calibTable[i+1].freq_MHz) {
            idx_low = i;
            idx_high = i + 1;
            break;
        }
    }

    // Select the calibration column that matches the selected reference power
    auto getAtt = [&](int idx) -> float {
        float val = 31.0;
        if(refPower == 6.0) val = FilterStatus ? calibTable[idx].att6_on : calibTable[idx].att6_off;
        else if(refPower == 3.0) val = FilterStatus ? calibTable[idx].att3_on : calibTable[idx].att3_off;
        else val = FilterStatus ? calibTable[idx].att_n3_on : calibTable[idx].att_n3_off;
        return (val < 0) ? 31.0 : val; // Invalid-data protection
    };

    float att_low = getAtt(idx_low);
    float att_high = getAtt(idx_high);

    // Interpolate the base attenuation between the two frequency points
    baseAtt = att_low; 
    if (calibTable[idx_high].freq_MHz != calibTable[idx_low].freq_MHz) {
        float ratio = (float)(freqMHz - calibTable[idx_low].freq_MHz) / (calibTable[idx_high].freq_MHz - calibTable[idx_low].freq_MHz);
        baseAtt = att_low + ratio * (att_high - att_low);
    }

    // Offset the interpolated attenuation by the difference between target and reference power
    powerDiff = targetDBm - refPower; 
    finalAtt = baseAtt - powerDiff;
  }

  // Clamp the attenuation value to the supported hardware range
  if (finalAtt < 0.0) finalAtt = 0.0;
  if (finalAtt > 31.5) finalAtt = 31.5;

  // --- Quantize attenuation by removing the fractional part ---
  finalAtt = (int)finalAtt;

  // 3. Apply the value to the hardware and update the display state
  SetAttenuator((uint8_t)finalAtt); // Apply the raw physical attenuation value to the hardware

  // Store and display the requested target power value in dBm
  String targetStr = String(targetDBm, 1);
  AmpValueForMainMenu = targetStr;
  enteredAmpValue = targetStr;
  currentAmplitude = targetStr;

  if (currentMenu == MAIN_MENU) {
      SetAmpUnitOnMainMenu();
      SetAmpOnMainMenu(targetStr);
  }

  rc_writeln("0");
}

// --- CALIBRATION MANAGEMENT COMMANDS ---
static void h_cal_clear(char *args, int q) { ClearCalibrationRAM(); rc_writeln("0"); }
static void h_cal_save(char *args, int q) { SaveCalibrationToNVS(); rc_writeln("0"); }
static void h_cal_data(char *args, int q) {
  int f, a1, a2, a3, a4, a5, a6;
  if(sscanf(args, "%d,%d,%d,%d,%d,%d,%d", &f, &a1, &a2, &a3, &a4, &a5, &a6) == 7) {
    if(AddCalibrationPoint(f, a1, a2, a3, a4, a5, a6)) rc_writeln("0");
    else rc_writeln("-223,RAM Full");
  } else {
    rc_writeln("-109,Format Error");
  }
}

static void h_outp(char *args, int q) {
  if (q) { rc_writeln("0"); return; }
  if (!args || !*args){ rc_writeln("-109,Missing parameter"); return; }
  int ok=0; int on = parse_onoff(args, &ok);
  if (!ok)            { rc_writeln("-104,Data type error");   return; }
  
  SetRfOnOff(on ? true : false);
  rc_writeln(on ? "1" : "0");
}

static void h_pll_write(char *args, int q) {
  (void)q;
  char *v[3] = {0};
  int n = split_params(args, v, 3);
  if (n < 2) { rc_writeln("-109,Missing parameter"); return; }

  uint32_t addr;
  uint32_t data;

  if (n == 2) {
    addr = (uint32_t)strtoul(v[0], NULL, 0); 
    data = (uint32_t)strtoul(v[1], NULL, 16);
  } else {
    addr = (uint32_t)strtoul(v[1], NULL, 0);
    data = (uint32_t)strtoul(v[2], NULL, 16);
  }
  PLL_write(addr, data);  
  rc_writeln("0");
}

static void h_pll_read(char *args, int q) {
  (void)q;
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
  uint32_t addr = (uint32_t)strtoul(args, NULL, 0);
  uint16_t d = PLL_read(addr);

  char out[16]; snprintf(out, sizeof(out), "%04X", (unsigned)d);
  rc_writeln(out);
}

static void h_io_write(char *args, int q) {
  (void)q;
  char *v[3] = {0};
  int n = split_params(args, v, 3);
  if (n < 2) { rc_writeln("-109,Missing parameter"); return; }

  int exp; uint32_t addr; uint32_t data;
  if (n == 2) {
    exp = 1; 
    addr = (uint32_t)strtoul(v[0], NULL, 10);
    data = (uint32_t)strtoul(v[1], NULL, 10);
  } else {
    exp = atoi(v[0]);
    addr = (uint32_t)strtoul(v[1], NULL, 10);
    data = (uint32_t)strtoul(v[2], NULL, 10);
  }

  if (exp == 1) { IO_EXP1_write(addr, data); } 
  else { rc_writeln("-222,IOEXP out of range"); return; }
  rc_writeln("0");
}

static void h_io_read(char *args, int q) {
  (void)q;
  char *v[2] = {0};
  int n = split_params(args, v, 2);
  if (n < 1) { rc_writeln("-109,Missing parameter"); return; }

  int exp; uint32_t addr;
  if (n == 1) {
    exp = 1;         
    addr = (uint32_t)strtoul(v[0], NULL, 10);
  } else {
    exp = atoi(v[0]);
    addr = (uint32_t)strtoul(v[1], NULL, 10);
  }

  uint8_t d = 0;
  if (exp == 1) { d = IO_EXP1_read(addr); } 
  else { rc_writeln("-222,IOEXP out of range"); return; }

  char out[16]; snprintf(out, sizeof(out), "%u", (unsigned)d);
  rc_writeln(out);
}

static void h_adc_read(char *args, int q) {
  (void)q;
  if (!args || !*args){ rc_writeln("-109,Missing parameter"); return; }
  uint32_t ch = (uint32_t)strtoul(args, NULL, 10);
  float v = ADC_Read(ch);
  char out[48]; snprintf(out, sizeof(out), "%f", (double)v);
  rc_writeln(out);
}

// =======================================================
// NEWLY ADDED DISPLAY MENU TOGGLE FUNCTION
// =======================================================
static void h_disp_menu(char *args, int q) {
    if (q) {
        if (currentMenu == MAIN_MENU) rc_writeln("CW");
        else if (currentMenu == SWEEP_MENU) rc_writeln("SWP");
        else rc_writeln("UNKNOWN");
        return;
    }

    if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
    
    char u[16]={0};
    for (int i=0; args[i] && i<15; ++i) u[i] = (char)toupper((unsigned char)args[i]);
    
    if (strstr(u, "CW") || strstr(u, "MAIN")) {
        currentMenu = MAIN_MENU; 
        drawMainMenu(); // Clear and redraw the CW screen
        rc_writeln("0");
    } 
    else if (strstr(u, "SWP") || strstr(u, "SWEEP")) {
        currentMenu = SWEEP_MENU;
        drawSweepMenu(); // Physically redraw the Sweep screen
        rc_writeln("0");
    } 
    else {
        rc_writeln("-104,Data type error");
    }
}

// =======================================================
// SWEEP MENU NEW COMMAND HANDLERS (PYTHON COMPATIBLE)
// =======================================================

static void h_sweep_start(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    int ok=0; double hz = parse_freq_to_hz(args, &ok);
    if (!ok || hz<=0) { rc_writeln("-104,Data type error"); return; }
    StartValueForSweepMenu = format_mhz(hz);
    StartUnitForSweepMenu = "MHz";
    if (currentMenu == SWEEP_MENU) {
        SetStartFreqOnSweepMenu(StartValueForSweepMenu); 
    }
    rc_writeln("0");
}

static void h_sweep_stop(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    int ok=0; double hz = parse_freq_to_hz(args, &ok);
    if (!ok || hz<=0) { rc_writeln("-104,Data type error"); return; }
    StopValueForSweepMenu = format_mhz(hz);
    StopUnitForSweepMenu = "MHz";
    if (currentMenu == SWEEP_MENU) {
        SetStopFreqOnSweepMenu(StopValueForSweepMenu); 
    }
    rc_writeln("0");
}

static void h_sweep_step(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    int ok=0; double hz = parse_freq_to_hz(args, &ok);
    if (!ok || hz<=0) { rc_writeln("-104,Data type error"); return; }
    StepValueForSweepMenu = format_mhz(hz);
    StepUnitForSweepMenu = "MHz";
    if (currentMenu == SWEEP_MENU) {
        SetStepFreqOnSweepMenu(StepValueForSweepMenu); 
    }
    rc_writeln("0");
}

static void h_sweep_dwell(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
    char *endp = nullptr;
    double ms = strtod(args, &endp);
    if (!endp || *endp!=0) { rc_writeln("-104,Data type error"); return; }
    DwellValueForSweepMenu = String(ms);
    if (currentMenu == SWEEP_MENU) {
        SetDwellFreqOnSweepMenu(DwellValueForSweepMenu); 
    }
    rc_writeln("0");
}

// Sets how many full sweep cycles to run before stopping automatically.
// 0 (the default) means "run forever", matching the sweep's original
// (unlimited) behavior.
static void h_sweep_count(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
    char *endp = nullptr;
    long count = strtol(args, &endp, 10);
    if (!endp || *endp!=0 || count < 0) { rc_writeln("-104,Data type error"); return; }
    CountValueForSweepMenu = String(count);
    if (currentMenu == SWEEP_MENU) {
        SetSCountOnSweepMenu(CountValueForSweepMenu); 
    }
    rc_writeln("0");
}

static void h_sweep_pow(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
    
    char *endp = nullptr;
    double dbm = strtod(args, &endp); // Read fractional dBm values as floating-point input
    if (!endp || *endp!=0) { rc_writeln("-104,Data type error"); return; }
    
    AmpValueSweepForSweepMenu = String(dbm, 1); 
    
    // Immediately update the UI from the command received through the GUI
    if (currentMenu == SWEEP_MENU) {
        SetAmpAmpOnSweepMenu(AmpValueSweepForSweepMenu); 
    }
    rc_writeln("0");
}

static void h_sweep_type(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }
    char u[16]={0};
    for (int i=0; args[i] && i<15; ++i) u[i] = (char)toupper((unsigned char)args[i]);
    
    if (strstr(u, "LIN")) {
        StepTypeValueForSweepMenu = "Lin";
        SetTypeOnSweepMenu(StepTypeValueForSweepMenu); // UI update
        rc_writeln("0");
    } else if (strstr(u, "LOG")) {
        StepTypeValueForSweepMenu = "Log";
        SetTypeOnSweepMenu(StepTypeValueForSweepMenu); // UI update
        rc_writeln("0");
    } else {
        rc_writeln("-104,Data type error");
    }
}

static void h_sweep_init(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    currentHz = 0; // Reset to restart the sweep from the beginning
    sweepCycleCount = 0; // Reset cycle counter every time a sweep run is (re)started.
    isSweepRunning = true;
    SetRfOnOff(true); // Safe RF turn on (via display.cpp)
    drawSweepMenu();  // Automatically switch screen to Sweep UI (Must stay here)
    rc_writeln("0");
}

static void h_sweep_abort(char *args, int q) {
    if (q) { rc_writeln("0"); return; }
    isSweepRunning = false;
    SetRfOnOff(false); // Turn RF off immediately and switch to a safe state
    if (currentMenu == SWEEP_MENU) {
        drawSweepMenu(); // Refresh screen to update Play/Pause icon (Must stay here)
    }
    rc_writeln("0");
}

// ====== Line processing ======
void RC_HandleLine(char *line) {
  if (!line) return;

  // Copy the line and separate command and arguments
  char buf[256]; memset(buf, 0, sizeof(buf));
  strncpy(buf, line, sizeof(buf)-1);

  // Clear CR/LF
  for (size_t i=0; buf[i]; ++i) {
    if (buf[i]=='\r' || buf[i]=='\n') { buf[i]=0; break; }
  }
  if (!buf[0]) return;

  // Separate the command part (up to the first space) and the argument
  char *space = strchr(buf, ' ');
  char *cmd   = buf;
  char *args  = nullptr;
  if (space) { *space = 0; args = space+1; }
  if (args) normalize_arg(args);

  // UPPER/trim version of the command
  char cmd_uc[128]; memset(cmd_uc,0,sizeof(cmd_uc));
  strncpy(cmd_uc, cmd, sizeof(cmd_uc)-1);

  // keep the leading ':' but make the rest upper case
  if (cmd_uc[0]==':') {
    for (size_t i=1; cmd_uc[i]; ++i) cmd_uc[i] = (char)toupper((unsigned char)cmd_uc[i]);
  } else {
    upper_trim(cmd_uc);
  }

  RC_ParseAndExecute(cmd_uc, args, 0);
}

// ====== Line collector ======
static char g_line[256];
static size_t g_len = 0;

void RC_Begin(void) {
  g_len = 0;
  g_line[0] = 0;
}

void RC_SetWrite(rc_write_fn fn) {
  g_writer = fn;
}

void RC_ProcessByte(uint8_t b) {
  if (b=='\r' || b=='\n') {
    if (g_len>0) {
      g_line[g_len] = 0;
      RC_HandleLine(g_line);
      g_len = 0;
    }
    return;
  }
  if (g_len < sizeof(g_line)-1) {
    g_line[g_len++] = (char)b;
  } else {
    g_len = 0;
  }
}


// --- FUNCTION THAT EXPORTS THE CURRENT DISPLAY STATE AS JSON ---
static void h_sync(char *args, int q) {
    if (!q) { rc_writeln("-109,Query only command"); return; }
    
    extern String FreqValueForMainMenu;
    extern String FreqUnitForMainMenu;
    extern String AmpValueForMainMenu;
    extern bool FilterStatus;
    extern String StartValueForSweepMenu;
    extern String StartUnitForSweepMenu;
    extern String StopValueForSweepMenu;
    extern String StopUnitForSweepMenu;
    extern String StepValueForSweepMenu;
    extern String StepUnitForSweepMenu;
    extern String DwellValueForSweepMenu;
    extern String AmpValueSweepForSweepMenu;
    extern String StepTypeValueForSweepMenu;
    extern String CountValueForSweepMenu;

    String json = "{";
    json += "\"cw_freq\":\"" + FreqValueForMainMenu + "\",";
    json += "\"cw_unit\":\"" + FreqUnitForMainMenu + "\",";
    json += "\"cw_amp\":\"" + AmpValueForMainMenu + "\",";
    json += "\"filt\":" + String(FilterStatus ? 1 : 0) + ",";
    json += "\"sw_start\":\"" + StartValueForSweepMenu + "\",";
    json += "\"sw_start_u\":\"" + StartUnitForSweepMenu + "\",";
    json += "\"sw_stop\":\"" + StopValueForSweepMenu + "\",";
    json += "\"sw_stop_u\":\"" + StopUnitForSweepMenu + "\",";
    json += "\"sw_step\":\"" + StepValueForSweepMenu + "\",";
    json += "\"sw_step_u\":\"" + StepUnitForSweepMenu + "\",";
    json += "\"sw_dwell\":\"" + DwellValueForSweepMenu + "\",";
    json += "\"sw_amp\":\"" + AmpValueSweepForSweepMenu + "\",";
    json += "\"sw_type\":\"" + StepTypeValueForSweepMenu + "\",";
    json += "\"sw_count\":\"" + CountValueForSweepMenu + "\"";
    json += "}";

    rc_writeln(json.c_str());
}
