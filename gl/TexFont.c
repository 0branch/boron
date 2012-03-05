/*============================================================================
//
// Load textured font using Freetype 2.
//
//==========================================================================*/


/*
  TODO:
   * Automatically compute best image size.
   * Pack glyphs more efficiently.
   * Speed up txf_kerning().
*/


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TexFont.h"
#include <ft2build.h>
#include FT_FREETYPE_H


//#define DUMP      1

#define FT_PIXELS(x)    (x >> 6)

#define GLYPHS(tf)      ((TexFontGlyph*) (tf + 1))
#define TABLE(tf)       (((uint16_t*) tf) + tf->table_offset)
#define KERN(tf)        (((int16_t*) tf) + tf->kern_offset)


//----------------------------------------------------------------------------


/**
  Returns the TexFontGlyph for a glyph or zero if it is not available.
  Automatically substitutes uppercase letters with lowercase if
  uppercase not available (and vice versa).
*/
TexFontGlyph* txf_glyph( const TexFont* tf, int c )
{
    uint16_t n;
    uint16_t* table = TABLE(tf);
    int min_glyph = tf->min_glyph;
    int max_glyph = min_glyph + tf->table_size;

    if( (c >= min_glyph) && (c < max_glyph) )
    {
        n = table[ c - min_glyph ];
        if( n < 0xffff )
            return GLYPHS(tf) + n;

        if( (c >= 'a') && (c <= 'z') )
        {
            c -= 'a' - 'A';     // toupper
            if( (c >= min_glyph) && (c < max_glyph) )
                return GLYPHS(tf) + table[ c - min_glyph ];
        }
        else if( (c >= 'A') && (c <= 'Z') )
        {
            c += 'a' - 'A';     // tolower
            if( (c >= min_glyph) && (c < max_glyph) )
                return GLYPHS(tf) + table[ c - min_glyph ];
        }
    }
    return 0;
}


int txf_kerning( const TexFont* tf, const TexFontGlyph* left,
                                    const TexFontGlyph* right )
{
    const int16_t* table; 
    if( left->kern_index > -1 )
    {
        table = KERN(tf) + left->kern_index;
        while( *table )
        {
            /*
            // If table sorted...
            if( right->c < *table )
                break;
            */
            if( right->c == *table )
                return table[1];
            table += 2;
        }
    }
    return 0;
}


static void swap16( uint16_t* it, uint16_t* end )
{
    uint8_t* bp;
    while( it != end )
    {
        bp = (uint8_t*) it;
        *it++ = (bp[0] << 8) | bp[1];
    }
}


/**
  Swap endianess of TexFont data if not native.
*/
void txf_swap( TexFont* tf )
{
    if( tf && (tf->glyph_offset == (sizeof(TexFont) << 8))  )
    {
        TexFontGlyph* tgi;
        TexFontGlyph* end;
        uint16_t* it;

        it = &tf->glyph_offset;
        swap16( it, it + (sizeof(TexFont) / sizeof(uint16_t)) );

        tgi = GLYPHS(tf);
        end = tgi + tf->glyph_count;
        while( tgi != end )
        {
            it = &tgi->c;
            swap16( it, it + 4 );
            ++tgi;
        }

        it = TABLE(tf);
        swap16( it, it + tf->table_size );

        it = (uint16_t*) KERN(tf);
        swap16( it, it + tf->kern_size );
    }
}


/**
  Change the texture position of all glyphs.
*/
void txf_moveGlyphs( TexFont* tf, int dx, int dy )
{
    TexFontGlyph* tgi = GLYPHS(tf);
    TexFontGlyph* end = tgi + tf->glyph_count;
    while( tgi != end )
    {
        tgi->x += dx;
        tgi->y += dy;
        ++tgi;
    }
}


//----------------------------------------------------------------------------


