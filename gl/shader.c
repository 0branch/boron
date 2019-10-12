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


/**
    Returns UR_OK/UR_THROW.
*/
static int createShader( UThread* ut, Shader* sh,
                         const char* vert, const char* frag )
{
    GLint ok;
    GLuint vertexObj;
    GLuint fragmentObj;


    sh->program = glCreateProgram();
    vertexObj   = glCreateShader( GL_VERTEX_SHADER );
    fragmentObj = glCreateShader( GL_FRAGMENT_SHADER );


    glShaderSource( vertexObj, 1, (const GLchar**) &vert, 0 );
    glCompileShader( vertexObj );
    glGetShaderiv( vertexObj, GL_COMPILE_STATUS, &ok );
    if( ! ok )
        return printInfoLog( ut, vertexObj, 0 );


    glShaderSource( fragmentObj, 1, (const GLchar**) &frag, 0 );
    glCompileShader( fragmentObj );
    glGetShaderiv( fragmentObj, GL_COMPILE_STATUS, &ok );
    if( ! ok )
        return printInfoLog( ut, fragmentObj, 0 );


    glAttachShader( sh->program, vertexObj );
    glAttachShader( sh->program, fragmentObj );

#ifndef __ANDROID__
    // These will actually go away when the program is deleted.
    glDeleteShader( vertexObj );
    glDeleteShader( fragmentObj );
#endif

#if 0
    glGetShaderiv( vertexObj, GL_DELETE_STATUS, &ok );
    fprintf( stderr, "KR vertex delete %d\n", ok );
    glGetShaderiv( fragmentObj, GL_DELETE_STATUS, &ok );
    fprintf( stderr, "KR fragment delete %d\n", ok );
#endif

    /*
    glBindAttribLocation( sh->program, 0, "bg_vertex" );
    glBindAttribLocation( sh->program, 1, "bg_color" );
    glBindAttribLocation( sh->program, 2, "bg_normal" );
    */
    
    glLinkProgram( sh->program );
    glGetProgramiv( sh->program, GL_LINK_STATUS, &ok );
    if( ! ok )
        return printInfoLog( ut, sh->program, 1 );

    return UR_OK;
}


void destroyShader( Shader* sh )
{
    if( sh->program )
    {
        glDeleteProgram( sh->program );
        sh->program = 0;
    }
}


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

    if( ! createShader( ut, &shad, vert, frag ) )
        return UR_THROW;

    glGetProgramiv( shad.program, GL_ACTIVE_UNIFORMS, &count );

    // Collect non-gl parameters.

    pi = param;
    for( i = 0; i < count; ++i )
    {
        glGetActiveUniform( shad.program, i, 128, &length, &size, &type,
                            name );
        if( name[0] == 'g' && name[1] == 'l' && name[2] == '_' )
            continue;

        pi->type     = type;
        pi->location = glGetUniformLocation( shad.program, name );
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
            printf( "KR Shader Param %d is type %d\n",
                    (int) (pi - param), pi->type );
#endif
            switch( pi->type )
            {
                case GL_FLOAT:
                    ur_setId( cval, UT_DOUBLE );
                    ur_double( cval ) = 0.0;
                    break;

                case GL_FLOAT_VEC2:
                case GL_FLOAT_VEC3:
                //case GL_FLOAT_VEC4:
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


void setShaderUniforms( const Shader* sh, const UBuffer* blk )
{
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
                glUniform1i( pi->location, texUnit );
                ++texUnit;
                break;

            //case GL_SAMPLER_3D:

            case GL_FLOAT_MAT4:
                /*
                fprintf( stderr, "sha %f,%f,%f\n    %f,%f,%f\n    %f,%f,%f\n",
                         matrixTop[0], matrixTop[1], matrixTop[2],
                         matrixTop[4], matrixTop[5], matrixTop[6],
                         matrixTop[8], matrixTop[9], matrixTop[10] );
                */
                if( pi->location == 0 )     // 0 = ULOC_TRANSFORM
                    es_matrixUsed = 1;
                //glUniformMatrix4fv( pi->location, 1, GL_FALSE, matrixTop );
                break;
        }
        ++pi;
    }
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
