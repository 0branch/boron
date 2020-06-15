/*
  Boron OpenGL Shader
  Copyright 2005-2010 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "glh.h"
#include "os.h"
#include "boron-gl.h"
#include "gl_atoms.h"
#include "shader.h"

#define CACHE_SHADERS


static int printInfoLog( UThread* ut, GLuint obj, int prog )
{
    GLint infologLength;
    GLint charsWritten;
    char* infoLog;

    if( prog )
        glGetProgramiv( obj, GL_INFO_LOG_LENGTH, &infologLength );
    else
        glGetShaderiv( obj, GL_INFO_LOG_LENGTH, &infologLength );

    if( infologLength > 0 )
    {
        infoLog = (char*) malloc( infologLength );

        if( prog )
            glGetProgramInfoLog( obj, infologLength, &charsWritten, infoLog );
        else
            glGetShaderInfoLog( obj, infologLength, &charsWritten, infoLog );

        //fprintf( stderr, "%s\n", infoLog );
        ur_error( ut, UR_ERR_SCRIPT, infoLog );

        free( infoLog );
    }
    else
    {
        ur_error( ut, UR_ERR_SCRIPT, prog ? "glLinkProgram failed"
                                          : "glCompileShader failed" );
    }
    return UR_THROW;
}


#if ! defined(USE_GLES) && ! defined(__ANDROID__)
#define REPLACE_ES_HEADER
#endif

#ifdef REPLACE_ES_HEADER
static const char* _skipEsHeader( const char* orig )
{
    const char* found;

    found = strstr( orig, "#version 310 es\n" );
    if( found )
        orig = found + 16;
    if( strncmp( orig, "precision", 9 ) == 0 )
    {
        if( (found = strstr( orig, ";" )) )
            orig = found + 2;
    }
    return orig;
}
#endif


/**
    Returns UR_OK/UR_THROW.
*/
static int compileProgram( UThread* ut, GLuint program,
                           const char* vert, const char* frag )
{
    GLint ok;
    GLuint vertexObj;
    GLuint fragmentObj;
#ifdef REPLACE_ES_HEADER
    const char* source[2];
#endif


    vertexObj   = glCreateShader( GL_VERTEX_SHADER );
    fragmentObj = glCreateShader( GL_FRAGMENT_SHADER );


#ifdef REPLACE_ES_HEADER
    // The extension is required until #version 430.
    source[0] = "#version 330\n"
                "#extension GL_ARB_explicit_uniform_location : enable\n";
    source[1] = _skipEsHeader( vert );
    glShaderSource( vertexObj, 2, (const GLchar**) source, 0 );
#else
    glShaderSource( vertexObj, 1, (const GLchar**) &vert, 0 );
#endif
    glCompileShader( vertexObj );
    glGetShaderiv( vertexObj, GL_COMPILE_STATUS, &ok );
    if( ! ok )
        return printInfoLog( ut, vertexObj, 0 );


#ifdef REPLACE_ES_HEADER
    // Using the same source[0] as vertex shader.
    source[1] = _skipEsHeader( frag );
    glShaderSource( fragmentObj, 2, (const GLchar**) source, 0 );
#else
    glShaderSource( fragmentObj, 1, (const GLchar**) &frag, 0 );
#endif
    glCompileShader( fragmentObj );
    glGetShaderiv( fragmentObj, GL_COMPILE_STATUS, &ok );
    if( ! ok )
        return printInfoLog( ut, fragmentObj, 0 );


    glAttachShader( program, vertexObj );
    glAttachShader( program, fragmentObj );

#ifdef CACHE_SHADERS
    // Necessary to retrieve compiled program binary.
    glProgramParameteri( program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                         GL_TRUE );
#endif

    /*
    glBindAttribLocation( program, 0, "bg_vertex" );
    glBindAttribLocation( program, 1, "bg_color" );
    glBindAttribLocation( program, 2, "bg_normal" );
    */

    glLinkProgram( program );
    glGetProgramiv( program, GL_LINK_STATUS, &ok );

    // These will actually go away when the program is deleted.
    glDeleteShader( vertexObj );
    glDeleteShader( fragmentObj );

#if 0
    glGetShaderiv( vertexObj, GL_DELETE_STATUS, &ok );
    fprintf( stderr, "KR vertex delete %d\n", ok );
    glGetShaderiv( fragmentObj, GL_DELETE_STATUS, &ok );
    fprintf( stderr, "KR fragment delete %d\n", ok );
#endif

    if( ! ok )
        return printInfoLog( ut, program, 1 );

    return UR_OK;
}