#if DUMP
static void dumpCharMaps( FT_Face face )
{
    FT_CharMap charmap;
    int n;

    printf( "  CharMaps %d [\n", face->num_charmaps );
    for( n = 0; n < face->num_charmaps; n++ )
    {
        long enc;
        charmap = face->charmaps[ n ];
        enc = charmap->encoding;
        printf( "    %lx (%c%c%c%c)\n", enc,
                 (char) ((enc >> 24) & 0xff),
                 (char) ((enc >> 16) & 0xff),
                 (char) ((enc >> 8) & 0xff),
                 (char) (enc & 0xff) );
    }
    printf( "  ]\n" );
}
#endif


static void blitGlyphToBitmap( FT_Bitmap* src, TexFontImage* dst, int x, int y )
{
    unsigned char* s;
    unsigned char* d;
    unsigned char* dend;
    int r;

    s = src->buffer;
    d = dst->pixels + (y * dst->pitch) + x;
    dend = dst->pixels + (dst->height * dst->pitch);

    r = src->rows;
    while( r && (d < dend) )
    {
        memcpy( d, s, src->width );
        s += src->pitch;
        d += dst->pitch;
        r--;
    }
}


static FT_Error _renderGlyph( TexFontImage* img, FT_GlyphSlot glyph,
                              int x_offset, int y_offset, int antialias )
{
    /* first, render the glyph into an intermediate buffer */
    if( glyph->format != ft_glyph_format_bitmap )
    {
        FT_Error error = FT_Render_Glyph( glyph,
                antialias ? ft_render_mode_normal : ft_render_mode_mono );
        if( error )
            return error;
    }
 
#if 0
    printf( "\nKR target offset %dx%d\n", x_offset, y_offset );
    printf( "KR bitmap left/top %d %d\n", glyph->bitmap_left, glyph->bitmap_top );
    printf( "KR metrics %ldx%ld %ldx%ld %ld\n", 
            FT_PIXELS(glyph->metrics.width),
            FT_PIXELS(glyph->metrics.height),
            FT_PIXELS(glyph->metrics.horiBearingX),
            FT_PIXELS(glyph->metrics.horiBearingY),
            FT_PIXELS(glyph->metrics.horiAdvance) );
#endif
 
    // Then, blit the image to the target surface
    // Ajusting by bitmap_top to pretty up the vertical alignment.
    blitGlyphToBitmap( &glyph->bitmap, img,
                       x_offset /*+ glyph->bitmap_left*/,
                       y_offset - glyph->bitmap_top );

    return 0;
}


/*
   Returns index into table or -1 if there is no kerning for left_glyph.
*/
static int _loadKerning( TexFont* tf, int space,
                         FT_Face face, FT_UInt left_glyph,
                         const uint8_t* codes )
{
    FT_Vector kern;
    FT_UInt right_glyph;
    const uint8_t* ci;
    int16_t* ktable = KERN(tf);
    int16_t* entry;
    uint32_t start = tf->kern_size;

    for( ci = codes; *ci != '\0'; ++ci )
    {
        right_glyph = FT_Get_Char_Index( face, *ci );
        if( right_glyph == 0 )
            continue;

        FT_Get_Kerning( face, left_glyph, right_glyph,
                        FT_KERNING_DEFAULT,
                        //FT_KERNING_UNFITTED, FT_KERNING_UNSCALED
                        &kern );

        // NOTE: kern.y is ignored since is it almost always zero.
        if( kern.x /*|| kern.y*/ )
        {
#ifdef DUMP
            printf(" %c %ld %ld\n", *ci, FT_PIXELS(kern.x), FT_PIXELS(kern.y));
#endif
            if( space < 3 )
            {
                ktable[ tf->kern_size ] = 0;
                fprintf( stderr, "txf_build: Kerning table full\n" );
                return -1;
            }

            entry = ktable + tf->kern_size;
            tf->kern_size += 2;
            space -= 2;

            entry[0] = *ci;
            entry[1] = FT_PIXELS(kern.x);
        }
    }

    if( tf->kern_size > start )
    {
        // Terminate entries.
        ktable[ tf->kern_size ] = 0;
        ++tf->kern_size;
        return start;
    }

    return -1;
}


