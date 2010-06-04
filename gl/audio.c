/*
  Boron OpenAL Module
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


//#define USE_ALUT    1


#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__

#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#ifdef USE_ALUT
#include <ALUT/alut.h>
#endif

#elif defined(_WIN32)

#include <al.h>
#include <alc.h>
#ifdef USE_ALUT
#include <alut.h>
#endif

#else

#include <AL/al.h>
#include <AL/alc.h>
#ifdef USE_ALUT
#include <AL/alut.h>
#endif

#endif

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "os.h"
#include "boron.h"
#include "audio.h"


void aud_stopMusic();


#define SAMPLE_ID(sd)   ur_int(sd)

#define FX_COUNT        14
#define AMBIENT_COUNT   2

#define SOURCE_COUNT    FX_COUNT + AMBIENT_COUNT


static int _audioUp = 0;
#ifndef USE_ALUT
static ALCdevice*  _adevice  = 0;
static ALCcontext* _acontext = 0;
#endif
static ALuint _asource[ SOURCE_COUNT ];


#define MUSIC_BUFFERS       3
static ALuint  _musicSource = 0;
static ALfloat _musicGain = 1.0f;
static ALuint  _musicBuffers[ MUSIC_BUFFERS ];

static FILE* _bgStream = 0;
static OggVorbis_File _vf;
static vorbis_info* _vinfo;

#if BYTE_ORDER == LITTLE_ENDIAN
#define OGG_BIG_ENDIAN  0
#else
#define OGG_BIG_ENDIAN  1
#endif

/* 
   OGG_BUFFER_SIZE is set to hold one second of data in each music buffer.
   If aud_update() is not called before all buffers are played then OpenAL
   will stop the sound source.
*/
#define OGG_BUFFER_SIZE  (44100 * 2)
static char _oggPCM[ OGG_BUFFER_SIZE ];

