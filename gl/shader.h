#ifndef SHADER_H
#define SHADER_H
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


typedef struct
{
    GLenum type;
    GLint  location;
    UAtom  name;
    uint16_t size;
}
ShaderParam;


typedef struct
{
    GLuint program;
    int    paramCount;
    ShaderParam param[1];
}
Shader;


int  ur_makeShader( UThread*, const char* vert, const char* frag, UCell* );
void destroyShader( Shader* );
//void loadShader( UThread*, UCell* );
//void setUniform( UThread*, UAtom name, UCell* );
void setShaderUniforms( UThread*, const Shader*, const UBuffer* );
int  shaderTextureUnit( const Shader*, UAtom name );
const Shader* shaderContext( UThread*, const UCell*, const UBuffer** ctxPtr );


#endif /*SHADER_H*/
