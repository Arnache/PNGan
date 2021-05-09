#define VERSION 0.961

#define PROG_NAME "PNGan"

/*

This program analyses a PNG file.

Language : C++ (version: C++11)

Compilation :
- Compiler : (tested with) gcc on linux and cygwin, clang++ on mac
- Requirements : zlib library and headers must be installed
- Command : g++ PNGan.cc -lz -o PNGan
- Alternative commands
> g++ -Wall --std=c++11 --pedantic-errors PNGan.cc -lz -o PNGan.exe
  (strict code error checking alternative) 
> g++ PNGan.cc -O3 -lz -o PNGan.exe
  (optimised for speed of execution of the binary, a priori not necessary) 

Author : Arnaud Chéritat

Licence : CC-BY-SA

See dev-notes.txt

*/

#include "constants.cc"

// Libraries

#include <zlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>
#include <cstring> // bad, used for strcmp and strncmp
#include <cstdint> // bad, used for int*_t and uint*_t

// Global variables

std::ifstream  ifs;          // input file stream of the PNG image
unsigned char  signature[10];
unsigned char  global_flags;
int32_t        width, height;
unsigned char  bit_depth, color_type, compression, filter, interlace;
bool           palette_used, color_used, alpha_used; // color type flags 
bool           end_chunk_met;
bool           text_only;
bool           no_text;
bool           hide_IDAT;
std::streampos chunk_start, chunk_next;
long int       palette_size;

int32_t        chunk_length;   // PNG standard tells something strange about the sign here
char           chunk_name[5];  // null terminated C-style array
std::vector<char>  chunk_data;
uint32_t       chunk_crc;

std::streamoff total_idat_chunks;
std::streamoff total_idat_bytes;
std::streamoff bad_crc_count;
std::streamoff total_text_chunks;

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
  if(ifs.eof())
    throw erreur_eof;
  else
    throw erreur_read;
}

/*
as the name says... 
*/
template<typename T>
std::string latin1_to_utf8(T &in, size_t len) {
  std::string out;
  for(size_t i=0; i<len; i++) {
    unsigned char ch = in[i];
    if(ch < 0x80) {
      out += ch;
    } else {
      out += 0xc0 | (ch & 0xc0) >> 6;
      out += 0x80 | (ch & 0x3f);
    }
  }
  return out;
}

/* Reads n bytes from file and puts them in the dynamic array "signature" */

void readSignature(int n)
{
  if(!ifs.read((char*) &signature[0],n)) call_err();
}

void readChunkName()
{
  if(!ifs.read(chunk_name,4)) call_err();
  chunk_name[4]=0; // null terminated C-style string
}

/* Read an (singed or unsigned) integer, reversing bytes order (endianness)
 *
 * This function used to be simpler, by direct memory mapping.
 * I decided to complicate it so as to ensure it works whatever the 
 * encoding of the integer type on the system.
 *
 * n: number of bytes to read
 * _signed: whether to interpret the read bytes as a signed value
 * dest: destination (its type does --not-- need to be an n-bytes type)
 *
 * if sizeof(Int) >= n it will give the correct value
 * if sizeof(Int) < n the behaviour is undefined
 */
template<typename Int>
void readNumber(int n, Int& dest, bool _signed)
{ // endianness is reversed
  std::vector<unsigned char> tempo(n);
  if(!ifs.read((char*)tempo.data(),n)) call_err();
  dest = 0;
  for(int i=_signed ? 1 : 0; i < n; i++) {
    dest += ((Int) tempo[i]) << 8*(n-1-i);
  }
  if(_signed) {
    unsigned char &c = tempo[0]; // convenience ; hopefully compiler optimizes
    if(c<128) {
      dest += ((Int) c) << 8*(n-1);
    }
    else { // this is complicated so as avoid an overflow in dest 
           // (behaviour undefined under C++ standard) 
      Int temp = 128-(c-128);
      dest = -((temp << 8*(n-1)) - dest);
    }
  }
}

/* reads content of chunk starting from chunk_start and puts it in chunk_data,
 * reads at most 65535 bytes
 * CAUTION : never call before having called chunkRead
 */

bool chunk_stream_finished;

