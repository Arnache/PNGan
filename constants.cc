// Errors

#define ARG_ERROR       1
#define OPEN_ERROR      2
#define HEAD_ERROR      4
#define EOF_ERROR       8
#define BLOC_ID_ERROR  16
#define EXT_ID_ERROR   32
#define SIGN_ERROR     64
#define NEG_ERROR     128
#define MEM_ERROR     256
#define READ_ERROR    512
#define FILE_ERROR   1024

// Critical

#define HEADER      "IHDR"
#define PALETTE     "PLTE"
#define DATA        "IDAT"
#define END         "IEND"

// Ancilliary (a lowercase letter at the end means : safe to copy)

// v1.0

#define BACKGROUND    "bKGD"
#define CHROMA        "cHRM"
#define GAMMA         "gAMA"
#define HISTOGRAM     "hIST"
#define PIXEL         "pHYs"
#define BITS          "sBIT"
#define TEXT          "tEXt"
#define TIME          "tIME"
#define TRANSPARENCY  "tRNS"
#define ZTEXT         "zTXt"

// v1.1

#define EMBEDDED_ICC  "iCCP"
#define SUGGESTED_PAL "sPLT" 
#define SRGB          "sRGB" 

// v1.2

#define INTERNATIONAL "iTXt"

// extension (not handled)

#define OFFSET        "oFFs"
#define PIX_CAL       "pCAL"
#define GIFGCE        "gIFg"
#define GIFAE         "gIFx"
#define FRACTAL       "fRAc"

// deprecated

#define GIFTEXT       "gIFt"
