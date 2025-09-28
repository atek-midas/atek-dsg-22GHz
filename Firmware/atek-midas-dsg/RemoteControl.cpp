#include "RemoteControl.h"
#include "Lmx2820.h"
#include "MCP23S17.h"
#include "ADC78H90.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "display.h" 


// ============================================================================
// SCPI-like Command Usage Examples (send over USB-CDC; responses shown after →)
// ----------------------------------------------------------------------------
// *IDN?                     → ATEK,DSG-22GHz,1.0
// *RST                      → 0                      (reset to safe defaults)
//                                                     (freq=1GHz, att=0, lo=0, filt=OFF, outp=OFF)
//
// :FREQ 1.234GHz            → 0                      (set PLL frequency; returns 0 if accepted)
// :FREQ?                    → 1234000000             (returns frequency in Hz)
//
// :POW:ATT 12               → 0                      (set global attenuator 0..31 dB)
// :POW:ATT?                 → 12                     (returns last set attenuation)
//
// :POW:LO 5                 → 0                      (set LO/OUTA power 0..7)
// :POW:LO?                  → 5                      (returns last set LO power)
//
// :FILT ON                  → 1                      (enable filter; returns 1 if ON)
// :FILT OFF                 → 0                      (disable filter; returns 0 if OFF)
// :FILT?                    → 0 or 1                 (current filter state)
//
// :OUTP ON                  → 1                      (enable RF output path)
// :OUTP OFF                 → 0                      (disable RF output path)
// :OUTP?                    → 0 or 1                 (current RF output state)
//
// :ADC:READ? 4              → 0.512345               (float reading from channel 4)
//
//
//////////////// ADVANCED COMMANDS /////////////////////////////////////////
// :PLL:WRITE 16,0x1234      → 0                      (write register 16 of PLL)
// :PLL:READ? 16             → 00AB                   (hex word read back from PLL)
//
// :IOEXP:WRITE 1,255        → 0                      (write IO expander addr=1, data=255 or 0xFF)
// :IOEXP:READ? 1            → 37                     (decimal byte read back from expander addr=1)
//

//
// Notes:
// - Numeric suffixes accepted for :FREQ (Hz/kHz/MHz/GHz; case-insensitive).
// - Error replies: -109 (Missing parameter), -104 (Data type error),
//                  -222 (Out of range), -100 (Unknown command).
// - Lines may end with \n or \r\n. Leading ':' is optional.
// - PLL lock check not yet implemented; set commands currently return 0 only.
// ============================================================================


static void h_idn (char*, int);    
static void h_rst (char*, int);
static void h_freq(char*, int);
static void h_pow_att(char*, int);
static void h_outa_pwr(char*, int);
static void h_filt(char*, int);
static void h_outp(char*, int);
static void h_pll_write(char*, int);
static void h_pll_read (char*, int);
static void h_io_write (char*, int);
static void h_io_read  (char*, int);
static void h_adc_read (char*, int);
static int parse_onoff(const char *s, int *ok);
static void rc_write(const char *s);
static void rc_writeln(const char *s);

// ====== SCPI komut imzası ve tablo ======
typedef void (*scpi_handler_t)(char *args, int is_query);



// === Command Table ===
static const scpi_cmd_t scpi_table[] = {
  { "*IDN",        h_idn       },
  { "*RST",        h_rst       },
  { "FREQ",        h_freq      },
  { "POW:ATT",     h_pow_att   },
  { "POW:LO",      h_outa_pwr  },
  { "FILT",        h_filt      },
  { "OUTP",        h_outp      },
  { "PLL:WRITE",   h_pll_write },
  { "PLL:READ",    h_pll_read  },
  { "IOEXP:WRITE", h_io_write  },
  { "IOEXP:READ",  h_io_read   },
  { "ADC:READ",    h_adc_read  },
};

static const size_t scpi_count = sizeof(scpi_table)/sizeof(scpi_table[0]);

