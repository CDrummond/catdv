/* Taken from smilutils_0.3.2+cvs20070731 AudioWav.h */

/*
 * AudioWav.h -- Audio IO via Wav
 * Copyright (C) 2003 Charles Yates <charles.yates@pandora.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _KINO_AUDIO_WAV_
#define _KINO_AUDIO_WAV_

#include "Frame.h"
#include "BufferedWriter.h"

typedef struct WavHeader
{
    char riff[ 4 ];
    uint32_t riff_length;
    char type[ 4 ];
    char format[ 4 ];
    uint32_t format_length;
    int16_t filler;
    int16_t channels;
    uint32_t rate;
    uint32_t bytespersecond;
    int16_t bytespersample;
    int16_t bitspersample;
    char data[ 4 ];
    uint32_t data_length;
}
WAVHeaderType, *WAVHeader;

class Wav
{
    private:

    struct WavHeader header;
    CBufferedWriter  &f;
    AudioResample    *resampler;
    
    public:
    
    Wav( CBufferedWriter &file );

    void SetInfo( int16_t channels, int rate, int bytespersample );
    int WriteHeader( );
    bool Set( int16_t *data, int length );
    int Write( uint32_t v );
    int Write( int16_t v );
    int Write( uint8_t v ) { return f.write((unsigned char)v); }
    int Write( int16_t *values, int length );    
    int Write( uint8_t *data, int size );
    bool Initialise( Frame & );
    bool Output( Frame & );
    bool Flush( );
};

#endif
