// Handlers

bool readKeyword(const char* key_text, bool output) {
  // returns false if chunk processing shall stop
  unsigned int sz = (unsigned int)((chunk_length < 80) ? chunk_length : 80);
  delete[] chunk_data;
  chunk_data = new char[sz];
  if(fread(chunk_data,1,sz,fp)<sz) call_err();

  bool null_found=false;
  bool printable=true;
  int i;
  unsigned char d;
  for(i=0; i<sz && !null_found; i++) {
    d=(unsigned char) chunk_data[i];
    null_found = d==0;
    printable=printable && (d==0 || (d>=32u && d<=126u) || (d>=161u && d<=255u));
  }
  
  if(!null_found) {
    cout << "Error: Keyword missing or too long (should be < 80 characters)\n";
    error_count++;
    return false;
  }

  fsetpos(fp,&chunk_start);
  fseek(fp,i,SEEK_CUR);

  if(!printable) {
    cout << "Error: Keyword contains non pritable characters (should be latin1 encoded with char codes in 32-126 or 161-255)\n";
    error_count++;
  }
  
  if(output) {
    char keyword[100];
    strcpy(keyword,chunk_data);
    char keyword2[200];
    unsigned int len2;
    latin1_to_utf8((unsigned char*)keyword,(unsigned char*)keyword2,(unsigned int)i,len2);
    cout << key_text;
    cout.write(keyword2,(int)len2);
    cout << "\"\n";
  }
  
  return true;
  // in this case file position is just after the null string terminator
}

void output_ztext(char *buffer, uint32_t len, const char* head_text, const char* trail_text, bool latin1) {
  int MORSEL=16384;

  int ret;
  uint32_t have,have2;
  z_stream strm;
  unsigned char out[MORSEL];
  unsigned char out2[2*MORSEL];

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit(&strm);
  if (ret != Z_OK) {
    cout << "Error initializing zlib...\n";
    return;
  } 

  cout << head_text;
  
  int32_t delta;
  uint32_t i=0;

  do {
    delta=len-i;
    if(delta==0) {
      cout << "\nError while deflating: chunk finished before any ending marker was reached\n";
      goto fin;
    }
    if(delta>MORSEL) delta=MORSEL;
    strm.avail_in = delta;
    strm.next_in = (unsigned char *)buffer+i;
    i += delta;
    
    do {
      strm.avail_out = MORSEL;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          cout << "\nData error while deflating... nb " << ret << "\n";
          goto fin;
      }        

      have = MORSEL - strm.avail_out;
      if(latin1) {
        latin1_to_utf8(out,out2,have,have2);
        cout.write((char *)out2,have2);
      }
      else {
        cout.write((char *)out,have);
      }

    } while (strm.avail_out == 0);
    
  } while (ret != Z_STREAM_END);

fin:
  (void)inflateEnd(&strm);

  cout << trail_text;
}