#ifdef CACHE_SHADERS
// From boron/eval/checksum.c
extern uint32_t checksum_crc32( const uint8_t* data, int byteCount );

static void shaderCacheFileName( UBuffer* str, const char* vert,
                                 const char* frag )
{
    char basename[32];
    sprintf( basename, "/vf-%08X-%08X",
             checksum_crc32( (uint8_t*) vert, (int) strlen(vert) ),
             checksum_crc32( (uint8_t*) frag, (int) strlen(frag) ) );

    ur_strAppendCStr( str, basename );
    ur_strTermNull( str );
}


#include "os_file.h"

/*
    Returns UR_OK/UR_THROW.
*/
static int loadCachedProgram( UThread* ut, GLuint program, const char* vert,
                              const char* frag )
{
    OSFileInfo fi;
    UBuffer path;
    const UCell* val;
    FILE* fp;
    uint8_t* data = NULL;
    GLsizei size;
    GLenum format;
    int ok;


    ur_strInit( &path, UR_ENC_UTF8, 0 );
    val = ur_ctxValueOfType( ur_threadContext(ut), UR_ATOM_SHADER_CACHE,
                             UT_FILE );
    if( val )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, val );
        ur_strAppend( &path, si.buf, si.it, si.end );

        shaderCacheFileName( &path, vert, frag );
    }

    if( path.used && ur_fileInfo( path.ptr.c, &fi, FI_Size /*| FI_Type*/ ) )
    {
        GLint status;

        //fprintf( stderr, "KR load program %5ld %s\n", fi.size, path.ptr.c );
        data = (uint8_t*) malloc( fi.size );
        fp = fopen( path.ptr.c, "rb" );
        if( ! fp )
            goto read_failed;

        size = (GLsizei) fread( data, 1, fi.size, fp );
        fclose( fp );

        if( size != fi.size )
            goto read_failed;

        format = *((GLenum*) data);
        glProgramBinary( program, format, data + sizeof(GLenum),
                         fi.size - sizeof(GLenum) );
        glGetProgramiv( program, GL_LINK_STATUS, &status );
        if( status )
        {
            ok = UR_OK;
        }
        else
        {
            fprintf(stderr, "glProgramBinary link failed; compiling instead\n");

            // Fallback to compiling from source code. The binary link can
            // fail if the GL driver was updated or if the GL stack is buggy
            // (e.g. in the Android emulator).
            ok = compileProgram( ut, program, vert, frag );

            // Delete the cached program. It will be re-created the next time
            // the program is run.
            remove( path.ptr.c );
        }
    }
    else
    {
        // Compile from source and save to cache file.
        ok = compileProgram( ut, program, vert, frag );
        if( path.used && ok == UR_OK )
        {
            GLsizei bytesWritten;

            glGetProgramiv( program, GL_PROGRAM_BINARY_LENGTH, &size );
            data = (uint8_t*) malloc( size + sizeof(GLenum) );
            glGetProgramBinary( program, size, &bytesWritten, &format,
                                data + sizeof(GLenum) );
            //fprintf( stderr, "KR cache program %5d %s\n",
            //         bytesWritten, path.ptr.c );
            fp = fopen( path.ptr.c, "wb" );
            if( fp )
            {
                *((GLenum*) data) = format;
                size = bytesWritten + sizeof(GLenum);
                if( fwrite( data, 1, size, fp ) != (size_t) size )
                    ok = ur_error( ut, UR_ERR_ACCESS,
                                   "Write to shader cache failed" );
                fclose( fp );
            }
        }
    }

cleanup:

    ur_strFree( &path );
    free( data );
    return ok;

read_failed:

    ok = ur_error( ut, UR_ERR_ACCESS, "Read from shader cache failed" );
    goto cleanup;
}
#endif


void destroyShader( Shader* sh )
{
    if( sh->program )
    {
        glDeleteProgram( sh->program );
        sh->program = 0;
    }
}