int32_t chunkReadMorsel() {
  int32_t len;
  auto pos = ifs.tellg();
  if(pos + 65535L >= chunk_start + (std::streamoff)chunk_length) {
    // the L suffix above is a security, though on most systems it is not necessary
    chunk_stream_finished=true;
    len = (int32_t)((chunk_start + (std::streamoff)chunk_length) - pos);
  }
  else {
    len = 65535u;
  }
  chunk_data.resize(len);
  ifs.read(chunk_data.data(),len); // NORMALLY : no eof, as checked in chunkRead()
  return len;
}

int32_t chunkReadMorselToCout() {
  int32_t len;
  auto pos = ifs.tellg();
  if(pos + 65535L >= chunk_start + (std::streamoff)chunk_length) {
    // the L suffix above is a security, though on most systems it is not necessary
    chunk_stream_finished=true;
    len = (int32_t)((chunk_start + (std::streamoff)chunk_length) - pos);
  }
  else {
    len = 65535u;
  }
  copy_n( std::istreambuf_iterator<char>(ifs),
          len,
          std::ostreambuf_iterator<char>(std::cout)
  );
  ifs.read(chunk_data.data(),len); // NORMALLY : no eof, as checked in chunkRead()
  return len;
}

void chunkStreamInit() {
  chunk_stream_finished=false;
}


#include "handlers.cc"


// reads next chunk, but without reading the content
// the file index should be at the beginning of the chunk

void chunkRead() {
  using std::cout;
  using std::hex;
  using std::dec;

  // read name and size
  
  readNumber(4,chunk_length,true);
  readChunkName();
  bool output=(!text_only) && !(hide_IDAT && strncmp(chunk_name,DATA,4)==0) ;
  if(output) {
    cout << "- Chunk " << chunk_name << " (size = " << chunk_length << " bytes)\n";
  }
  if(chunk_length < 0) {
    throw erreur_neg;
  }

  // memorize position in chunk_start

  chunk_start = ifs.tellg();

  // data for the CRC check include the chunk type (but not the chunk length)
  ifs.seekg(-4,std::ios_base::cur);
  
  uint32_t crc = 0xffffffffL;
  unsigned int len;
  chunkStreamInit();
  for( ; !chunk_stream_finished; ) {
    len = chunkReadMorsel();
    crc = update_crc(crc,(unsigned char*) chunk_data.data(), len);
  };
  crc = crc ^ 0xffffffffL;
  
  // CRC (Cyclic Redundancy Check) : value is stored as 4 bytes following the chunk
  
  readNumber(4,chunk_crc,false);
    
  // memorize next chunk position
  chunk_next=ifs.tellg();

  if(chunk_crc != crc) {
    cout << "\n" << "Error: CRC check incorrect (file tells 0x"
         << hex << chunk_crc << " computation gives 0x" << crc << dec << ")\n\n";
    bad_crc_count++;
    error_count++;
  };
  
  // Check if the chunk respects chunk ordering rules
  
  checkOrder();
  
  // Depending on the chunk name, call appropriate handling function

  ifs.seekg(chunk_start);
  
  handleChunk();
    
  ifs.seekg(chunk_next);

  // line jump
  
  if(output) { cout << "\n"; }
}

void show_options() { 
  std::cout << "  options : -t (--text-only) : output text chunk contents only\n";
  std::cout << "            -x (--no-text) : do not output text chunks content\n";
  std::cout << "            -n (--no-idat) : keep silent for image data chunks\n";
  std::cout << "                             total count given at the end\n";
}

/*
 * Entry point of the program
 */