void handleHeader(bool output) {
  
  if(chunk_length!=13) {
    cout << "Fatal Error: header chunk should be 13 bytes long\n";
  }
  else {
    
    myfread3(4,&width);
    if(output) { cout << "    Width: " << width << "\n"; }
    if(width<0) {
      cout << "Error: negative width";
      error_count++;
    }
    
    myfread3(4,&height);
    if(output) { cout << "    Height: " << height << "\n"; }
    if(height<0) {
      cout << "Error: negative height";
      error_count++;
    }
    
    myfread3(1,&bit_depth);
    if(output) { cout << "    Bit depth: " << (int) bit_depth << "\n"; }
    
    myfread3(1,&color_type);
    bool good_color;
    char aux2[10];
    if(color_type>7) {
      good_color=false;
    }
    else {
      good_color=true;
      aux2[0] = 'b';
      aux2[1] = color_type & 1 ? '1' : '0';
      aux2[2] = color_type & 2 ? '1' : '0';
      aux2[3] = color_type & 4 ? '1' : '0';
      aux2[4] = 0;
    }
    if(output) { cout << "    Color type: " << (int) color_type; }
    if(good_color) {
      if(output) { cout << " (" << aux2 << ")"; }
    }
    if(output) { cout << "\n"; }
    
    myfread3(1,&compression);
    if(output) { cout << "    Compression: " << (int) compression << "\n"; }
    bool good_compression = (compression == 0);
    
    myfread3(1,&filter);
    if(output) { cout << "    Filter: " << (int) filter << "\n"; }
    bool good_filter = (compression == 0);
    
    myfread3(1,&interlace);
    if(output) { cout << "    Interlace: " << (int) interlace << "\n"; }
        
    if(!good_color) {
      cout << "Error: color type has no meaning";
      error_count++;
    }
    else {
      palette_used = color_type & 1;
      color_used = color_type & 2;
      alpha_used = color_type & 4;
      if(output) { cout << "\n  Interpretation: " 
           << (palette_used ? "palette used" : "no palette")
           << ", "
           << (color_used   ? "color used" : "no color")
           << ", "
           << (alpha_used ? "alpha channel used" : "no alpha channel")
           << "\n" ;
      cout << "  meaning: "; }
      
      switch(color_type) {
      case 0 : {
        switch(bit_depth) {
        case 1 : case 2 : case 4 : case 8 : case 16 : {
          if(output) { cout << "Monochrome with " << (1 << bit_depth) << " gray levels"; }
        } break;
        default : {
          cout << "\n  Error: forbidden value of bit depth (should be 1,2,4,8 or 16 for color type 0)";
          error_count++;
        }
        };
      } break;
      case 2 : {
        switch(bit_depth) {
        case 8 : case 16 : {
          if(output) { cout << "True color with " << (1 << bit_depth) << " levels of R, G and B"; }
        } break;
        default : {
          cout << "\n  Error: forbidden value of bit depth (should be 8 or 16 for color type 2)"; 
          error_count++;
        }
        };
      } break;
      case 3 : {
        switch(bit_depth) {
        case 1 : case 2 : case 4 : case 8 : {
          if(output) { cout << "Palette with " << (1 << bit_depth) << " colors"; }
        } break;
        default : {
          cout << "\n  Error: forbidden bit depth (should be 1,2,4 or 8 for color type 3)";
          error_count++;
        }
        };
      } break;
      case 4 : {
        switch(bit_depth) {
        case 8 : case 16 : {
          if(output) { cout << "Monochrome with transparency with " << (1 << bit_depth) << " levels of gray and alpha"; }
        } break;
        default : {
          cout << "\n  Error: forbidden bit depth (should be 8 or 16 for color type 4)";
          error_count++;
        }
        };
      } break;
      case 6 : {
        switch(bit_depth) {
        case 8 : case 16 : {
          if(output) { cout << "TrueColor with transparency withn " << (1 << bit_depth) << " levels of R, G, B and alpha"; }
        } break;
        default : {
          cout << "\n  Error: forbidden bit depth (should be 8 or 16 for color type 6)";
          error_count++;
        }
        };
      } break;
      default : {
        cout << "Error: forbidden color type";
        error_count++;
      }
      };
      cout << "\n";
      
      if(!good_compression) {
              cout << "Error: compression type unknown (only 0 is allowed in PNG 1.0)\n";
        error_count++;
      };
      if(!good_filter) {
        cout << "Error: filter type unknown (only 0 is allowed in PNG 1.0)\n";
        error_count++;
      };
      switch(interlace) {
      case 0 : {
          if(output) { cout << "  No interlace\n";}
      } break;
      case 1 : {
        if(output) { cout << "  Interlace: Adam7\n"; }
      } break;
      default : {
        cout << "Error: unknown interlace type (only 0 and 1 are allowed in PNG 1.0)\n";
        error_count++;
      }
      };
    };
  };
}