// ====== Dispatcher ======
void RC_ParseAndExecute(char *cmd_uc, char *args, int is_query) {
  if (cmd_uc[0]==':') cmd_uc++;

  size_t L = strlen(cmd_uc);
  if (L && cmd_uc[L-1]=='?') { cmd_uc[L-1]=0; is_query = 1; }

  for (size_t i=0; i<scpi_count; i++) {
    if (!strcmp(cmd_uc, scpi_table[i].path)) {
      scpi_table[i].handler(args, is_query); // yönlendir + çalıştır
      return;
    }
  }
  rc_write("-100,Unknown command: ");
  rc_writeln(cmd_uc);
}



// ====== Yazma helper'ı ======
static rc_write_fn g_writer = nullptr;
static void rc_write(const char *s) {
  if (g_writer) { g_writer(s); }
  else          { Serial.print(s); }
}

// ====== Basit yardımcılar ======

static void normalize_arg(char *s) {
  if (!s) return;
  char *d = s;
  for (char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    // Kontrol karakterleri, whitespace vs. hepsini at
    if (c <= 32) continue;
    *d++ = *p;
  }
  *d = '\0';
}



static void rc_writeln(const char *s) { rc_write(s); rc_write("\r\n"); }

static void upper_trim(char *s) {
  // baştaki/sondaki boşlukları kırp + büyük harfe çevir (komut kısmı için)
  size_t n = strlen(s);
  // trim head
  size_t i = 0; while (i<n && (s[i]==' '||s[i]=='\t')) i++;
  if (i>0) memmove(s, s+i, n-i+1), n -= i;
  // trim tail
  while (n>0 && (s[n-1]==' '||s[n-1]=='\t')) s[--n] = 0;
  // upcase
  for (size_t k=0; k<n; ++k) s[k] = (char)toupper((unsigned char)s[k]);
}

static int g_filter_cached = -1;  // -1: bilinmiyor, 0/1: OFF/ON

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
  int on = parse_onoff(args, &ok);   // "ON"/"OFF"/"1"/"0" -> 1/0
  if (!ok) { rc_writeln("-104,Data type error"); return; }

  ApplyFilter(on != 0);
  g_filter_cached = on ? 1 : 0;
  rc_writeln(on ? "1" : "0");
}


static double parse_freq_to_hz(const char *token, int *ok) {
    *ok = 0;
    if (!token || !*token) return 0.0;

    char buf[64]; memset(buf, 0, sizeof(buf));
    strncpy(buf, token, sizeof(buf)-1);

    // Baştaki ve sondaki boşluk/CR/LF temizle
    size_t len = strlen(buf);
    while (len && (isspace((unsigned char)buf[len-1]) || buf[len-1]=='\r' || buf[len-1]=='\n'))
        buf[--len] = 0;
    while (*buf && isspace((unsigned char)*buf)) {
        memmove(buf, buf+1, strlen(buf));
        len--;
    }

    // Hepsini büyük harfe çevirip suffix kontrol yap
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

    // Artık buf sadece sayı
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

// Parametreleri virgülle ayır
static int split_params(char *args, char *outv[], int maxv) {
  int c = 0;
  char *p = args;
  while (p && *p && c<maxv) {
    // baştaki boşlukları at
    while (*p==' '||*p=='\t') p++;
    if (!*p) break;
    outv[c++] = p;
    normalize_arg(outv[c-1]); 
    // bir sonraki virgülü bul
    char *comma = strchr(p, ',');
    if (!comma) break;
    // bu parametreyi sonlandır
    *comma = 0;
    p = comma + 1;
  }
  return c;
}



// ---- İLERİDE WIFI İÇİN AYNI HANDLER'LAR KULLANILACAK ----

// === Handlers ===
static void h_idn (char *args, int q) {
  (void)args; (void)q;
  rc_writeln("ATEK,DSG-22.6GHz,r1.0");
}

static void h_rst (char *args, int q) {
  (void)args; (void)q;
  rc_writeln("Not Implemented yet!");
}


static int g_outa_pwr_cached = -1;  // -1 = unknowm, 0..7 valid

static void h_outa_pwr(char *args, int q) {

  if (q) {
    if (g_outa_pwr_cached >= 0) {
      char out[8];
      snprintf(out, sizeof(out), "%d", g_outa_pwr_cached);
      rc_writeln(out);
    } else {
      rc_writeln("-303,No cached value");   // istersen 0 yazdır
    }
    return;
  }

  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }

  // boşlukları kırp
  while (*args==' ' || *args=='\t') ++args;
  char *endp = nullptr;
  long v = strtol(args, &endp, 10);
  if (endp && *endp!=0) { rc_writeln("-104,Data type error"); return; }
  if (v < 0 || v > 7)   { rc_writeln("-222,Data out of range"); return; }

  Lmx2820SetOUTA_PWR((uint8_t)v);
  g_outa_pwr_cached = (int)v;
  rc_writeln("0");
}




