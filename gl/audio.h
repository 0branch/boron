#ifndef AUDIO_H
#define AUDIO_H
/*
  Boron OpenAL Module
  Copyright 2005-2011 Karl Robillard

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


#ifdef __cplusplus
extern "C" {
#endif

extern int  aud_startup();
extern void aud_shutdown();
extern int  aud_playSound( const UCell* );
extern void aud_playMusic( const char* file );
extern void aud_stopMusic();
extern void aud_setSoundVolume( float );
extern void aud_setMusicVolume( float );

#ifdef __cplusplus
}
#endif


#endif /* AUDIO_H */