void handlePalette(bool output) {
  if(header_met) {
    if((chunk_length % 3) != 0) {
      cout << "Error: chuck size should be a multiple of 3\n";
      error_count++;
    }
    else {
      palette_size =(uint32_t)( ldiv(chunk_length,3).quot); // normally, length >0
      if(output) { cout << "    number of entries = " << palette_size << "\n"; }
      if(color_type==2 || color_type==6) { // Suggested Palette
        if(output) { cout << "    the suggested palette if the display is not TrueColor\n"; }
        if(256 < palette_size ) {
          cout << "Error: palette length should not exceed 256\n";
          error_count++;
        }
      }
      if(color_type==3) { // Palette (compulsory)
        if((1 << bit_depth) < palette_size) {
          cout << "Error: palette length should not exceed what has been\n"
               << "        declared in the Header\n";
          error_count++;
        }
      }
    }
  }
}

void handleData() { // do nothing
}

void handleEnd() {
  end_chunk_met=true;
}

void handleBackground(bool output) {
  if(header_met) {
    switch(color_type) {
    case 3 : {
      unsigned char c;
      if(chunk_length!=1) {
        cout << "Error: chunk size should be 1 for color mode 3";
        error_count++;
      }
      else {
        myfread3(1,&c);
        if(c < palette_size) {
          if(output) { cout << "    Background color has index (in the palette) = " << c << "\n"; }
        }
        else {
          cout << "Error: background color's index is out of the palette\n";
          error_count++;
        }
      }
    } break;
    case 0 : case 4 : {
      uint16_t c;
      if(chunk_length!=2) {
        cout << "Error: chunk size should be 2 for color modes 0 and 4";
        error_count++;
      }
      else {
        myfread3(2,&c);
        if(c < (1 << bit_depth)) {
          if(output) { cout << "    Background Intensity = " << c << "\n"; }
        }
        else {
          cout << "Error: background intensity is over the maximum\n";
          error_count++;
        }
      }
    } break;
    case 2 : case 6 : {
      uint16_t cR,cG,cB;
      if(chunk_length!=6) {
        cout << "Error: chunk size should be 6 for color modes 2 and 6";
        error_count++;
      }
      else {
        myfread3(2,&cR);
        myfread3(2,&cG);
        myfread3(2,&cB);
        if(cR < (1 << bit_depth) && cG < (1 << bit_depth) && cB < (1 << bit_depth)) {
          if(output) { cout << "    RGB values of background = " << cR << "," << cG << "," << cB << "\n"; }
        }
        else {
          cout << "Error: RGB values of background over bit_depth\n";
          error_count++;
        }
      }
    } break;
    default : {
      cout << "Error: meaning of background color depends on color type, which has a forbidden value\n";
      error_count++;
    }
    };
  }
  else {
    cout << "Error: meaning of background color depends on color type, which is undefined\n";
    error_count++;
  }
}

void handleChroma(bool output) {
  if(chunk_length!=32) {
    cout << "Error: chromaticity chunk length should be 32 bytes\n";
    error_count++;
  }
  else {
    typedef long double MYREAL;
    double auxf;
    uint32_t a;
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    White Point x = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    White Point y = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Red Point x = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Red Point y = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Green Point x = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Green Point y = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Blue Point x = " << auxf << "\n"; }
    myfread3(4,&a); auxf = ((MYREAL) a)/((MYREAL) 100000L);
    if(output) { cout << "    Blue Point y = " << auxf << "\n"; }
  }
}

void handleGamma(bool output) {
  if(chunk_length!=4) {
    cout << "Error: GAMMA chunk length should be 4 bytes\n";
    error_count++;
  }
  else {
    float gamma;
    uint32_t a;
    myfread3(4,&a);
    gamma = ((float) a)/((float) 100000L);
    if(output) { cout << "    Gamma = " << gamma << "\n"; }
  }
}

void handleHistogram(bool output) {
  if(palette_met) {
    if(chunk_length != 2*palette_size) {
      cout << "Error: histogram should have same number of entries as the palette\n";
      error_count++;
    }
  }
}