#define ARRAY_ENUM_OFF  3   // 0x8B4D - 0x8B4F
#define ARRAY_FLOAT_VEC2    (GL_FLOAT_VEC2 - ARRAY_ENUM_OFF)
#define ARRAY_FLOAT_VEC3    (GL_FLOAT_VEC3 - ARRAY_ENUM_OFF)
#define ARRAY_FLOAT_VEC4    (GL_FLOAT_VEC4 - ARRAY_ENUM_OFF)


/*
  Sets res to either a shader! or a context! whose first value is a shader!.
  Returns UR_OK/UR_THROW.
*/
int ur_makeShader( UThread* ut, const char* vert, const char* frag, UCell* res )
{
#define MAX_SHADER_PARAM    16      // LIMIT: User parameters per shader.
    Shader shad;
    Shader* sh;
    UIndex bufN;
    UBuffer* buf;
    int i;
    char name[ 128 ];
    ShaderParam param[ MAX_SHADER_PARAM ];
    ShaderParam* pi;
    GLsizei length;
    GLint size;
    GLenum type;
    GLint count = 0;


    shad.program = glCreateProgram();
#ifdef CACHE_SHADERS
    if( ! loadCachedProgram( ut, shad.program, vert, frag ) )
        return UR_THROW;
#else
    if( ! compileProgram( ut, shad.program, vert, frag ) )
        return UR_THROW;
#endif

    glGetProgramiv( shad.program, GL_ACTIVE_UNIFORMS, &count );

    // Collect non-gl parameters.

    pi = param;
    for( i = 0; i < count; ++i )
    {
        glGetActiveUniform( shad.program, i, 128, &length, &size, &type,
                            name );
        if( name[0] == 'g' && name[1] == 'l' && name[2] == '_' )
            continue;

        // Remove any "[0]" array suffix for Boron atom.
        if( name[ length-1 ] == ']' )
            length -= 3;

        pi->type     = type;
        pi->location = glGetUniformLocation( shad.program, name );
        pi->size     = size;
        pi->name     = ur_intern( ut, name, length );
        if( pi->name == UR_INVALID_ATOM )
            return UR_THROW;

        ++pi;
    }
    count = pi - param;

    if( count )
    {
        ShaderParam* pend;
        UBuffer* ctx;
        UCell* cval;

        // We have parameters - make context to hold shader & params.

        ctx = ur_makeContextCell( ut, count + 1, res );
        cval = ur_ctxAddWord( ctx, UR_ATOM_SHADER );

        pi = param;
        pend = param + count;
        while( pi != pend )
        {
            cval = ur_ctxAddWord( ctx, pi->name );
#if 0
            printf( "KR Shader Param %d type:0x%X size:%d\n",
                    (int) (pi - param), pi->type, pi->size );
#endif
            switch( pi->type )
            {
                case GL_FLOAT:
                    ur_setId( cval, UT_DOUBLE );
                    ur_double( cval ) = 0.0;
                    break;

                case GL_FLOAT_VEC2:
                case GL_FLOAT_VEC3:
                case GL_FLOAT_VEC4:
                    // Use custom non-GL enum for arrays.
                    if( pi->size > 1 )
                        pi->type -= ARRAY_ENUM_OFF;

                    ur_setId( cval, UT_VEC3 );
                    cval->vec3.xyz[0] =
                    cval->vec3.xyz[1] =
                    cval->vec3.xyz[2] = 0.0f;
                    break;

                case GL_INT:
                    ur_setId( cval, UT_INT );
                    ur_int( cval ) = 0;
                    break;

                case GL_INT_VEC2:
                case GL_INT_VEC3:
                //case GL_INT_VEC4:
                    ur_setId( cval, UT_COORD );
                    break;

                case GL_BOOL:
                    ur_setId( cval, UT_LOGIC );
                    //ur_logic( cval ) = 0;
                    break;

                case GL_SAMPLER_2D:
                case GL_SAMPLER_CUBE:
                case GL_SAMPLER_3D:
                case GL_SAMPLER_2D_SHADOW:
                    ur_setId( cval, UT_NONE );
                    ur_texId(cval) = 0;     // Expecting texture!.
                    break;

                case GL_FLOAT_MAT4:
                    ur_setId( cval, UT_NONE );
                    break;

                default:
                    ur_setId( cval, UT_NONE );
                    break;

                /*
                GL_BOOL_VEC2:
                GL_BOOL_VEC3:
                GL_BOOL_VEC4:
                GL_FLOAT_MAT2:
                GL_FLOAT_MAT3:
                GL_FLOAT_MAT4:
                GL_SAMPLER_2D_RECT:
                GL_SAMPLER_2D_RECT_SHADOW:
                */
            }

            ++pi;
        }
        ur_ctxSort( ctx );
    }

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );
    buf->type = UT_SHADER;
    buf->ptr.v = memAlloc( sizeof(Shader) +
                           sizeof(ShaderParam) * (count - 1) );

    sh = (Shader*) buf->ptr.v;
    sh->program     = shad.program;
    sh->paramCount  = count;

    if( count )
    {
        memCpy( sh->param, param, sizeof(ShaderParam) * count );

        // First context value will be set to shader!.
        res = ur_buffer( res->series.buf )->ptr.cell;
    }

    ur_initSeries( res, UT_SHADER, bufN );
    return UR_OK;
}


