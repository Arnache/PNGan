#define VERSION 0.95

/*

This program analyses a PNG file.

Language : C++

Compilation :
- Compiler : (tested with) gcc
- Requirements : zlib library and headers must be installed
- Command : g++ PNGan.cc -lz -o PNGan

Author : Arnaud Chéritat

Licence : CC-BY-SA

Copyright : 2001 for the first version, a few rare updates in between.
Present update: 2016,
  added zlib support + UTF8 output and some conversion from latin1 to utf8
  so now can decode zTXt and iTXt
  partially cleaned the code

input : filename of the png file

outputs to cout

TODO : interpret iCCP?
TODO : some messages have no meaning if corresponding variable is undefined
TODO : concise version
*/

#include "constants.cc"

// Libraries

#include <zlib.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
using namespace std; 

// Global variables

FILE           *fp=NULL;      // pointer on the PNG file
char           *filename;     // will contain argv[..] so no need to free it
//char           str[64];
unsigned char  signature[10];
unsigned char  global_flags;
unsigned char  auxu;
int32_t           width, height;
unsigned char  bit_depth, color_type, compression, filter, interlace;
bool           palette_used, color_used, alpha_used; // color type flags 
bool           end_chunk_met;
bool           text_only;
fpos_t         chunk_start, chunk_next;
long int       chunk_start_longint;
enum types     {pasdef,png1_0};
types          type;
long int       fsize;
long int       palette_size;

int32_t        chunk_length;   // PNG standard tells something strange about the sign here
char           chunk_name[5];  // array
char*          chunk_data;     // dynamic array
uint32_t       chunk_crc;

long int       error_count;

struct erreur_eof_struct {}  erreur_eof;
struct erreur_read_struct {} erreur_read;
struct erreur_neg_struct {}  erreur_neg;

bool first_chunk;      // is current chunk the first one ?
bool header_met;       // HEADER chunk met ?
bool palette_met;
bool data_met;
bool data_ended;
bool data_error;
bool end_met;
bool background_met;
bool chroma_met;
bool gamma_met;
bool histogram_met;
bool pixel_met;
bool transparency_met;
bool bits_met;

// CRC check (adapted from sample of recommandation document)

uint32_t crc_table[256];