void handlePixel(bool output) {
  if(chunk_length!=9) {
    cout << "Error: this chunk should have 9 octets\n";
    error_count++;
  }
  else {
    uint32_t a,b;
    unsigned char c;
    myfread3(4,&a);
    myfread3(4,&b);
    myfread3(1,&c);
    if(output) { cout << "    X : " << a << " dots per unit"; }
    if(c==1) {
      if(output) { cout << " (meaning " << 0.0254*((float) a) << "dpi)"; }
    }
    if(output) { cout << "\n"; }
    if(output) { cout << "    Y : " << b << " dots per unit" ; }
    if(c==1) {
      if(output) { cout << " (meaning " << 0.0254*((float) b) << "dpi)"; }
    }
    if(output) { cout << "\n"; }
    if(output) { cout << "    Unit specifier " << (int) c ; }
    switch(c) {
    case 0 : {
      if(output) { cout << " (no unit)\n"; }
    } break;
    case 1 : {
      if(output) { cout << " (meter)\n"; }
    } break;
    default : {
      cout << "\n" << "Error: unit specifier should be 0 or 1\n";
      error_count++;
    }
    }
  }
}

void handleBits(bool output) {
  if(output) { cout << "    Significant bits of original data: "; }
  uint16_t red,green,blue,gray,alpha;
  switch(color_type) {
  case 0 : {
    if(chunk_length!=1) {
      cout << "Error: in color mode 0, this chunk should be 1 byte long";
      error_count++;
      return;
    }
    myfread3(1,&gray);
    if(output) { cout << gray << "\n"; }
    if(gray==0 || gray>bit_depth) {
      cout << "Error: should be > 0 and at most equal to the bit depth";
      error_count++;
    }
  } break;
  case 2 : case 3 : {
    if(chunk_length!=1) {
      cout << "Error: in color modes 2 and 3, this chunk should be 3 bytes long";
      error_count++;
      return;
    }
    myfread3(1,&red);
    myfread3(1,&green);
    myfread3(1,&blue);
    if(output) { cout << "red=" << red << ", green=" << green << ", blue=" << blue << "\n"; }
    if(red==0 || red>bit_depth || green==0 || green>bit_depth || blue==0 || blue>bit_depth) {
      cout << "Error: values should be > 0 and at most equal to the bit depth";
      error_count++;
    }
  } break;
  case 4 : {
    if(chunk_length!=2) {
      cout << "Error: in color mode 4, this chunk should be 2 bytes long";
      error_count++;
      return;
    }
    myfread3(1,&gray);
    myfread3(1,&alpha);
    if(output) { cout << "gray=" << gray << ", alpha=" << alpha << "\n"; }
    if(gray==0 || gray>bit_depth || alpha==0 || alpha>bit_depth) {
      cout << "Error: values should be > 0 and at most equal to the bit depth";
      error_count++;
    }
  } break;
  case 6 : {
    if(chunk_length!=4) {
      cout << "Error: in color mode 6, this chunk should be 4 bytes long";
      error_count++;
      return;
    }
    myfread3(1,&red);
    myfread3(1,&green);
    myfread3(1,&blue);
    myfread3(1,&alpha);
    if(output) { cout << "red=" << red << ", green=" << green << ", blue=" << blue 
         << ", alpha=" << alpha << "\n"; }
    if(red==0 || red>bit_depth || green==0 || green>bit_depth ||
       blue==0 || blue>bit_depth || alpha==0 || alpha>bit_depth ) {
      cout << "Error: values should be > 0 and at most equal to the bit depth";
      error_count++;
    }
  } break;
    default : cout << "depends on colortype which is wrong\n";
  }
}

void handleTime(bool output) {
  if(chunk_length!=7) {
    cout << "Error: this chunk should be 7 bytes long\n";
    error_count++;
  }
  else {
    uint16_t year;
    unsigned char month, day, hour, minute, second;
    myfread3(2,&year);
    myfread3(1,&month);
    myfread3(1,&day);
    myfread3(1,&hour);
    myfread3(1,&minute);
    myfread3(1,&second);
    if(output) { cout << "    Last modification:" 
         << " time: "  << (int) hour << "h:" << (int) minute << "mn:" << (int) second << "s"
         << " date: " << (int) day << "/" << (int) month << "/" << (int) year
         << "\n"; }
    
  }
}