/**
  \param file       Filename of TrueType font to load.
  \param index      File face index.
  \param codes      Null terminated Latin-1 string of character glyphs to
                    generate.
  \param img        Destination image for rendered glyphs.
  \param psize      Pixel size of font to render.
  \param gap        Number of pixels to leave between rows on image.
  \param antialias  Non-zero to antialias.

  \return TexFont pointer or zero if fails.
*/
TexFont* txf_build( const char* file, int index, const uint8_t* codes,
                    TexFontImage* img, int psize, int gap, int antialias )
{
    FT_Library library;
    FT_Face face;
    FT_Error error;
    FT_GlyphSlot glyph;
    FT_Size size;
    FT_F26Dot6 start_x;
    FT_F26Dot6 step_y;
    int x, y;
    int nextX;
    int glyph_index;
    int gmin, gmax;
    int ch;
    int kern_space;
    const uint8_t* ci;
    uint16_t* table;
    TexFontGlyph* tgi;
    TexFont* txf = 0;
    int count;
    int loadFailed = 0;


    error = FT_Init_FreeType( &library );
    if( error )
        return 0;

    error = FT_New_Face( library, file, index, &face );
    if( error )
        goto cleanup_lib;


#if DUMP
#define EM_PIX(n)   n,FT_PIXELS(n)
    printf( "FT_Face [\n" );
    printf( "  family_name:  \"%s\"\n", face->family_name );
    printf( "  style_name:   \"%s\"\n", face->style_name );
    printf( "  num_glyphs:   %ld\n",    face->num_glyphs );
    printf( "  units_per_EM: %d (%d)\n", EM_PIX(face->units_per_EM) );
    printf( "  ascender:     %d (%d)\n", EM_PIX(face->ascender) );
    printf( "  descender:    %d (%d)\n", EM_PIX(face->descender) );
    printf( "  height:       %d (%d)\n", EM_PIX(face->height) );
    dumpCharMaps( face );
    printf( "]\n" );
#endif


    error = FT_Set_Pixel_Sizes( face, psize, psize );
    if( error )
        goto cleanup;

    glyph = face->glyph;
    size  = face->size;


    // Count glyphs and find the high & low.
    gmin = 0xffff;
    gmax = 0;
    ci = codes;
    while( *ci )
    {
        ch = *ci++;
        if( ch < gmin )
            gmin = ch;
        if( ch > gmax )
            gmax = ch;
    }
    count = ci - codes;


    // Most glyphs have no kerning adjustment, and those with it have only
    // a handful (at least at small font sizes).
    kern_space = count * 7;     // 7 = 3 glyphs + terminator

    txf = (TexFont*) malloc(  sizeof(TexFont) +
                             (sizeof(TexFontGlyph) * count) +
                             (sizeof(uint16_t) * (gmax - gmin + 1)) +
                             (sizeof(int16_t) * kern_space) );

    txf->glyph_offset = sizeof(TexFont);
    txf->glyph_count  = count;
    txf->table_offset = (txf->glyph_offset + (sizeof(TexFontGlyph) * count))
                            / sizeof(uint16_t);
    txf->table_size   = gmax - gmin + 1;
    txf->kern_offset  = txf->table_offset + txf->table_size;
    txf->kern_size    = 0;

    //txf.max_ascent  = size->metrics.y_ppem;
    txf->max_ascent  =  face->ascender  * psize / face->units_per_EM;
    txf->max_descent = -face->descender * psize / face->units_per_EM;

    txf->min_glyph   = gmin;
    txf->tex_width   = img->pitch;
    txf->tex_height  = img->height;

    table = TABLE(txf);
    memset( table, 0xff, sizeof(uint16_t) * txf->table_size );


    // Clear bitmap.
    memset( img->pixels, 0, img->height * img->pitch );


    step_y = (face->height * psize / face->units_per_EM) + gap;
             //size->metrics.y_ppem + gap;

    start_x = gap;
    x = start_x;
    y = step_y;

    count = 0;
    tgi = GLYPHS(txf);
    ci = codes;
    while( *ci )
    {
        ch = *ci++;

        glyph_index = FT_Get_Char_Index( face, ch );
        if( glyph_index == 0 )
        {
            fprintf( stderr, "txf_build: Code 0x%x (%c) is undefined\n",
                     ch, (char) ch );
            continue;
        }

        error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
        if( error )
        {
            ++loadFailed;
            continue;
        }

        nextX = x + FT_PIXELS(glyph->metrics.width) + gap;

        if( nextX > img->width )
        {
            x  = start_x;
            y += step_y;

            //if( (y + step_y) >= img->rows )
            if( y >= img->height )
            {
                fprintf( stderr, "txf_build: Texture too small\n" );
                break;
            }

            nextX = x + FT_PIXELS(glyph->metrics.width) + gap;
        }

        _renderGlyph( img, glyph, x, y, antialias );

        tgi->c       = ch;
        tgi->width   = FT_PIXELS(glyph->metrics.width);
        tgi->height  = FT_PIXELS(glyph->metrics.height);
        tgi->xoffset = FT_PIXELS(glyph->metrics.horiBearingX);
        tgi->yoffset = FT_PIXELS(glyph->metrics.horiBearingY) - tgi->height;
        tgi->advance = FT_PIXELS(glyph->metrics.horiAdvance);
        tgi->_pad    = 0;  // Initialize entire struct in case it gets saved.
        tgi->x       = x /*+ tgi->xoffset*/;
#if 0
        tgi->y       = img->height - y + tgi->yoffset;
#else
        tgi->y       = y - tgi->yoffset;
#endif

#ifdef DUMP
            printf( "Glyph %c %d,%d %d,%d %d %d,%d\n", ch,
                    tgi->width, tgi->height,
                    tgi->xoffset, tgi->yoffset,
                    tgi->advance, tgi->x, tgi->y );
#endif

        table[ ch - txf->min_glyph ] = count++;

        if( FT_HAS_KERNING( face ) )
        {
#ifdef DUMP
            printf( "Kern %c\n", ch );
#endif
            tgi->kern_index = _loadKerning( txf,
                                            kern_space - txf->kern_size,
                                            face, glyph_index, codes );
        }
        else
        {
            tgi->kern_index = -1;
        }

        ++tgi;
        x = nextX;
    }

    if( loadFailed )
        fprintf( stderr, "txf_build: Failed to load %d glyphs.\n", loadFailed );

    txf->glyph_count = count;

cleanup:

    FT_Done_Face( face );

cleanup_lib:

    FT_Done_FreeType( library );

    return txf;
}


