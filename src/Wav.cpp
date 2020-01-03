/* Taken from smilutils_0.3.2+cvs20070731 AudioWav.cc */

/*
 * AudioWav.cc -- Audio IO via Wav
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "Wav.h"

Wav::Wav(CBufferedWriter &file)
           : f(file)
{
    memset( &header, 0, sizeof( WAVHeader ) );
}

void Wav::SetInfo( int16_t channels, int rate, int bytespersample )
{
    memcpy( header.riff, "RIFF", 4 );
    header.riff_length = sizeof(header) - sizeof(header.riff) - sizeof(header.riff_length);
    header.data_length = 0;
    memcpy( header.type, "WAVE", 4 );
    memcpy( header.format, "fmt ", 4 );
    header.format_length = 0x10;
    header.filler = 0x01;
    header.channels = channels;
    header.rate = rate;
    header.bytespersecond = rate * channels * bytespersample;
    header.bytespersample = bytespersample * channels;
    header.bitspersample = bytespersample * 8;
    memcpy( header.data, "data", 4 );
}

int Wav::WriteHeader( )
{
    int bytes;

    bytes = Write( (uint8_t*)header.riff, 4 );
    bytes += Write( header.riff_length );
    bytes += Write( (uint8_t*)header.type, 4 );

    bytes += Write( (uint8_t*)header.format, 4 );
    bytes += Write( header.format_length );
    bytes += Write( header.filler );
    bytes += Write( header.channels );
    bytes += Write( header.rate );
    bytes += Write( header.bytespersecond );
    bytes += Write( header.bytespersample );
    bytes += Write( header.bitspersample );

    bytes += Write( (uint8_t*)header.data, 4 );
    bytes += Write( header.data_length );

    return bytes;
}

bool Wav::Set( int16_t *data, int length )
{
    header.data_length += length;
    header.riff_length += length;
    return Write( data, length / 2 ) == length;
}

int Wav::Write( uint32_t v)
{
    return f.write((unsigned char)v) +
           f.write((unsigned char)(v >> 8)) +
           f.write((unsigned char)(v >> 16)) +
           f.write((unsigned char)(v >> 24));
}

int Wav::Write( int16_t v)
{
    return f.write((unsigned char)v ) +
           f.write((unsigned char)(v >> 8));
}

int Wav::Write( int16_t *values, int length )
{
    int index = 0;
    int bytes = 0;

    for ( index = 0; index < length; index ++ )
        bytes += Write( ( uint8_t ) values[ index ] ) +
                 Write( ( uint8_t )( values[ index ] >> 8 ) );

    return bytes;
}

int Wav::Write( uint8_t *data, int size )
{
    return f.write((unsigned char *)data, size);
}

bool Wav::Initialise( Frame &frame )
{
    if ( f )
    {
        AudioInfo info;
        frame.GetAudioInfo( info );
        SetInfo( ( int16_t )frame.decoder->audio->num_channels, info.frequency, 2 );
        resampler = new FastAudioResample( info.frequency );
        return WriteHeader( ) != 0;
    }
    else
        return false;
}

bool Wav::Output( Frame &frame )
{
    resampler->Resample( frame );
    return Set( resampler->output, resampler->size );
}

bool Wav::Flush( )
{
    if (f.seekToStart())
        WriteHeader( );
    return true;
}