int main(int argc, char * argv[]) {

  using std::cout;

  cout << "PngAn v" << VERSION << "\n\n";
  
  make_crc_table();
  
  // test number of aguments

  if(argc<2) {
    cout << "Usage : " << PROG_NAME << " [options] filename\n";
    cout << "  filename : name of the PNG file to be analysed\n";
    show_options();
    cout << "A simple PNG file Analyser\n";
    cout << "(PNG version up to 1.2, no decoding of image data)\n";
    cout << "author : Arnaud Cheritat\n";
    cout << "licence : CC-By-SA\n";
    exit(0);
  };

  text_only = false;
  hide_IDAT = false;
  no_text = false;
  for(int i=1; i<argc-1; i++) {
    if(strcmp(argv[i],"-t")==0 || strcmp(argv[i],"--text-only")==0) {
      text_only = true;
    }
    else if(strcmp(argv[i],"-n")==0 || strcmp(argv[i],"--no-idat")==0) {
      hide_IDAT = true;
    }
    else if(strcmp(argv[i],"-x")==0 || strcmp(argv[i],"--no-text")==0) {
      no_text = true;
    }
    else {
      cout << "Error : bad option " << argv[i] << " (option=all but last argument, filename comes last)\n";
      show_options();
      exit(ARG_ERROR);
    }
  }
  
  // open the file
  
  char *filename=argv[argc-1]; // PNG filename
                               // argv so no need to free this pointer
  ifs.open(filename,std::ifstream::binary);
  if(!ifs) {
    std::cerr << "Fatal Error : unable to open file " << filename << "\n";
    exit(OPEN_ERROR);
  };
  
  ifs.exceptions( std::ifstream::failbit | std::ifstream::badbit );
  cout << "Analysis of file " << filename << "\n\n";
  
  // start the analysis

  total_idat_chunks = 0;
  total_idat_bytes = 0;
  bad_crc_count = 0;

  bool output=!text_only;

  try {
    
    // Read signature
    
    const int sig_size=8; // 8
    unsigned char sig[sig_size] = {137,80,78,71,13,10,26,10};

    readSignature(sig_size);
    
    if(output) { cout << "- Signature (first 8 bytes) :"; }
    
    if(output) {
      for(int i=0; i<sig_size; i++) {
        cout  << " " << (int) signature[i];
      }
      cout << "\n"; 
    } 
    if(strncmp((char *) signature,(char *) sig, sig_size)==0) {
      if(output) { cout << "  correct" << "\n\n"; }
    }
    else {
      cout << "\n" << "Fatal Error: wrong signature\n" 
           << "(should be = h89 h50 h4E h47 h0D h0A h1A h0A in hex,\n" 
           << "meaning 137 80 78 71 13 10 26 10 in decimal)\n";
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
    do {
      chunkRead(); // handle next chunk
      file_end = ifs.peek() == EOF; // ifs.eof();
    } while(!end_chunk_met && !file_end);

    if(!end_chunk_met) {
      cout << "Error: no END chunk\n";
      error_count++;
    }
    else {
      if(!file_end) {
        std::streampos pos = ifs.tellg();
        ifs.seekg(0,std::ios_base::end);
        std::streampos end = ifs.tellg();
        cout << "Error: data beyond chunk END (" << (end-pos) << " bytes)\n";
        error_count++;
      }
    }
    
    if(!text_only) {
      if(hide_IDAT) cout << "- ";
      cout << "Image data: " << total_idat_bytes << " bytes in " << total_idat_chunks << " IDAT chunks\n\n"; 
    }

    if(text_only && total_text_chunks==0) {
      cout << "Found no text chunk\n\n";
    }

    if(bad_crc_count) cout << "Found " << bad_crc_count << " chunks with bad crc checksum\n\n"; 

    cout << "Analysis finished.\n\n";
    
    if(error_count >0) {
      cout << error_count << " non-fatal error" << (error_count>1 ? "s" : "") << " detected.\n";
    } else {
      cout << "File looks OK.\n";
    }
    cout << "(No image decoding attempted.)\n";
  }
  catch(erreur_eof_struct err) {
    cout << "Fatal Error: unexpected end of file\n";
    exit(EOF_ERROR);
  }
  catch(erreur_read_struct err) {
    cout << "Fatal Error: file read error\n";
    exit(READ_ERROR);
  }
  catch(std::bad_alloc &err) {
    std::cerr << "Fatal Error : memory error\n";
    exit(MEM_ERROR);
  }
  catch(erreur_neg_struct err) {
    cout << "Fatal Error: negative length chunk\n";
    exit(NEG_ERROR);
  }
  catch (std::ifstream::failure &e) {
    std::cerr << "Fatal Error : exception opening/reading/closing file\n";
    exit(FILE_ERROR);
  }

  // file automatically closed
}
