#ifndef TEXFONT_H
#define TEXFONT_H


#include <stdint.h>


typedef struct
{
    void*    pixels;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
}
TexFontImage;


typedef struct
{
    uint16_t c;
    int16_t  x;
    int16_t  y;
    int16_t  kern_index;
    uint8_t  width;
    uint8_t  height;
    int8_t   xoffset;
    int8_t   yoffset;
    int8_t   advance;
    int8_t   _pad;
}
TexFontGlyph;


typedef struct
{
    uint16_t glyph_offset;      // Array of TexFontGlyph.
    uint16_t glyph_count;
    uint16_t table_offset;      // Lookup table; array of uint16_t.
    uint16_t table_size;
    uint16_t kern_offset;       // Kerning info; array of int16_t;
    uint16_t kern_size;

    int16_t  max_ascent;
    int16_t  max_descent;
    int16_t  min_glyph;
    uint16_t tex_width;
    uint16_t tex_height;
}
TexFont;


#ifdef __cplusplus
extern "C" {
#endif

extern TexFont* txf_build( const char* file, int index, const uint8_t* codes,
                           TexFontImage* img, int psize,
                           int gap, int antialias );
extern TexFontGlyph* txf_glyph( const TexFont*, int c );
extern int  txf_kerning( const TexFont*, const TexFontGlyph* left,
                         const TexFontGlyph* right );
extern void txf_swap( TexFont* );
extern void txf_moveGlyphs( TexFont*, int dx, int dy );
extern int  txf_width( const TexFont*, const uint8_t* it, const uint8_t* end );
extern int  txf_lineSpacing( const TexFont* );

#ifdef __cplusplus
}
#endif


#define txf_byteSize(tf)    ((tf->kern_offset + tf->kern_size) * 2)


#endif  /*TEXFONT_H*/