const Shader* shaderContext( UThread* ut, const UCell* cell,
                             const UBuffer** ctxPtr )
{
    const UBuffer* ctx;

    if( ur_is(cell, UT_CONTEXT) )
    {
        ctx = ur_bufferSer( cell );
        cell = ur_ctxCell( ctx, 0 );
        if( ! cell )
            return 0;
    }
    else
    {
        ctx = 0;
    }

    if( ur_is(cell, UT_SHADER) )
    {
        const Shader* sh = (const Shader*) ur_bufferSer( cell )->ptr.v;
        if( ctxPtr )
            *ctxPtr = ctx;
        return sh;
    }

    return 0;
}


GLuint ur_shaderProgram( UThread* ut, const UCell* cell )
{
    const Shader* sh = shaderContext( ut, cell, 0 );
    return sh ? sh->program : 0;
}


void setShaderUniforms( UThread* ut, const Shader* sh, const UBuffer* blk )
{
    USeriesIter si;
    const ShaderParam* pi   = sh->param;
    const ShaderParam* pend = sh->param + sh->paramCount;
    const UCell* cval = blk->ptr.cell;
    int texUnit = 0;

    es_matrixUsed = 0;

    while( pi != pend )
    {
        // There is no cell type checking or conversion here for speed.
        // If the user loads bad types then we'll just get garbage.

        ++cval;
        switch( pi->type )
        {
            case GL_FLOAT:
                glUniform1f( pi->location, ur_double(cval) );
                break;

            case ARRAY_FLOAT_VEC2:
                ur_seriesSlice( ut, &si, cval );
                assert( si.buf->form == UR_VEC_F32 );
                glUniform2fv( pi->location, (si.end - si.it) / 2,
                              si.buf->ptr.f + si.it );
                break;

            case ARRAY_FLOAT_VEC3:
                ur_seriesSlice( ut, &si, cval );
                assert( si.buf->form == UR_VEC_F32 );
                glUniform3fv( pi->location, (si.end - si.it) / 3,
                              si.buf->ptr.f + si.it );
                break;

            case ARRAY_FLOAT_VEC4:
                ur_seriesSlice( ut, &si, cval );
                assert( si.buf->form == UR_VEC_F32 );
                glUniform4fv( pi->location, (si.end - si.it) / 4,
                              si.buf->ptr.f + si.it );
                break;

            case GL_FLOAT_VEC2:
                glUniform2fv( pi->location, 1, cval->vec3.xyz );
                break;

            case GL_FLOAT_VEC3:
                glUniform3fv( pi->location, 1, cval->vec3.xyz );
                break;

            case GL_FLOAT_VEC4:
                glUniform4f( pi->location, cval->vec3.xyz[0],
                                           cval->vec3.xyz[1],
                                           cval->vec3.xyz[2], 1.0f );
                break;

            case GL_INT:
            case GL_BOOL:
                if( ur_is(cval, UT_LOGIC) )
                    glUniform1i( pi->location, ur_logic(cval) );
                else
                    glUniform1i( pi->location, ur_int(cval) );
                break;

            case GL_INT_VEC2:
                glUniform2i( pi->location,
                             cval->coord.n[0],
                             cval->coord.n[1] );
                break;

            case GL_INT_VEC3:
                glUniform3i( pi->location,
                             cval->coord.n[0],
                             cval->coord.n[1],
                             cval->coord.n[2] );
                break;

            //case GL_INT_VEC4:

            case GL_SAMPLER_2D:
            case GL_SAMPLER_2D_SHADOW:
                glActiveTexture( GL_TEXTURE0 + texUnit );
                glBindTexture( GL_TEXTURE_2D, ur_texId(cval) );
sampler_uniform:
                glUniform1i( pi->location, texUnit );
                ++texUnit;
                break;

            case GL_SAMPLER_3D:
                glActiveTexture( GL_TEXTURE0 + texUnit );
                glBindTexture( GL_TEXTURE_3D, ur_texId(cval) );
                goto sampler_uniform;

            case GL_SAMPLER_CUBE:
                glActiveTexture( GL_TEXTURE0 + texUnit );
                glBindTexture( GL_TEXTURE_CUBE_MAP, ur_texId(cval) );
                goto sampler_uniform;

            case GL_FLOAT_MAT4:
                /*
                fprintf( stderr, "sha %f,%f,%f\n    %f,%f,%f\n    %f,%f,%f\n",
                         matrixTop[0], matrixTop[1], matrixTop[2],
                         matrixTop[4], matrixTop[5], matrixTop[6],
                         matrixTop[8], matrixTop[9], matrixTop[10] );
                */
                if( pi->location == 0 )         // 0 = ULOC_TRANSFORM
                    es_matrixUsed |= 1;
                else if( pi->location == 2 )    // 2 = ULOC_MODELVIEW
                    es_matrixUsed |= 2;
                else if( ur_is(cval, UT_VECTOR) )
                {
                    // Assuming form == UR_VEC_F32 & used == 16.
                    const float* mat = ur_bufferSer(cval)->ptr.f;
                    glUniformMatrix4fv( pi->location, 1, GL_FALSE, mat );
                }
                break;
        }
        ++pi;
    }

    if( es_matrixUsed )
        es_matrixMod = 1;   // Trigger matrix uniform update for this shader.
}