void handleTransparency(bool output) {
  if(color_type==3) {
    if(output) { cout << "    in color mode 3, this chunk contains an array of\n"
         << "    alpha values corresponding to palette entries\n";
    cout << "    entries: " << chunk_length; }
    if(palette_met) {   
      if(chunk_length>palette_size) {
        cout << "Error: more entries than the palette\n";
        error_count++;
      }
    }
    else {
      cout << "Error: this chunk should occur before palette chunk\n";
      error_count++;
    }
    return;
  }
  if(color_type==0) {
    if(chunk_length!=2) {
      cout << "Error: chunk should be 2 bytes long\n";
      error_count++;
    } else {
      uint16_t index;
      myfread3(2,&index);
      if(output) { cout << "    in color mode 0, this chunk contains the gray level of\n"
           << "    the only transparent color: " << index << "\n"; }
      if(index >= (1 << bit_depth) ) {
        cout << "Error: index too big for given bit depth (max="
             << ((1 << bit_depth) -1) << ")\n";
        error_count++;
      }
    }
    return;
  }
  if(color_type==2) {
    if(chunk_length!=6) {
      cout << "Error: chunk should be 6 bytes long\n";
      error_count++;
    } else {
      uint16_t ir,ig,ib,mx;
      myfread3(2,&ir);
      myfread3(2,&ig);
      myfread3(2,&ib);
      if(output) { cout << "    in color mode 0, this chunk contains the RGB values of\n"
           << "    the only transparent color: "; 
      cout << ir << ", " << ig << ", " << ib << "\n"; }
      mx=ir;
      mx=max(mx,ig);
      mx=max(mx,ib);
      if(mx >= (1 << bit_depth)) {
        cout << "Error: value too big for given bit depth (max="
             << ((1 << bit_depth) -1) << ")\n";
        error_count++;
      }
    }
    return;
  }
  cout << "Error: this chunk is forbidden in color modes other than 0,2,3\n";
}

void handleText(bool output) {
  
  cout << "    Textual data, latin-1 encoded.\n";

  if(!readKeyword("    Keyword: \"",output)) return;

  if(output) {
    cout << "    Text: \"";
//    fsetpos(fp,&chunk_start);
    chunkStreamInit();
    for( ; !chunk_stream_finished; ) {
      uint32_t len=chunkReadMorsel();
      char* texte=new char[len];
      strncpy(texte, chunk_data, len);
//      if(output) { cout.write((char *)texte, len); }
      unsigned char* texte2=new unsigned char[2*len]; // 2*len cannot exceed uint32_t's limit because the morsel is small enough (for 2 reasons)
      uint32_t len2;
      latin1_to_utf8((unsigned char*)texte,texte2,len,len2);
      if(output) { cout.write((char *)texte2, len2); } // len2 should be small enough to be correctly converted to a length of type "streamsize"
      delete[] texte;
      cout << "\"\n";
    }
  }
}

void handleZtext(bool output) {

  cout << "    Compressed textual data, latin-1 encoded.\n";

  if(!readKeyword("    Keyword: \"",output)) return;

  unsigned char method;
  myfread2(1,&method);
  
  if(output) {
    cout << "    Compression method (0=zlib) : " << (int)method << "\n";
  }
  
  if(output) {
    // buffer the whole chunk remaider in memory
    uint32_t len = chunk_length-(ftell(fp)-chunk_start_longint);
    delete[] chunk_data;
    chunk_data = new char[len];
    if(fread(chunk_data,1,len,fp)<len) call_err();

    output_ztext(chunk_data,len,"    Text: \"","\"\n",true);
  }
}