static void h_freq(char *args, int q) {
  if (q) {
    rc_writeln("0"); 
    return;
  }
  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }

  int ok=0; 
  double hz = parse_freq_to_hz(args, &ok);
  if (!ok || hz<=0)    { rc_writeln("-104,Data type error"); return; }

  double fMHz = hz / 1e6;     // Hz → MHz çeviriyoruz
  SetFilterBand(fMHz);
  ApplyFrequency(hz);
  rc_writeln("0");
}


static int g_att_cached = -1;  // -1: bilinmiyor, 0..31 geçerli
 

static void h_pow_att(char *args, int q) {
  // :POW:ATT <0..31>
  // :POW:ATT?          -> son ayarlanan değeri döner (cache)
  if (q) {
    if (g_att_cached >= 0) {
      char out[8];
      snprintf(out, sizeof(out), "%d", g_att_cached);
      rc_writeln(out);
    } else {
      rc_writeln("-303,No cached value");
    }
    return;
  }

  if (!args || !*args) { rc_writeln("-109,Missing parameter"); return; }

  // baştaki boşlukları kırp
  while (*args==' ' || *args=='\t') ++args;

  char *endp = nullptr;
  long v = strtol(args, &endp, 10);
  if (endp && *endp!=0) { rc_writeln("-104,Data type error"); return; }
  if (v < 0 || v > 31)  { rc_writeln("-222,Data out of range"); return; }

  ApplyAmplitude((double)v);
  g_att_cached = (int)v;
  rc_writeln("0");
}


static void h_outp(char *args, int q) {
  if (q) {
    rc_writeln("0"); // istersen OUTP state tutup yaz
    return;
  }
  if (!args || !*args){ rc_writeln("-109,Missing parameter"); return; }
  int ok=0; int on = parse_onoff(args, &ok);
  if (!ok)            { rc_writeln("-104,Data type error");   return; }
  // RF yolunu aç/kapat — örnek olarak PE4257 switch’e map
  SetPLL1OnOff(on ? 1 : 0);
  rc_writeln(on ? "1" : "0");
}

static void h_pll_write(char *args, int q) {
  (void)q;
  // :PLL:WRITE <pll>,<addr>,<dataHex>
  char *v[3] = {0};
  int n = split_params(args, v, 3);
  if (n != 3) {
    rc_writeln("-109,Missing parameter");
    return;
  }


  uint32_t addr = (uint32_t)strtoul(v[1], NULL, 10);   // adres onluk verildi ise
  if (strncasecmp(v[1], "0X", 2) == 0)
    addr = (uint32_t)strtoul(v[1], NULL, 16);
  uint32_t data = (uint32_t)strtoul(v[2], NULL, 16);

  PLL_write(addr, data);  

  rc_writeln("0");
}