/*
  Return texture unit (0-N) or -1 if name not found.
*/
int shaderTextureUnit( const Shader* sh, UAtom name )
{
    const ShaderParam* pi   = sh->param;
    const ShaderParam* pend = sh->param + sh->paramCount;
    int texUnit = 0;

    while( pi != pend )
    {
        switch( pi->type )
        {
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_3D:
            case GL_SAMPLER_2D_SHADOW:
                if( pi->name == name )
                    return texUnit;
                ++texUnit;
                break;
        }
        ++pi;
    }
    return -1;
}


#if 0
#if 1
void setUniform( UThread* ut, UAtom name, UCell* cell )
{
    GLint loc;

    loc = glGetUniformLocation( grState.shaderProg, ur_atomCStr(name, 0) );
    if( loc == -1 )
    {
        printf( "KR setUniform failed %d\n", grState.shaderProg );
        return;
    }

    switch( ur_type(cell) )
    {
        case UT_LOGIC:
            glUniform1i( loc, ur_logic(cell) );
            break;
        case UT_INT:
            glUniform1i( loc, ur_int(cell) );
            break;
    }
}
#else
void setUniform( UThread* ut, UCell* path, UCell* val )
{
    if( ur_is(path, UT_SELECT) )
    {
        UCell* cval;
        Shader* sh;
        ShaderParam* pi;
        ShaderParam* pend;

        cval = ur_buffer( ctx->ctx.valBlk )->ptr.cell;
        if( cval && ur_is(cval, UT_SHADER) )
        {
            sh = (Shader*) ur_resPtr( cval->series.n )->ptr;
            pi   = sh->param;
            pend = sh->param + sh->paramCount;
            while( pi != pend )
            {
                if( pi->name == sel )
                {
                    switch( pi->type )
                    {
                    case GL_INT:
                    case GL_BOOL:
                        glUniform1i( pi->location, ur_int(val) );
                        break;
                    }
                    return;
                }
                ++pi;
            }
        }
    }
}
#endif
#endif


// EOF