void handleItext(bool output) {

  cout << "    International textual data, utf-8 encoded.\n";

  if(!readKeyword("    Keyword: \"",output)) return;

  unsigned char compressed;
  myfread2(1,&compressed);
  if(output) { cout << "    Compressed? (0=no, 1=yes) : " << (int)compressed << "\n"; }

  unsigned char method;
  myfread2(1,&method);
  if(output) { cout << "    Compression method (0=zlib) : " << (int)method << "\n"; }

  // buffer the whole chunk remainder in memory
  uint32_t po=(uint32_t) (ftell(fp)-chunk_start_longint);
  uint32_t len = chunk_length-po;
  delete[] chunk_data;
  chunk_data = new char[len];
  if(fread(chunk_data,1,len,fp)<len) call_err();

  bool null_found=false;
  uint32_t po2;
  for(po2=po; po2<chunk_length && !null_found; po2++) {
    null_found = chunk_data[po2]==0;
  }
  if(!null_found) {
    cout << "Error: no null-terminating character found for the language tag\n";
    error_count++;
    return;
  }
  if(output) { 
    char *lang=new char[po2-po];
    strcpy(lang,chunk_data+po);
    cout << "    Language tag: \"" << lang << "\"\n";
    delete[] lang;
  }

  null_found=false;
  uint32_t po3;
  for(po3=po2; po3<chunk_length && !null_found; po3++) {
    null_found = chunk_data[po3]==0;
  }
  if(!null_found) {
    cout << "Error: no null-terminating character found for the translated keyword\n";
    error_count++;
    return;
  }
  if(output) { 
    char *keyword=new char[po3-po2];
    strcpy(keyword,chunk_data+po2);
    cout << "    Translated keyword: \"" << keyword << "\"\n";
    delete[] keyword;
  }

  if(compressed) {
    if(output) {

      output_ztext(chunk_data,len,"    Text: \"","\"\n",false);
    };
  }
  else {
    if(output) {
      chunkStreamInit();
      for( ; !chunk_stream_finished; ) {
        uint32_t len=chunkReadMorsel();
        char* texte=new char[len];
        strncpy(texte, chunk_data, len);
        cout.write((char *)texte,len);
        delete[] texte;
        cout << "\"\n";
      }
    }
  }
}

void handleICCP(bool output) {

  cout << "    Embedded International Color Consortium profile.\n";

  if(!readKeyword("    Profile name: \"",output)) return;

  unsigned char method;
  myfread2(1,&method);
  
  if(output) {
    cout << "    Compression method (0=zlib) : " << (int)method << "\n";
  }
  
  cout << "    Content: this version of PNGan does not support ICC interpretation.\n" ;
/*
  if(output) {
    // buffer the whole chunk remaider in memory
    uint32_t len = chunk_length-(ftell(fp)-chunk_start_longint);
    delete[] chunk_data;
    chunk_data = new char[len];
    if(fread(chunk_data,1,len,fp)<len) call_err();
    
    err... dunno what to do with this data: have to write/get an ICC decoder

  }
*/
}

void handleSRGB(bool output) {
  if(chunk_length!=1) {
    cout << "Error: should be 1 byte long\n";
    error_count++;
  }
  else {
    unsigned char ri;
    myfread3(1,&ri);
    if(output) { cout << "    Rendering intent = " << (int)ri << "\n"; }
    if(ri>3) {
      cout << "Error: value has no meaning\n";
      error_count++;
    }
    else {
      if(output) { cout << "    meaning: ";
      switch(ri) {
      case 0 : cout << "Perceputal\n"; break;
      case 1 : cout << "Relative colorimetric\n"; break;
      case 2 : cout << "Saturation\n"; break;
      case 3 : cout << "Absolute colorimetric\n"; break;
      } }
    }
  }
}

