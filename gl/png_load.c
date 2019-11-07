//============================================================================
//
// PNG Loader
//
//============================================================================


#include <png.h>
#include "boron-gl.h"
#include "os.h"
#ifdef __ANDROID__
#include "glv_asset.h"
#endif


#ifndef png_jmpbuf
#define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif


//#define PRINT_IMAGE

static int make_image( UThread* ut, png_structp png, png_infop info,
                       UCell* res )
{
    UBuffer* bin;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_type;
    png_bytepp row_pointers;

    png_get_IHDR( png, info, &width, &height, &bit_depth, &color_type,
                  &interlace_type, &compression_type, &filter_type );

#ifdef PRINT_IMAGE
    dprint( "\n%ldx%ld bit_depth: %d color_type: %d\n", width, height,
            bit_depth, color_type );
#endif

    if( bit_depth == 8 )
    {
        int png_bpp = bit_depth / 8;
        int format = UR_RAST_GRAY;

        if( color_type == PNG_COLOR_TYPE_PALETTE )
        {
            return ur_error( ut, UR_ERR_INTERNAL, "png palette not supported" );
        }
        else if( color_type == PNG_COLOR_TYPE_RGB )
        {
            png_bpp *= 3;
            format = UR_RAST_RGB;
        }
        else if( color_type == PNG_COLOR_TYPE_RGB_ALPHA )
        {
            png_bpp *= 4;
            format = UR_RAST_RGBA;
        }

        bin = ur_makeRaster( ut, format, width, height, res );
        if( bin->ptr.b )
        {
            png_uint_32 y;
            int png_bpl = width * png_bpp;
#define bpl     png_bpl
            //int bpl = orImageBytesPerLine(img);
            png_byte* dest = (png_byte*) ur_rastElem(bin);

            //dprint( "KR %ldx%ld depth: %d color: %d bpp: %d\n",
            //        width, height, bit_depth, color_type, png_bpp );

            row_pointers = png_get_rows( png, info );

#ifdef IMAGE_BOTTOM_AT_0
#define FOREACH_ROW for( y = height; y; )
#define ADJUST_Y    --y;
#else
#define FOREACH_ROW for( y = 0; y < height; ++y )
#define ADJUST_Y
#endif

            if( color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
            {
                // Drop Alpha.
                png_byte* rit;
                png_byte* rend;
                FOREACH_ROW
                {
                    ADJUST_Y
                    rit  = row_pointers[ y ];
                    rend = rit + png_bpl;
                    while( rit != rend )
                    {
                        *dest++ = *rit;
                        rit += 2;
                    }
                }
            }
            else
            {
                FOREACH_ROW
                {
                    ADJUST_Y
                    memCpy( dest, row_pointers[ y ], png_bpl );
                    dest += bpl;
#ifdef PRINT_IMAGE
                    png_bytep c, end;
                    c = row_pointers[ y ];
                    end = c + png_bpl;
                    for( ; c != end; ++c )
                    {
                        dprint( "%02x", *c );
                    }
                    dprint( "\n" );
#endif
                }
            }
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_INTERNAL,
                     "png bit_depth %d not supported", bit_depth );
}


typedef struct
{
    const unsigned char* buf;
    unsigned int len;
}
MemStream;


static void readMemStream( png_structp png, png_bytep data, png_size_t len )
{
    MemStream* st = (MemStream*) png_get_io_ptr( png );
    if( st->len < len )
    {
        png_error( png, "End of input memory buffer reached" );
        return;
    }
    memcpy( data, st->buf, len );
    st->buf += len;
    st->len -= len;
}


/**
  Generic PNG read function using the high-level png_read_png().

  If bufLen is zero then fpBuf is treated as a FILE*.
*/
static int load_png( UThread* ut, const void* fpBuf, unsigned int bufLen,
                     unsigned int sig_read, UCell* res )
{
    png_structp png;
    png_infop info;
    int ok;

    png = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
                                  /*
                                  (png_voidp) user_error_ptr,
                                  user_error_fn,
                                  user_warning_fn );
                                  */
    if( ! png )
        return 0;

    /* Allocate/initialize the memory for image information.  REQUIRED. */
    info = png_create_info_struct( png );
    if( ! info )
    {
        png_destroy_read_struct( &png, (png_infopp)NULL, (png_infopp)NULL );
        return 0;
    }

    if( setjmp( png_jmpbuf(png) ) )
    {
        /* If we get here, we had a problem reading the file */
        png_destroy_read_struct( &png, &info, (png_infopp) NULL );
        return 0;
    }

    if( bufLen )
    {
        MemStream stream;
        stream.buf = fpBuf;
        stream.len = bufLen;
        png_set_read_fn( png, (void*) &stream, readMemStream );
    }
    else
    {
        png_init_io( png, (FILE*) fpBuf ); /* Using standard C streams */
    }

    /* If we have already read some of the signature */
    png_set_sig_bytes( png, sig_read );

#if 0
    row_pointers = png_malloc(png_ptr, height*sizeof(png_bytep));
    for( int i=0; i<height, i++ )
        row_pointers[i]=png_malloc(png_ptr, width*pixel_size);
    png_set_rows(png_ptr, info_ptr, &row_pointers); 
#endif

    /* hilevel read */
    png_read_png( png, info, 
                  PNG_TRANSFORM_STRIP_16 |
                  PNG_TRANSFORM_PACKING |
                  PNG_TRANSFORM_EXPAND,
                  NULL );

    // At this point the image has been read (make_image calls png_get_IHDR).
    ok = make_image( ut, png, info, res );

    /* clean up after the read, and free any memory allocated - REQUIRED */
    png_destroy_read_struct( &png, &info, (png_infopp) NULL );

    return ok;
}


/*-cf-
    load-png
        filename    string!/file!
    return: raster!
    group: io
*/
CFUNC_PUB( cfunc_load_png )
{
    if( ur_is(a1, UT_FILE) || ur_is(a1, UT_STRING) )
    {
        int ok;
        const char* file = boron_cstr( ut, a1, 0 );
#ifdef __ANDROID__
        struct AssetFile af;
        if( ! glv_assetOpen( &af, file, "r" ) )
            return ur_error( ut, UR_ERR_ACCESS, "Could not open \"%s\"", file );
        ok = load_png( ut, af.fp, 0, 0, res );
        glv_assetClose( &af );
#else
        FILE* fp = fopen( file, "rb" );
        if( ! fp )
            return ur_error( ut, UR_ERR_ACCESS, "Could not open \"%s\"", file );
        ok = load_png( ut, fp, 0, 0, res );
        fclose( fp );
#endif
        return ok;
    }
    else if( ur_is(a1, UT_BINARY) )
    {
        UBinaryIter bi;
        int size;
        ur_binSlice( ut, &bi, a1 );
        size = bi.end - bi.it;
        if( size > 0 )
            return load_png( ut, bi.it, size, 0, res );
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "load-png expected binary!/string!/file!" );
}


#if 0
int png_loader( char* filename, uint8_t* sig, int sigLen )
{
    if( png_sig_cmp( sig, 0, sigLen ) == 0 )
    {
        FILE* fp = fopen( filename, "rb" );
        if( fp )
        {
            load_png( fp, 0 );
            fclose( fp );
        }
        return 1;
    }
    return 0;
}
#endif


/*EOF*/