/*
  Returns false if all data has been read or an error occurs.
*/
static int _readOgg( ALuint buffer, OggVorbis_File* vf, vorbis_info* vinfo )
{
    int stream;
    ALint format;
    unsigned int count = 0;

    while( count < OGG_BUFFER_SIZE )
    {
        int amt = ov_read( vf, _oggPCM + count, OGG_BUFFER_SIZE - count,
                           OGG_BIG_ENDIAN, 2, 1, &stream );
        if( amt <= 0 )
        {
            if( amt < 0 )
                fprintf( stderr, "ov_read" );
            break;
        }
        count += amt;
    }

    if( ! count )
        return 0;

    format = (vinfo->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    //printf( "KR ogg count %d\n", count );
    alBufferData( buffer, format, _oggPCM, count, vinfo->rate );

    return 1;
}


static void _startMusic()
{
    int i;
    for( i = 0; i < MUSIC_BUFFERS; ++i )
    {
        if( ! _readOgg( _musicBuffers[i], &_vf, _vinfo ) )
        {
            ov_clear( &_vf );   // Closes _bgStream for us.
            _bgStream = 0;
            break;
        }
    }

    if( i > 0 )
    {
        alSourceQueueBuffers( _musicSource, i, _musicBuffers );
        alSourcePlay( _musicSource );
        //printf( "KR _startMusic %d\n", i );
    }
}


/**
  Called once at program startup.
  Returns 0 on a fatal error.
*/
int aud_startup()
{
#ifdef USE_ALUT
    if( alutInit( 0, 0 ) == AL_FALSE )
        return 0;
#else
    _adevice = alcOpenDevice( 0 );
    if( ! _adevice )
        return 0;
    _acontext = alcCreateContext( _adevice, 0 );
    alcMakeContextCurrent( _acontext );
#endif

    alGenSources( SOURCE_COUNT, _asource );
    alGenBuffers( MUSIC_BUFFERS, _musicBuffers );

    _audioUp = 1;

    return 1;
}


/**
  Called once when program exits.
  It is safe to call this even if aud_startup() was not called.
*/
void aud_shutdown()
{
    if( _audioUp )
    {
        aud_stopMusic();

        alDeleteSources( SOURCE_COUNT, _asource );
        alDeleteBuffers( MUSIC_BUFFERS, _musicBuffers );

#ifdef USE_ALUT
        alutExit();
#else
        // Could not use alutExit() on my box (FC3/Shuttle XPC) since
        // alcMakeContextCurrent(0) -> _alLockMixerPause() will hangup.
        alcDestroyContext( _acontext );
        alcCloseDevice( _adevice );
#endif
    }
}


/**
  Called periodically (once per frame) to drive sound engine.
*/
void aud_update()
{
#if 0
    static Millisec prev = 0;
    Millisec now, diff;
    now = osNow();
    diff = now - prev;
    prev = now;
    if( diff > (16 * 5) )
        printf( "%ld\n", diff );
#endif

    if( _musicSource )
    {
#if 0
        ALint sourcestate;
        alGetSourcei( _musicSource, AL_SOURCE_STATE, &sourcestate );
        if( sourcestate != AL_PLAYING )
        {
            if( _bgStream )
            {
                // The music source will stop playing when the buffers
                // have been emptied.  This can easily occur if the X11 window
                // is moved vigorously around or a long load happens between
                // audioUpdate() calls.

                printf( "KR audioUpdate - _musicSource not playing!\n" );

                // For some reason this solution causes the stream quality
                // to degrade (noise and echos occur).
                _startMusic();
            }
            else
            {
                aud_stopMusic();
            }
            return;
        }
#endif

        if( _bgStream )
        {
            ALint processed;
            ALuint freeBuf;

            alGetSourcei( _musicSource, AL_BUFFERS_PROCESSED, &processed );

            //if( processed ) printf( "KR proc %d\n", processed );

            while( processed )
            {
                alSourceUnqueueBuffers( _musicSource, 1, &freeBuf );
                //printf( "   buf %d\n", freeBuf );

                if( _readOgg( freeBuf, &_vf, _vinfo ) )
                {
                    alSourceQueueBuffers( _musicSource, 1, &freeBuf );
                }
                else
                {
                    printf( "KR audioUpdate - end of stream\n" );
                    ov_clear( &_vf );   // Closes _bgStream for us.
                    _bgStream = 0;
                    break;
                }

                --processed;
            }
        }
    }
}


#if 0
static int wav_sig( uint8_t* sig, int sigLen )
{
    if( sigLen < 4 )
        return 0;
    return( sig[0]=='R' && sig[1]=='I' && sig[2]=='F' && sig[3]=='F');
}


int wav_loader( UBuffer* filename, uint8_t* sig, int sigLen )
{
    if( wav_sig( sig, sigLen ) )
    {
        return UR_OK;
    }
    return UR_THROW;
}
#endif


/*-cf-
    buffer-audio
        audio-sample    context!
    return: al_buf_num int!.
*/
CFUNC_PUB( cfunc_buffer_audio )
{
    static ALenum formats[4] = {
        AL_FORMAT_MONO8,  AL_FORMAT_STEREO8,
        AL_FORMAT_MONO16, AL_FORMAT_STEREO16
    };
    const UBuffer* buf;
    const UCell* cell;
    ALuint alBuffer;
    ALenum alFormat;
    ALenum err;
    ALsizei freq;

    if( ! ur_is(a1, UT_CONTEXT) )
        goto bad_ctx;
    buf = ur_bufferSer(a1);
    cell = ur_ctxCell(buf, 0);
    if( ! ur_is(cell, UT_COORD) )
        goto bad_ctx;
    alFormat = ((cell->coord.n[0] - 8) / 4) + (cell->coord.n[1] - 1);
    if( alFormat < 0 || alFormat > 3 )
        return ur_error( ut, UR_ERR_TYPE, "Invalid audio-sample format" );
    alFormat = formats[ alFormat ];

    cell = ur_ctxCell(buf, 1);
    if( ! ur_is(cell, UT_INT) )
        goto bad_ctx;
    freq = ur_int(cell);

    cell = ur_ctxCell(buf, 2);
    if( ! ur_is(cell, UT_BINARY) )
        goto bad_ctx;
    buf = ur_bufferSer(cell);

    if( _audioUp )
    {
        alGenBuffers( 1, &alBuffer );

        alGetError();
        alBufferData( alBuffer, alFormat, buf->ptr.b, buf->used, freq );
        if( (err = alGetError()) != AL_NO_ERROR )
            return ur_error( ut, UR_ERR_INTERNAL, "alBufferData %d", err );

        ur_setId(res, UT_INT /*UT_SOUND*/ );
        SAMPLE_ID(res) = alBuffer;
    }
    else
    {
        ur_setId(res, UT_INT);
        ur_int(res) = 0;
    }
    return UR_OK;

bad_ctx:

    return ur_error( ut, UR_ERR_TYPE,
                     "buffer-audio expected audio-sample context!" );
}


#if 0
bool osLoadSample( OS_SAMPLE* sp, BSample* bs )
{
    if( _audioUp )
    {
        ALenum format;

        switch( bs->format )
        {
            case BSAMPLE_MONO8:    format = AL_FORMAT_MONO8;    break;
            case BSAMPLE_MONO16:   format = AL_FORMAT_MONO16;   break;
            case BSAMPLE_STEREO8:  format = AL_FORMAT_STEREO8;  break;
            case BSAMPLE_STEREO16: format = AL_FORMAT_STEREO16; break;
        }

        alGenBuffers( 1, &sp->uh );
        alBufferData( sp->uh, format, bs->pcm(), bs->size, bs->freq );
    }
    else
    {
        sp->uh = 0;
    }

    return 1;
}


void osInitSample( OS_SAMPLE* sp )
{
    sp->uh = 0;
}


void osFreeSample( OS_SAMPLE* sp )
{
    if( sp->uh )
    {
        alDeleteBuffers( 1, &sp->uh );
        sp->uh = 0;
    }
}
#endif


int aud_playSound( const UCell* sound )
{
    static int sn = 0;
    if( sound && _audioUp )
    {
        ALuint src = _asource[ sn ];
        ++sn;
        if( sn == FX_COUNT )
            sn = 0;

        alSourcei( src, AL_BUFFER, SAMPLE_ID( sound ) );
        alSourcePlay( src );

        return src;
    }
    return 0;
}


#if 0
int osPlayAmbientLoop( SoundData* sd )
{
    static int sn = 0;
    if( sd && _audioUp )
    {
        ALuint src = _asource[ sn + FX_COUNT ];
        ++sn;
        if( sn == AMBIENT_COUNT )
            sn = 0;

        alSourcei( src, AL_BUFFER, SAMPLE_ID( sd ) );
        alSourcei( src, AL_LOOPING, AL_TRUE );
        alSourcePlay( src );

        return src;
    }
    return 0;
}


void osStopSound( int id )
{
    if( _audioUp )
        alSourceStop( id );
}
#endif


void aud_playMusic( const char* file )
{
    if( _audioUp )
    {
        //dprint( "aud_playMusic( %s )", file );

        aud_stopMusic();

        _bgStream = fopen( file, "rb" );
        if( ! _bgStream )
            return;

        if( ov_open( _bgStream, &_vf, NULL, 0 ) < 0 )
        {
            fclose( _bgStream );
            _bgStream = 0;

            fprintf( stderr, "aud_playMusic - %s is not a valid Ogg file\n",
                     file );
        }
        else
        {
            _vinfo = ov_info( &_vf, -1 );

            alGenSources( 1, &_musicSource );

            alSourcei ( _musicSource, AL_LOOPING, AL_FALSE );
            alSourcei ( _musicSource, AL_SOURCE_RELATIVE, AL_TRUE );
            alSource3f( _musicSource, AL_POSITION,  0.0f, 0.0f, 0.0f );
            alSource3f( _musicSource, AL_VELOCITY,  0.0f, 0.0f, 0.0f );
            alSource3f( _musicSource, AL_DIRECTION, 0.0f, 0.0f, 0.0f );
            alSourcef ( _musicSource, AL_ROLLOFF_FACTOR, 0.0f );
            alSourcef ( _musicSource, AL_GAIN, _musicGain );

            _startMusic();
        }
    }
}


void aud_stopMusic()
{
    if( _audioUp )
    {
        if( _musicSource )
        {
            alSourceStop( _musicSource );
            alDeleteSources( 1, &_musicSource );
            _musicSource = 0;
        }

        if( _bgStream )
        {
            ov_clear( &_vf );   // Closes _bgStream for us.
            _bgStream = 0;
        }
    }
}


/*
  \param vol    0-255
*/
void aud_setSoundVolume( int vol )
{
    alListenerf( AL_GAIN, ((ALfloat) vol) / 256.0f );
}


/*
  \param vol    0-255
*/
void aud_setMusicVolume( int vol )
{
    //ALfloat gain;
    //alGetListenerfv( AL_GAIN, &gain );
    _musicGain = ((ALfloat) vol) / 256.0f;
    if( _musicSource )
        alSourcef( _musicSource, AL_GAIN, _musicGain );
}


//EOF