void handleUnknown(bool output) {
  cout << "Error: chunk name unknown\n";
  error_count++;
  bool is_name=true;
  bool a[4];
  for(int i=0; i<4; i++) {
    if((chunk_name[i]>='a' && chunk_name[i]<='z')) {
      a[i]=false;
    }
    else {
      if((chunk_name[i]>='A' && chunk_name[i]<='Z')) {
        a[i]=true;
      }
      else {
        is_name=false;
      }
    }
  }
  if(is_name) {
    cout << "  Analysis of the name: this chunk is " << (a[0] ? "Critical" : "Ancilliary" )
         << " , "       << (a[1] ? "public" : "private" )
         << " , 3rd letter should be uppercase and is: " << (a[2] ? "Uppercase" : "Lowercase" )
         << " , "       << (a[3] ? "unsafe to copy" : "safe to copy" )
         << "\n";
  }
  else {
    cout << "Error: not a valid name\n";
    error_count++;
  }
}


// Checks whether the order of chunks is correct

void checkOrder() {
  if(first_chunk) {
    first_chunk=false;
    
    if(strncmp(chunk_name,HEADER,4)!=0) {
      cout << "ERROR: first chunk is not HEADER\n";
      error_count++;
    }
    else {
      header_met=true;
    }
  }
  else {
    if(strncmp(chunk_name,HEADER,4)==0) {
      if(header_met) {
        cout << "ERROR: several HEADER chunks\n";
        error_count++;
      }
      else {
        cout << "ERROR: HEADER must be at the beginning\n";
        error_count++;
        header_met=true;
      }
    }
  }
  if(strncmp(chunk_name,PALETTE,4)==0) {
    if(palette_met) {
      cout << "ERROR: several palettes\n";
      error_count++;
    }
    else {
      palette_met=true;
    }
  }
  if(strncmp(chunk_name,DATA,4)==0) {
    if(data_ended &! data_error) {
      cout << "Error: DATA chunk should be contiguous\n";
      error_count++;
      data_error=true;
    }
    else {
      if(palette_used && !palette_met) {
        cout << "ERROR: palette needed before beginning of data\n";
        error_count++;
      }
      data_met=true;
    }
  }
  else {
    if(data_met & !data_ended) {
      data_ended=true;
    }
  }
  if(strncmp(chunk_name,BACKGROUND,4)==0) {
    if(background_met) {
      cout << "ERROR: background color defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && (palette_met || !palette_used) && !data_met)) {
        cout << "ERROR: background color must be after the header,\n"
             << "        after the palette (if any), and before the data\n";
        error_count++;
      }
      background_met=true;
    }
  }
  if(strncmp(chunk_name,GAMMA,4)==0) {
    if(gamma_met) {
      cout << "ERROR: gamma defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && !palette_met && !data_met)) {
        cout << "ERROR: GAMMA chunk must be after the header,\n"
             << "         before the palette (if any), and before the data\n";
      }
      gamma_met=true;
    }
  }
  if(strncmp(chunk_name,CHROMA,4)==0) {
    if(chroma_met) {
      cout << "ERROR: chromaticity defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && !palette_met && !data_met)) {
        cout << "ERROR: chromaticity must be after the header,\n"
             << "        before the palette (if any), and before the data\n";
        error_count++;
      }
      chroma_met=true;
    }
  }
  if(strncmp(chunk_name,HISTOGRAM,4)==0) {
    if(histogram_met) {
      cout << "ERROR: histogram defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && palette_met && !data_met)) {
        cout << "ERROR: histogramme must be after the palette and\n"
             << "        before the data\n";
        error_count++;
      }
      histogram_met=true;
    }
  }
  if(strncmp(chunk_name,PIXEL,4)==0) {
    if(pixel_met) {
      cout << "ERROR: physical pixel chunk defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && !data_met)) {
        cout << "ERROR: physical pixel chunk must be after the header\n"
             << "        and before the data\n";
        error_count++;
      }
      pixel_met=true;
    }
  }
  if(strncmp(chunk_name,TRANSPARENCY,4)==0) {
    if(transparency_met) {
      cout << "ERROR: transparency chunk defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && (palette_met || !palette_used) && !data_met)) {
        cout << "ERROR: transparency chunk must be after the header,\n"
             << "        after the palette (if any) and before the data\n";
        error_count++;
      }
      transparency_met=true;
    }
  }
  if(strncmp(chunk_name,BITS,4)==0) {
    if(bits_met) {
      cout << "ERROR: significant bits chunk defined several times\n";
      error_count++;
    }
    else {
      if(!(header_met && !palette_met && !data_met)) {
        cout << "ERROR: significant bits chunk must be after the header,\n"
             << "        before the palette (if any) and before the data\n";
        error_count++;
      }
      bits_met=true;
    }
  }
}