static void h_pll_read(char *args, int q) {
  (void)q;
  // :PLL:READ? <pll>,<addr>
  char *v[2]={0};
  int n = split_params(args, v, 2);
  if (n != 2) {
    rc_writeln("-109,Missing parameter");
    return;
  }

  uint32_t addr = (uint32_t)strtoul(v[1], NULL, 10);
  if (strncasecmp(v[1], "0X", 2) == 0)
    addr = (uint32_t)strtoul(v[1], NULL, 16);

  uint16_t d = PLL_read(addr);

  char out[16];
  snprintf(out, sizeof(out), "%04X", (unsigned)d);
  rc_writeln(out);
}


static void h_io_write(char *args, int q) {
  (void)q;
  // :IOEXP:WRITE <exp>,<addr>,<data>
  char *v[3] = {0};
  if (split_params(args, v, 3) != 3) {
    rc_writeln("-109,Missing parameter");
    return;
  }

  int exp = atoi(v[0]);
  uint32_t addr = (uint32_t)strtoul(v[1], NULL, 10);
  uint32_t data = (uint32_t)strtoul(v[2], NULL, 10);

  if (exp == 1) {
    IO_EXP1_write(addr, data);
  } else {
    rc_writeln("-222,IOEXP out of range");
    return;
  }

  rc_writeln("0");
}


static void h_io_read(char *args, int q) {
  (void)q;
  // :IOEXP:READ? <exp>,<addr>
  char *v[2] = {0};
  if (split_params(args, v, 2) != 2) {
    rc_writeln("-109,Missing parameter");
    return;
  }
  int exp = atoi(v[0]);
  uint32_t addr = (uint32_t)strtoul(v[1], NULL, 10);

  uint8_t d = 0;
  if (exp == 1) {
    d = IO_EXP1_read(addr);
  } else {
    rc_writeln("-222,IOEXP out of range");
    return;
  }

  char out[16];
  snprintf(out, sizeof(out), "%u", (unsigned)d);
  rc_writeln(out);
}


static void h_adc_read(char *args, int q) {
  (void)q;
  // :ADC:READ? <ch>
  if (!args || !*args){ rc_writeln("-109,Missing parameter"); return; }
  uint32_t ch = (uint32_t)strtoul(args, NULL, 10);
  float v = ADC_Read(ch);
  char out[48]; snprintf(out, sizeof(out), "%f", (double)v);
  rc_writeln(out);
}


// ====== Satır işleme ======
void RC_HandleLine(char *line) {
  if (!line) return;

  // Satırı kopyalayıp komut ve argümanları ayıralım
  char buf[256]; memset(buf, 0, sizeof(buf));
  strncpy(buf, line, sizeof(buf)-1);

  // CR/LF temizle
  for (size_t i=0; buf[i]; ++i) {
    if (buf[i]=='\r' || buf[i]=='\n') { buf[i]=0; break; }
  }
  if (!buf[0]) return;

  // Komut kısmını (ilk boşluğa kadar) ve argümanı ayır
  char *space = strchr(buf, ' ');
  char *cmd   = buf;
  char *args  = nullptr;
  if (space) { *space = 0; args = space+1; }
  if (args) normalize_arg(args);

  // Komutun UPPER/trim hali
  char cmd_uc[128]; memset(cmd_uc,0,sizeof(cmd_uc));
  strncpy(cmd_uc, cmd, sizeof(cmd_uc)-1);
  // baştaki ':' kalsın ama geri kalan upper olsun
  if (cmd_uc[0]==':') {
    for (size_t i=1; cmd_uc[i]; ++i) cmd_uc[i] = (char)toupper((unsigned char)cmd_uc[i]);
  } else {
    upper_trim(cmd_uc);
  }

  // args sade kalsın (parametrelerde case önemli olabilir)
  RC_ParseAndExecute(cmd_uc, args, 0);
}

// ====== Satır toplayıcı ======
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
    // taşma — satırı sıfırla
    g_len = 0;
  }
}