void make_crc_table() {
  uint32_t c;
  int n,k;
  
  for(n=0; n<256; n++) {
    c = (uint32_t) n;
    for(k=0; k<8; k++) {
      if(c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    crc_table[n]=c;
  }
}

uint32_t update_crc(uint32_t crc, unsigned char* buf, unsigned int len) {
  uint32_t c = crc;
  unsigned int n;
  
  for(n=0; n<len; n++) {
    c = crc_table[(unsigned int)((c ^ buf[n]) & 0xff)] ^ (c >> 8);
  }
  return c;
}

/*
when fread returns a value < the number of blocks that were called for,
this can be for two reasons. The function below will determine which and throw the corresponding error.
*/

void call_err()
{
  if(feof(fp))
    throw erreur_eof;
  else
    throw erreur_read;
}

/*
as the name says... 
(note that it does not append a null character)
*/
void latin1_to_utf8(unsigned char *in, unsigned char* out, uint32_t len_in, uint32_t& len_out) {
  len_out=0;
  for(uint32_t i=0; i<len_in; i++) {
    uint8_t ch = in[i];
    if(ch < 0x80) {
      out[len_out++]=ch;
    } else {
      out[len_out++]= 0xc0 | (ch & 0xc0) >> 6;
      out[len_out++]= 0x80 | (ch & 0x3f);
    }
  }
}

/* Reads n bytes from file and puts them in the dynamic array "signature" */

void myfread(int n)
{
  if(fread(signature,1,n,fp)<n) call_err();
}

/* Reads n bytes and copies them in the memory starting from adress dest */

void myfread2(int n, void* dest) // DO NOT FORGET : dest argument needs an '&'
{
  if(fread(dest,1,n,fp)<n) call_err();
}

/* Number reading version : bytes order needs to be reversed */

void myfread3(int n, void* dest)  // DO NOT FORGET : dest argument needs an '&'
{
   char* tempo=new char[n];
   if(fread(tempo,1,n,fp)<n) {
     delete[] tempo;
     call_err();
   } 
   for(int i=0; i<n; i++) {  // DANGER here
     ((char *) dest)[i] = tempo[n-1-i];
   }
   delete[] tempo;
}

/* reads content of chunk starting from chunk_start and puts it in chunk_data,
 * CAUTION : never call before having called chunkRead
 */

bool chunk_stream_finished;

unsigned int chunkReadMorsel() {
  unsigned int len;
  if(ftell(fp) + 65535L >= chunk_start_longint + chunk_length) {
    chunk_stream_finished=true;
    len = (unsigned int) ((chunk_start_longint + chunk_length) - ftell(fp) );
  }
  else {
    len = 65535u;
  }
  delete[] chunk_data;
  chunk_data = new char[len];  // CAUTION : no NULL added at the end
  fread(chunk_data,1,len,fp);  // NORMALLY : no eof, as checked in chunkRead()
  return len;
}

void chunkStreamInit() {
  chunk_stream_finished=false;
}


#include "handlers.cc"


// reads next chunk, but without reading the content
// the file index should be at the beginning of the chunk

void chunkRead() {
  bool output=!text_only;

  // read name and size
  
  myfread3(4,&chunk_length);
  myfread2(4,&chunk_name); chunk_name[4]=0;
  if(output) { cout << "- Chunk " << chunk_name << " \n";
  cout << "  Size = " << chunk_length << " bytes\n"; }
  if(chunk_length < 0) {
    throw erreur_neg;
  }

  // memorize position in chunk_start

  fgetpos(fp,&chunk_start);
  chunk_start_longint = ftell(fp);

  // data for the CRC check include the chunk type (but not the chunk length)
  fseek(fp,-4,SEEK_CUR); 
  
  uint32_t crc = 0xffffffffL;
  unsigned int len;
  chunkStreamInit();
  for( ; !chunk_stream_finished; ) {
    len = chunkReadMorsel();
    crc = update_crc(crc,(unsigned char*) chunk_data, len);
  };
  crc = crc ^ 0xffffffffL;
  
  // CRC (Cyclic Redundancy Check) : value is stored as 4 bytes following the chunk
  
  myfread3(4,&chunk_crc);
    
  // memorize next chunk position
  fgetpos(fp,&chunk_next);

  if(chunk_crc != crc) {
    cout << "\n" << "Error: CRC check incorrect (file tells 0x"
         << hex << chunk_crc << " computation gives 0x" << crc << dec << ")\n\n";
    error_count++;
  };
  
  // Vérification du bon ordre des chunks
  
  checkOrder();
  
  // Traitement personnalisé du chunk

  fsetpos(fp,&chunk_start);
  
//      cout << "POS-deb: "<< ftell(fp) << endl;
  handleChunk();
//      cout << "POS-fin: "<< ftell(fp) << endl;
    
  fsetpos(fp,&chunk_next);

  // saut de ligne
  
  if(output) { cout << "\n"; }
}

void show_options() { 
  cout << "  options: -t (--text): Text only\n";
}

void clean() {
  if(fp) fclose(fp);
  delete[] chunk_data;
}

/*
 * Entry point of the program
 */

int main(int argc, char * argv[]) {

  cout << "PngAn v" << VERSION << "\n\n";
  
  chunk_data = new char[1];
  
  make_crc_table();
  
  // test number of aguments

  if(argc!=2 && argc!=3) {
    cout << "Usage: PNGan [options] filename\n";
    cout << "  filename: name of the PNG file to be analysed\n";
    show_options();
    cout << "PNG file Analyser\n";
    cout << "author: Arnaud Cheritat\n";
    cout << "(PNG version up to 1.2, no extensions, no zlib decompression)\n";
    exit(ARG_ERROR);
  };

  if(argc==3) {
    if(strcmp(argv[1],"-t")==0 || strcmp(argv[1],"--text")==0) {
      text_only = true;
    }
    else {
      cout << "Error: bad option (option=first argument)\n";
      show_options();
      exit(ARG_ERROR);
    }
  }
  else {
    text_only = false;
  }
  
  // open the file
  
  filename=argv[argc-1];
  fp=fopen(filename,"rb"); // PNG filename
  if(fp==NULL) {
    cout << "Fatal Error: unable to open file " << filename << "\n";
    exit(OPEN_ERROR);
  };
//  fseek(fp,0,SEEK_END);
 // fsize=ftell(fp);
//  fseek(fp,0,SEEK_SET);
  
  cout << "Analysis of file " << filename << "\n\n\n";
  
  try {
    
    // Read signature
    
    myfread(8);
    
    bool output=!text_only;
    if(output) { cout << "- Signature (first 8 bytes) :"; }
    
    type=pasdef;
    int N=8;
    char sig[8];
    sig[0]=137;
    sig[1]=80;
    sig[2]=78;
    sig[3]=71;
    sig[4]=13;
    sig[5]=10;
    sig[6]=26;
    sig[7]=10;
    if(output) {
      for(int i=0; i<8; i++) {
        cout  << " " << (int) signature[i];
      }
      cout << "\n"; 
    } 
    if(strncmp((char *) signature,sig,N)==0) {
      if(output) { cout << "  correct" << "\n\n"; }
    }
    else {
      cout << "\n" << "Fatal Error: wrong signature\n" 
           << "(should be = h89 h50 h4E h47 h0D h0A h1A h0A in hex,\n" 
           << "meaning 137 80 78 71 13 10 26 10 in decimal)\n";
      clean();
      exit(SIGN_ERROR);
    };
    
    // initialise order flags
    
    first_chunk=true;
    header_met=false;
    palette_met=false;
    data_met=false;
    data_ended=false;
    data_error=false;
    end_met=false;
    background_met=false;
    gamma_met=false;
    chroma_met=false;
    histogram_met=false;
    pixel_met=false;
    transparency_met=false;
    bits_met=false;

    // main loop
    
    end_chunk_met=false;
    bool file_end=false;
    unsigned char dummy;
    do {
      chunkRead(); // handle next chunk
      // the 3 following lines are to check
      // whether we reached the end of the file
      fread(&dummy,1,1,fp);   // this is ridiculous
      file_end = feof(fp);   // this is ridiculous
      fseek(fp,-1,SEEK_CUR); // this is ridiculous
    } while(!end_chunk_met && !file_end);

    if(end_chunk_met && !file_end) {
      cout << "Error: data beyond chunk END (" << (fsize-ftell(fp)) << " bytes)\n";
      error_count++;
    };

    if(!end_chunk_met && file_end) {
      cout << "Error: no END chunk\n";
      error_count++;
    };
    
    cout << "\n" << "Analysis finished.\n\n";
    
    if(error_count >0) {
      cout << error_count << " non-fatal errors detected.\n";
    } else {
      cout << "File looks OK.\n";
    }
    cout << "(No image decoding attempted.)\n";
  }
  catch(erreur_eof_struct err) {
    cout << "Fatal Error: unexpected end of file\n";
    clean();
    exit(EOF_ERROR);
  }
  catch(erreur_read_struct err) {
    cout << "Fatal Error: file read error\n";
    clean();
    exit(READ_ERROR);
  }
  catch(bad_alloc err) {
    cout << "Fatal Error: memory error\n";
    clean();
    exit(MEM_ERROR);
  }
  catch(erreur_neg_struct err) {
    cout << "Fatal Error: negative length chunk\n";
    clean();
    exit(NEG_ERROR);
  }
  
  clean();
}
