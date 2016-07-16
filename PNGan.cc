/*

This program analyses a PNG file.

Language : C++

Compiler : gcc

Compilation : g++ PNGan.cc -o PNGan.exe

Version : 0.92

Author : Arnaud Chéritat

Licence : CC-BY-SA

Copyright : 2001 for the first version, a few rare updates in between.
Present update: 2016

input : filename of the png file

outputs to cout

TODO : handle ztext (needs zlib)
TODO : some messages have no meaning if corresponding variable is undefined
TODO : count non-fatal errors
TODO : concise version

*/

#include "constants.cc"

// Libraries

#include <iostream>
#include <cstdio>
#include <cstdlib>
using namespace std; 

// Global variables

FILE           *fp=NULL;      // pointer on the PNG file
char           *filename;     // will contain argv[..] so no need to free it
//char           str[64];
unsigned char  signature[10];
unsigned char  global_flags;
unsigned char  auxu;
long           width, height;
unsigned char  bit_depth, color_type, compression, filter, interlace;
bool           palette_used, color_used, alpha_used; // color type flags 
bool           fin; // end of file reached
bool           text_only;
fpos_t         pos_retenue, pos_temp;
long int       pos_retenue_longint;
enum types     {pasdef,png1_0};
types          type;
long int       fsize;
int            palette_size;

long int       chunk_length;
char           chunk_name[5];  // array
char*          chunk_data;     // dynamic array
unsigned long  chunk_crc;

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

unsigned long crc_table[256];

void make_crc_table() {
  unsigned long c;
  int n,k;
  
  for(n=0; n<256; n++) {
    c = (unsigned long) n;
    for(k=0; k<8; k++) {
      if(c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    crc_table[n]=c;
  }
}

unsigned long update_crc(unsigned long crc, unsigned char* buf, unsigned int len) {
  unsigned long c = crc;
  unsigned int n;
  
  for(n=0; n<len; n++) {
    c = crc_table[(unsigned int)((c ^ buf[n]) & 0xff)] ^ (c >> 8);
  }
  return c;
}

/*
fp is the file pointer
hold() marks the current position in and goes to another
release() goes back to the marked position
*/

void hold() {
  fgetpos(fp,&pos_temp);
  fsetpos(fp,&pos_retenue);
}

void release() {
  fsetpos(fp,&pos_temp);
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

/* reads content of chunk starting from pos_retenue and puts it in chunk_data,
 * CAUTION : never call before having called chunkRead
 */

bool chunk_load_finished;

unsigned int chunkLoad() {
  unsigned int len;
  if(ftell(fp) + 65535L >= pos_retenue_longint + chunk_length) {
    chunk_load_finished=true;
    len = (unsigned int) ((pos_retenue_longint + chunk_length) - ftell(fp) );
  }
  else {
    len = 65535u;
  }
  delete[] chunk_data;
  chunk_data = new char[len];  // CAUTION : no NULL added at the end
  fread(chunk_data,1,len,fp);  // NORMALLY : no eof, as checked in chunkRead()
  if(chunk_load_finished) {
    release();
  }
  return len;
}

void chunkLoadInit() {
  chunk_load_finished=false;
  hold();
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
  
  // memorize position in pos_retenue
  
  fgetpos(fp,&pos_retenue);
  pos_retenue_longint = ftell(fp);
  fseek(fp,chunk_length,SEEK_CUR);
  
  // CRC (Cyclic Redundancy Check)
  
  myfread3(4,&chunk_crc);
  
  chunkLoadInit();
  fseek(fp,-4,SEEK_CUR);
  unsigned long crc = 0xffffffffL;
  unsigned int len;
  for( ; !chunk_load_finished; ) {
    len = chunkLoad();
    crc = update_crc(crc,(unsigned char*) chunk_data, len);
  };
  crc = crc ^ 0xffffffffL;
  if(chunk_crc != crc) {
    cout << "\n" << "Error : CRC check incorrect (file tells 0x"
         << hex << chunk_crc << " computation gives 0x" << crc << dec << ")\n\n";
  };
  
  // Vérification du bon ordre des chunks
  
  checkOrder();
  
  // Traitement personnalisé du chunk
  
  handleChunk();
  
  // saut de ligne
  
  if(output) { cout << "\n"; }
}

void show_options() { 
  cout << "  options : -t (--text) : Text only\n";
}

void clean() {
  if(fp) fclose(fp);
  delete[] chunk_data;
}

/*
 * Entry point of the program
 */

int main(int argc, char * argv[]) {

  cout << "PngAn v0.92\n\n";
  
  chunk_data = new char[1];
  
  make_crc_table();
  
  // test number of aguments

  if(argc!=2 && argc!=3) {
    cout << "Usage : PNGan [options] filename\n";
    cout << "  filename : name of the PNG file to be analysed\n";
    show_options();
    cout << "PNG file Analyser\n";
    cout << "author : Arnaud Cheritat\n";
    cout << "(PNG version up to 1.2, no extensions, no zlib decompression)\n";
    exit(ARG_ERROR);
  };

  if(argc==3) {
    if(strcmp(argv[1],"-t")==0 || strcmp(argv[1],"--text")==0) {
      text_only = true;
    }
    else {
      cout << "Error : bad option (option=first argument)\n";
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
    cout << "Fatal Error : unable to open file " << filename << "\n";
    exit(OPEN_ERROR);
  };
  fseek(fp,0,SEEK_END);
  fsize=ftell(fp);
  fseek(fp,0,SEEK_SET);
  
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
      cout << "\n" << "Fatal Error : wrong signature\n" 
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
    
    fin=false;
    do {
      chunkRead(); // handle next chunk
    } while(!fin);

    if(ftell(fp) != fsize) {
            cout << "Error : data beyond chunk END (" << (fsize-ftell(fp)) << " bytes)\n";
    };
    
    cout << "\n" << "Analysis finished.\n\n";
    
    if(error_count >0) {
      cout << "Errors occurred.\n";
    } else {
      cout << "File is OK.\n";
    }
  }
  catch(erreur_eof_struct err) {
    cout << "Fatal Error : unexpected end of file\n";
    clean();
    exit(EOF_ERROR);
  }
  catch(erreur_read_struct err) {
    cout << "Fatal Error : unexpected end of file\n";
    clean();
    exit(READ_ERROR);
  }
  catch(bad_alloc err) {
    cout << "Fatal Error : memory error\n";
    clean();
    exit(MEM_ERROR);
  }
  catch(erreur_neg_struct err) {
    cout << "Fatal Error : negative length chunk\n";
    clean();
    exit(NEG_ERROR);
  }
  
  clean();
}
