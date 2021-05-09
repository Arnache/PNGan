# PNGan

Command-line tool that analyses the structure of a PNG file and shows the metadata.
Does not decode the image data.

Should compile fine on all three major OS.
You must have zlib installed.

The output charset is UTF8, which means your system terminal should be set to this charset. 
Non-ascii characters are only printed when decoding tEXt, zTXt or ITXt chunks, or on faulty keywords.
UTF8 has been the default on most Linux systems for a while. This is still not the case on the Windows console. Windows cygwin should work fine. On Mac, I do not remember if it is the default, but you can surely set it in the options.

See notes.txt for version notes

Example of output

`> PNGan testing.png`

    PngAn v0.961

    Analysis of file testing.png

    - Signature (first 8 bytes) : 137 80 78 71 13 10 26 10
      correct

    - Chunk IHDR (size = 13 bytes)
        Width: 300
        Height: 200
        Bit depth: 8
        Color type: 2 (b010)
        Compression: 0
        Filter: 0
        Interlace: 1

      Interpretation: no palette, color used, no alpha channel
      meaning: True color with 256 levels of R, G and B
      Interlace: Adam7

    - Chunk IDAT (size = 8192 bytes)

    - Chunk IDAT (size = 8192 bytes)

    - Chunk IDAT (size = 8192 bytes)

    - Chunk IDAT (size = 8192 bytes)

    - Chunk IDAT (size = 3638 bytes)

    - Chunk zTXt (size = 24 bytes)
        Compressed textual data, latin-1 encoded.
        Keyword: "Comment"
        Compression method (0=zlib) : 0
        Text: "à é À ?"

    - Chunk IEND (size = 0 bytes)

    Image data: 36406 bytes in 5 IDAT chunks

    Analysis finished.

    File looks OK.
    (No image decoding attempted.)


Example of compilation commands:

On Linux (gcc)

`g++ -std=c++11 -Wall --pedantic-errors PNGan.cc -lz -o PNGan`

On Mac (clang)

`clang++ -std=c++11 PNGan.cc -lz -o PNGan -Wall`

On Windows (via cygwin)

same as Linux

On Windows (Visual C++)

(...)

