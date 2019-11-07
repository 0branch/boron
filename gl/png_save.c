//============================================================================
//
// PNG Writer
//
//============================================================================


#include <png.h>
#include "boron-gl.h"


static int save_png( FILE* fp, const RasterHead* rast )
{
    png_structp png;
    png_infop info;
    png_bytep row_pointer;
    int color_type;
    int bpl;
    int i;

    png = png_create_write_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    if( ! png )
        return 0;

    info = png_create_info_struct( png );
    if( ! info )
    {
        png_destroy_write_struct( &png, 0 );
        return 0;
    }

    if( setjmp( png_jmpbuf( png ) ) )
    {
error:
        /* If we get here, we had a problem writing the file */
        png_destroy_write_struct( &png, &info );
        return 0;
    }

    png_init_io( png, fp ); /* Using standard C streams */

    /*
    png_set_write_fn(png, (void*) user_io_ptr,user_write_fn,user_io_flush);
    */
#if 0
    // Set up info for write (func calls png_set_IHDR).
    func( png, info, data );
    int png_transforms = PNG_TRANSFORM_IDENTITY;
    png_write_png( png, info, png_transforms, 0 );
#else

    /*
      PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA,
      PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
      or PNG_COLOR_TYPE_RGB_ALPHA.
    */
    switch( rast->format )
    {
        case UR_RAST_GRAY:
            bpl = rast->width;
            color_type = PNG_COLOR_TYPE_GRAY;
            break;

        case UR_RAST_RGB:
            bpl = rast->width * 3;
            color_type = PNG_COLOR_TYPE_RGB;
            break;

        case UR_RAST_RGBA:
            bpl = rast->width * 4;
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            break;

        default: goto error;
    }

    png_set_IHDR( png, info, rast->width, rast->height, 8, color_type,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT );

    png_write_info( png, info );

    row_pointer = (png_byte*) (rast + 1);
#ifdef IMAGE_BOTTOM_AT_0
    row_pointer += (rast->height - 1) * bpl;
#endif
    for( i = 0; i < rast->height; i++ )
    {
        png_write_row( png, row_pointer );
#ifdef IMAGE_BOTTOM_AT_0
        row_pointer -= bpl;
#else
        row_pointer += bpl;
#endif
    }

    png_write_end( png, info );
#endif

    png_destroy_write_struct( &png, &info );
    return 1;
}


/*-cf-
    save-png
        file    string!/file!
        raster  raster!
    return: unset!
    group: io
*/
CFUNC_PUB( cfunc_save_png )
{
    FILE* fp;
    const char* file;
    const UCell* a2 = a1 + 1;
    int ok;

    ur_setId(res, UT_UNSET);

    if( (ur_is(a1, UT_FILE) || ur_is(a1, UT_STRING)) &&
        ur_is(a2, UT_RASTER) )
    {
        file = boron_cstr( ut, a1, 0 );

        if( ! boron_requestAccess( ut, "Save PNG \"%s\"", file ) )
            return UR_THROW;

        fp = fopen( file, "wb" );
        if( ! fp )
            return ur_error( ut, UR_ERR_ACCESS, "Could not open \"%s\"", file );
        ok = save_png( fp, ur_rastHead(a2) );
        fclose( fp );
        return ok ? UR_OK : ur_error( ut, UR_ERR_ACCESS, "save-png failed" );
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "save-png expected string!/file! raster!" );
}


/*EOF*/