// long and boring procedure to call the right procedure

void handleChunk() {
  bool output=!text_only;

  // Critical
  if(strncmp(chunk_name,HEADER,4)==0)        { handleHeader(output); goto escape_pt; }
  if(strncmp(chunk_name,PALETTE,4)==0)       { handlePalette(output); goto escape_pt; }
  if(strncmp(chunk_name,DATA,4)==0)          { handleData(); goto escape_pt; }
  if(strncmp(chunk_name,END,4)==0)           { handleEnd(); goto escape_pt; }
  // Ancilliary
  // v1.0
  if(strncmp(chunk_name,BACKGROUND,4)==0)    { handleBackground(output); goto escape_pt; }
  if(strncmp(chunk_name,CHROMA,4)==0)        { handleChroma(output); goto escape_pt; }
  if(strncmp(chunk_name,GAMMA,4)==0)         { handleGamma(output); goto escape_pt; }
  if(strncmp(chunk_name,HISTOGRAM,4)==0)     { handleHistogram(output); goto escape_pt; }
  if(strncmp(chunk_name,PIXEL,4)==0)         { handlePixel(output); goto escape_pt; }
  if(strncmp(chunk_name,BITS,4)==0)          { handleBits(output); goto escape_pt; }
  output=true;
  if(strncmp(chunk_name,TEXT,4)==0)          { handleText(output); goto escape_pt; }
  output=!text_only;
  if(strncmp(chunk_name,TIME,4)==0)          { handleTime(output); goto escape_pt; }
  if(strncmp(chunk_name,TRANSPARENCY,4)==0)  { handleTransparency(output); goto escape_pt; }
  output=true;
  if(strncmp(chunk_name,ZTEXT,4)==0)         { handleZtext(output); goto escape_pt; }
  // v1.1
  if(strncmp(chunk_name,EMBEDDED_ICC,4)==0)  { handleICCP(output); goto escape_pt; }
  if(strncmp(chunk_name,SUGGESTED_PAL,4)==0) { goto escape_pt; }
  output=!text_only;
  if(strncmp(chunk_name,SRGB,4)==0)          { handleSRGB(output); goto escape_pt; }
  // v1.2
  if(strncmp(chunk_name,INTERNATIONAL,4)==0) { handleItext(output); goto escape_pt; }
  // extension (not handled)
  if(strncmp(chunk_name,OFFSET,4)==0)        { goto not_handled; }
  if(strncmp(chunk_name,PIX_CAL,4)==0)       { goto not_handled; }
  if(strncmp(chunk_name,GIFGCE,4)==0)        { goto not_handled; }
  if(strncmp(chunk_name,GIFAE,4)==0)         { goto not_handled; }
  if(strncmp(chunk_name,FRACTAL,4)==0)       { goto not_handled; }
  // deprecated
  if(strncmp(chunk_name,GIFTEXT,4)==0) {
    cout << "Warning: this chunk type (GIF Plain Text Extension)\n"
         << "          has been deprecated since version 1.1.0\n";
    goto escape_pt;
  }

  output=text_only;
  // If the chunk name is unknown :
  handleUnknown(output); goto escape_pt;

 not_handled:
  cout << "  This chunk type (registered in PNG extension 1.2.0),\n"
       << "  is not handled in this program\n";
  
 escape_pt: 
  ;
}