int txf_width( const TexFont* tf, const uint8_t* it, const uint8_t* end )
{
    TexFontGlyph* prev = 0;
    TexFontGlyph* tgi;
    int width = 0;

    while( it != end )
    {
        if( *it == '\n' )
            break;
        if( *it == ' ' )
        {
            tgi = txf_glyph( tf, ' ' );
            if( tgi )
                width += tgi->advance;
            prev = 0;
        }
        else
        {
            tgi = txf_glyph( tf, *it );
            if( tgi )
            {
                width += tgi->advance;
                if( prev )
                    width += txf_kerning( tf, prev, tgi );
                prev = tgi;
            }
        }
        ++it;
    }

    return width;
}


/*
   Returns character index under x or -1 if x is not inside the string area.
*/
int txf_charAtPixel( const TexFont* tf, const uint8_t* it, const uint8_t* end,
                     int x )
{
    const uint8_t* start = it;
    TexFontGlyph* prev = 0;
    TexFontGlyph* tgi;
    int width = 0;

    if( x < 0 )
        return -1;

    while( it != end )
    {
        if( *it == '\n' )
            break;
        if( *it == ' ' )
        {
            tgi = txf_glyph( tf, ' ' );
            if( tgi )
                width += tgi->advance;
            prev = 0;
        }
        else
        {
            tgi = txf_glyph( tf, *it );
            if( tgi )
            {
                width += tgi->advance;
                if( prev )
                    width += txf_kerning( tf, prev, tgi );
                prev = tgi;
            }
        }
        if( x <= width )
            return it - start;
        ++it;
    }

    return -1;
}


int txf_lineSpacing( const TexFont* tf )
{
    return tf->max_ascent + (tf->max_ascent / 2);
}


/*EOF*/
