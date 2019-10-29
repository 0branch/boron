#include "boron.h"

int  aud_startup() { return 1; }
void aud_shutdown() {}
void aud_pauseProcessing( int paused ) { (void) paused; }
int  aud_playSound( const UCell* sound ) { (void) sound; return 0; }
void aud_playMusic( const char* file ) { (void) file; }
void aud_stopMusic() {}
void aud_setSoundVolume( float vol ) { (void) vol; }
void aud_setMusicVolume( float vol ) { (void) vol; }

CFUNC_PUB( cfunc_buffer_audio )
{
    (void) ut;
    (void) a1;
    ur_setId(res, UT_INT);
    ur_int(res) = -1;
    return UR_OK;
}
