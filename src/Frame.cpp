/*
 * frame.cc -- utilities for processing digital video frames
 * Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
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

/** Code for handling raw DV frame data
 
    These methods are for handling the raw DV frame data. It contains methods for 
    getting info and retrieving the audio data.
 
    \file frame.cc
*/

// #ifdef HAVE_CONFIG_H
// #include <config.h>
// #endif

#include <string.h>

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char   guchar;
typedef unsigned short  gushort;
typedef unsigned long   gulong;
typedef unsigned int    guint;

typedef float   gfloat;
typedef double  gdouble;

// C++ includes

#include <string>
#include <iostream>
// #include <strstream>
#include <iomanip>
#include <deque>

// using std::strstream;
using std::setw;
using std::setfill;
using std::hex;
using std::dec;
using std::deque;
using std::cerr;
using std::endl;

// C includes

// #include <pthread.h>

// local includes
#include "Frame.h"
// #include "preferences.h"

// extern Preferences prefs;

VideoInfo::VideoInfo(): width(0), height(0), isPAL(false)
{}

#ifndef HAVE_LIBDV
bool Frame::maps_initialized = false;
int Frame::palmap_ch1[2000];
int Frame::palmap_ch2[2000];
int Frame::palmap_2ch1[2000];
int Frame::palmap_2ch2[2000];

int Frame::ntscmap_ch1[2000];
int Frame::ntscmap_ch2[2000];
int Frame::ntscmap_2ch1[2000];
int Frame::ntscmap_2ch2[2000];

short Frame::compmap[4096];
#endif


/** constructor
 
    All Frame objects share a set of lookup maps,
    which are initalized once (we are using a variant of the Singleton pattern). 
 
*/

Frame::Frame() : playlist_position( -1 ), bytesInFrame(0)
{
//     memset(data, 0, 144000);

#if defined(HAVE_LIBAVCODEC)
	avcodec_init();
	avcodec_get_context_defaults( &libavcodec );
    avcodec_open( &libavcodec, &dvvideo_decoder );
#endif

#ifdef HAVE_LIBDV
    decoder = dv_decoder_new(0, 0, 0);
	decoder->audio->arg_audio_emphasis = 2;
	this->SetPreferredQuality( );
	dv_set_audio_correction ( decoder, DV_AUDIO_CORRECT_AVERAGE );
#else
#define DV_AUDIO_MAX_SAMPLES 1944
	
    if (maps_initialized == false) {

        for (int n = 0; n < 1944; ++n) {
            int sequence1 = ((n / 3) + 2 * (n % 3)) % 6;
            int sequence2 = sequence1 + 6;
            int block = 3 * (n % 3) + ((n % 54) / 18);

            block = 6 + block * 16;
            {
                register int byte = 8 + 2 * (n / 54);
                palmap_ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                palmap_ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
                byte += (n / 54);
                palmap_2ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                palmap_2ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
            }
        }
        for (int n = 0; n < 1620; ++n) {
            int sequence1 = ((n / 3) + 2 * (n % 3)) % 5;
            int sequence2 = sequence1 + 5;
            int block = 3 * (n % 3) + ((n % 45) / 15);

            block = 6 + block * 16;
            {
                register int byte = 8 + 2 * (n / 45);
                ntscmap_ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                ntscmap_ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
                byte += (n / 45);
                ntscmap_2ch1[n] = sequence1 * 150 * 80 + block * 80 + byte;
                ntscmap_2ch2[n] = sequence2 * 150 * 80 + block * 80 + byte;
            }
        }
        for (int y = 0x700; y <= 0x7ff; ++y)
            compmap[y] = (y - 0x600) << 6;
        for (int y = 0x600; y <= 0x6ff; ++y)
            compmap[y] = (y - 0x500) << 5;
        for (int y = 0x500; y <= 0x5ff; ++y)
            compmap[y] = (y - 0x400) << 4;
        for (int y = 0x400; y <= 0x4ff; ++y)
            compmap[y] = (y - 0x300) << 3;
        for (int y = 0x300; y <= 0x3ff; ++y)
            compmap[y] = (y - 0x200) << 2;
        for (int y = 0x200; y <= 0x2ff; ++y)
            compmap[y] = (y - 0x100) << 1;
        for (int y = 0x000; y <= 0x1ff; ++y)
            compmap[y] = y;
        for (int y = 0x800; y <= 0xfff; ++y)
            compmap[y] = -1 - compmap[0xfff - y];
        maps_initialized = true;
    }
#endif
	for ( int n = 0; n < 4; n++ )
		audio_buffers[n] = (int16_t *) malloc(2 * DV_AUDIO_MAX_SAMPLES * sizeof(int16_t));
}


Frame::~Frame()
{
#if defined(HAVE_LIBAVCODEC)
    avcodec_close( &libavcodec );
#endif
	
#ifdef HAVE_LIBDV
    dv_decoder_free(decoder);
#endif
	for (int n = 0; n < 4; n++)
		free(audio_buffers[n]);
}


/** gets a subcode data packet
 
    This function returns a SSYB packet from the subcode data section.
 
    \param packNum the SSYB package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool Frame::GetSSYBPack(int packNum, Pack &pack) const
{
#ifdef HAVE_LIBDV
	pack.data[ 0 ] = packNum;
	dv_get_vaux_pack( decoder, packNum, &pack.data[ 1 ] );
	return true;
#else
    /* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are two DIF blocks in the subcode section */

        for (int j = 0; j < 2; ++j) {

            /* each block has 6 packets */

            for (int k = 0; k < 6; ++k) {

                /* calculate address: 150 DIF blocks per sequence, 80 bytes
                   per DIF block, subcode blocks start at block 1, block and
                   packet have 3 bytes header, packet is 8 bytes long
                   (including header) */

                const unsigned char *s = &data[i * 150 * 80 + 1 * 80 + j * 80 + 3 + k * 8 + 3];
                // printf("ssyb %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                // j * 6 + k, s[0], s[1], s[2], s[3], s[4]);
                if (s[0] == packNum) {
//					printf("GetSSYBPack[%x]: sequence %d, block %d, packet %d\n", packNum,i,j,k);
                    pack.data[0] = s[0];
                    pack.data[1] = s[1];
                    pack.data[2] = s[2];
                    pack.data[3] = s[3];
                    pack.data[4] = s[4];
                    return true;
                }
            }
        }
    }
    return false;
#endif
}


/** gets a video auxiliary data packet
 
    Every DIF block in the video auxiliary data section contains 15
    video auxiliary data packets, for a total of 45 VAUX packets. As
    the position of a VAUX packet is fixed, we could directly look it
    up, but I choose to walk through all data as with the other
    GetXXXX routines.
 
    \param packNum the VAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */
bool Frame::GetVAUXPack(int packNum, Pack &pack) const
{
#ifdef HAVE_LIBDV
	pack.data[ 0 ] = packNum;
	dv_get_vaux_pack( decoder, packNum, &pack.data[ 1 ] );
	//cerr << "VAUX: 0x" 
	//<< setw(2) << setfill('0') << hex << (int) pack.data[0] 
	//<< setw(2) << setfill('0') << hex << (int) pack.data[1] 
	//<< setw(2) << setfill('0') << hex << (int) pack.data[2] 
	//<< setw(2) << setfill('0') << hex << (int) pack.data[3] 
	//<< setw(2) << setfill('0') << hex << (int) pack.data[4] 
	//<< endl;
	return true;

#else
    /* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are three DIF blocks in the VAUX section */

        for (int j = 0; j < 3; ++j) {

            /* each block has 15 packets */

            for (int k = 0; k < 15; ++k) {

                /* calculate address: 150 DIF blocks per sequence, 80 bytes
                   per DIF block, vaux blocks start at block 3, block has 3
                   bytes header, packets have no header and are 5 bytes
                   long. */

                const unsigned char *s = &data[i * 150 * 80 + 3 * 80 + j * 80 + 3 + k * 5];
                //printf("vaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                //	j * 15 + k, s[0],  s[1],  s[2],  s[3],  s[4]);
                if (s[0] == packNum) {
                    pack.data[0] = s[0];
                    pack.data[1] = s[1];
                    pack.data[2] = s[2];
                    pack.data[3] = s[3];
                    pack.data[4] = s[4];
                    return true;
                }
            }
        }
    }
    return false;
#endif
}


/** gets an audio auxiliary data packet
 
    Every DIF block in the audio section contains 5 bytes audio
    auxiliary data and 72 bytes of audio data.  The function searches
    through all DIF blocks although AAUX packets are only allowed in
    certain defined DIF blocks.
 
    \param packNum the AAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */
bool Frame::GetAAUXPack(int packNum, Pack &pack) const
{
#ifdef HAVE_LIBDV
	bool done = false;
	switch ( packNum )
	{
		case 0x50:
			memcpy( pack.data, &decoder->audio->aaux_as, 5 );
			done = true;
			break;
		
		case 0x51:
			memcpy( pack.data, &decoder->audio->aaux_asc, 5 );
			done = true;
			break;
		
		case 0x52:
			memcpy( pack.data, &decoder->audio->aaux_as1, 5 );
			done = true;
			break;
		
		case 0x53:
			memcpy( pack.data, &decoder->audio->aaux_asc1, 5 );
			done = true;
			break;
	}
	if (done)
		return true;
#endif
	
    /* number of DIF sequences is different for PAL and NTSC */

    int seqCount = IsPAL() ? 12 : 10;

    /* process all DIF sequences */

    for (int i = 0; i < seqCount; ++i) {

        /* there are nine audio DIF blocks */
        for (int j = 0; j < 9; ++j) {

            /* calculate address: 150 DIF blocks per sequence, 80 bytes
               per DIF block, audio blocks start at every 16th beginning
               with block 6, block has 3 bytes header, followed by one
               packet. */

            const unsigned char *s = &data[i * 150 * 80 + 6 * 80 + j * 16 * 80 + 3];
            if (s[0] == packNum) {
                // printf("aaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
                // j, s[0], s[1], s[2], s[3], s[4]);
                pack.data[0] = s[0];
                pack.data[1] = s[1];
                pack.data[2] = s[2];
                pack.data[3] = s[3];
                pack.data[4] = s[4];
                return true;
            }
        }
    }
    return false;
}

/** gets the date and time of recording of this frame
 
    Returns a struct tm with date and time of recording of this frame.
 
    This code courtesy of Andy (http://www.videox.net/) 
 
    \param recDate the time and date of recording of this frame
    \return true for success, false if no date or time information could be found */

bool Frame::GetRecordingDate(struct tm &recDate) const
{
#ifdef HAVE_LIBDV
	return dv_get_recording_datetime_tm( decoder, (struct tm *) &recDate);
#else
    Pack pack62;
    Pack pack63;

    if (GetSSYBPack(0x62, pack62) == false)
        return false;

    int day = pack62.data[2];
    int month = pack62.data[3];
    int year = pack62.data[4];

    if (GetSSYBPack(0x63, pack63) == false)
        return false;

    int sec = pack63.data[2];
    int min = pack63.data[3];
    int hour = pack63.data[4];

    sec = (sec & 0xf) + 10 * ((sec >> 4) & 0x7);
    min = (min & 0xf) + 10 * ((min >> 4) & 0x7);
    hour = (hour & 0xf) + 10 * ((hour >> 4) & 0x3);
    year = (year & 0xf) + 10 * ((year >> 4) & 0xf);
    month = (month & 0xf) + 10 * ((month >> 4) & 0x1);
    day = (day & 0xf) + 10 * ((day >> 4) & 0x3);

    if (year < 25)
        year += 2000;
    else
        year += 1900;

    recDate.tm_sec = sec;
    recDate.tm_min = min;
    recDate.tm_hour = hour;
    recDate.tm_mday = day;
    recDate.tm_mon = month - 1;
    recDate.tm_year = year - 1900;
    recDate.tm_wday = -1;
    recDate.tm_yday = -1;
    recDate.tm_isdst = -1;

    /* sanity check of the results */

    if (mktime(&recDate) == -1)
        return false;
    return true;
#endif
}


string Frame::GetRecordingDate(void) const
{
    string recDate;
#ifdef HAVE_LIBDV
	char s[64];
	if (dv_get_recording_datetime( decoder, s))
		recDate = s;
	else
		recDate = "0000-00-00 00:00:00";
#else
    struct tm date;
    strstream sb;

    if (GetRecordingDate(date) == true) {
        sb << setfill('0')
        << setw(4) << date.tm_year + 1900 << '.'
        << setw(2) << date.tm_mon + 1 << '.'
        << setw(2) << date.tm_mday << '_'
        << setw(2) << date.tm_hour << '-'
        << setw(2) << date.tm_min << '-'
        << setw(2) << date.tm_sec;
        sb >> recDate;
    } else {
        recDate = "0000.00.00_00-00-00";
    }
#endif
    return recDate;
}


/** gets the timecode information of this frame
 
    Returns a string with the timecode of this frame. The timecode is
    the relative location of this frame on the tape, and is defined
    by hour, minute, second and frame (within the last second).
     
    \param timeCode the TimeCode struct
    \return true for success, false if no timecode information could be found */

bool Frame::GetTimeCode(TimeCode &timeCode) const
{
#ifdef HAVE_LIBDV
	int timestamp[4];
	
	dv_get_timestamp_int( decoder, timestamp);
	
	timeCode.hour = timestamp[0];
	timeCode.min = timestamp[1];
	timeCode.sec = timestamp[2];
	timeCode.frame = timestamp[3];
#else
    Pack tc;

    if (GetSSYBPack(0x13, tc) == false)
        return false;

    int frame = tc.data[1];
    int sec = tc.data[2];
    int min = tc.data[3];
    int hour = tc.data[4];

    timeCode.frame = (frame & 0xf) + 10 * ((frame >> 4) & 0x3);
    timeCode.sec = (sec & 0xf) + 10 * ((sec >> 4) & 0x7);
    timeCode.min = (min & 0xf) + 10 * ((min >> 4) & 0x7);
    timeCode.hour = (hour & 0xf) + 10 * ((hour >> 4) & 0x3);
#endif
    return true;
}


/** gets the audio properties of this frame
 
    get the sampling frequency and the number of samples in this particular DV frame (which can vary)
 
    \param info the AudioInfo record
    \return true, if audio properties could be determined */

bool Frame::GetAudioInfo(AudioInfo &info) const
{
#ifdef HAVE_LIBDV
	info.frequency = decoder->audio->frequency;
	info.samples = decoder->audio->samples_this_frame;
	info.frames = (decoder->audio->aaux_as.pc3.system == 1) ? 50 : 60;
	info.channels = decoder->audio->num_channels;
	info.quantization = (decoder->audio->aaux_as.pc4.qu == 0) ? 16 : 12;
	return true;
#else	
    int af_size;
    int smp;
    int flag;
    Pack pack50;
	

	info.channels = 2;
	
    /* Check whether this frame has a valid AAUX source packet
       (header == 0x50). If so, get the audio samples count. If not,
       skip this audio data. */

    if (GetAAUXPack(0x50, pack50) == true) {

        /* get size, sampling type and the 50/60 flag. The number of
           audio samples is dependend on all of these. */

        af_size = pack50.data[1] & 0x3f;
        smp = (pack50.data[4] >> 3) & 0x07;
        flag = pack50.data[3] & 0x20;

        if (flag == 0) {
            info.frames = 60;
            switch (smp) {
            case 0:
                info.frequency = 48000;
                info.samples = 1580 + af_size;
                break;
            case 1:
                info.frequency = 44100;
                info.samples = 1452 + af_size;
                break;
            case 2:
                info.frequency = 32000;
                info.samples = 1053 + af_size;
                break;
            }
        } else { // 50 frames (PAL)
            info.frames = 50;
            switch (smp) {
            case 0:
                info.frequency = 48000;
                info.samples = 1896 + af_size;
                break;
            case 1:
                info.frequency = 44100;
                info.samples = 0; // I don't know
                break;
            case 2:
                info.frequency = 32000;
                info.samples = 1264 + af_size;
                break;
            }
        }
        return true;
    } else {
        return false;
    }
#endif
}


bool Frame::GetVideoInfo(VideoInfo &info) const
{
    GetTimeCode(info.timeCode);
    GetRecordingDate(info.recDate);
    info.isPAL = IsPAL();
    return true;
}


/** gets the size of the frame
 
    Depending on the type (PAL or NTSC) of the frame, the length of the frame is returned 
 
    \return the length of the frame in Bytes */

int Frame::GetFrameSize(void) const
{
    return IsPAL() ? 144000 : 120000;
}


/** gets the frame rate of the video
 
    Depending on the type (PAL or NTSC) of the frame, the frame rate is returned 
 
    \return frames per second */

float Frame::GetFrameRate(void) const
{
    return IsPAL() ? 25.0 : 30000.0/1001.0;
}


/** checks whether the frame is in PAL or NTSC format
 
    \todo function can't handle "empty" frame
    \return true for PAL frame, false for a NTSC frame
*/

bool Frame::IsPAL(void) const
{
    unsigned char dsf = data[3] & 0x80;
    bool pal = (dsf == 0) ? false : true;
#ifdef HAVE_LIBDV
    if (!pal) {
        pal = (dv_system_50_fields (decoder) == 1) ? true : pal;
    }
#endif
    return pal;
}


/** checks whether this frame is the first in a new recording
 
    To determine this, the function looks at the recStartPoint bit in
    AAUX pack 51.
 
    \return true if this frame is the start of a new recording */

bool Frame::IsNewRecording() const
{
#ifdef HAVE_LIBDV
	return (decoder->audio->aaux_asc.pc2.rec_st == 0);
#else
    Pack aauxSourceControl;

    /* if we can't find the packet, we return "no new recording" */

    if (GetAAUXPack(0x51, aauxSourceControl) == false)
        return false;

    unsigned char recStartPoint = aauxSourceControl.data[2] & 0x80;

    return recStartPoint == 0 ? true : false;
#endif
}


/** check whether we have received as many bytes as expected for this frame
 
    \return true if this frames is completed, false otherwise */

bool Frame::IsComplete(void) const
{
    return bytesInFrame == GetFrameSize();
}


/** retrieves the audio data from the frame
 
    The DV frame contains audio data mixed in the video data blocks, 
    which can be retrieved easily using this function.
 
    The audio data consists of 16 bit, two channel audio samples (a 16 bit word for channel 1, followed by a 16 bit word
    for channel 2 etc.)
 
    \param sound a pointer to a buffer that holds the audio data
    \return the number of bytes put into the buffer, or 0 if no audio data could be retrieved */

int Frame::ExtractAudio(void *sound) const
{
    AudioInfo info;
	
#ifdef HAVE_LIBDV
    if (GetAudioInfo(info) == true) {
		int n, i;
		int16_t* s = (int16_t *) sound;
		if(!dv_decode_full_audio( decoder, data, (int16_t **)audio_buffers))
			memset(s, 0, info.samples*info.channels*2);
		else

		for (n = 0; n < info.samples; ++n)
			for (i = 0; i < info.channels; i++)
				*s++ = audio_buffers[i][n];
			
	} else
		info.samples = 0;
	
#else
        /* Collect the audio samples */
        char* s = (char *) sound;
	
		GetAudioInfo(info);
        switch (info.frequency) {
        case 32000:

            /* This is 4 channel audio */

            if (IsPAL()) {
                short* p = (short*)sound;
                for (int n = 0; n < info.samples; ++n) {
                    register int r = ((unsigned char*)data)[palmap_2ch1[n] + 1]; // LSB
                    *p++ = compmap[(((unsigned char*)data)[palmap_2ch1[n]] << 4) + (r >> 4)];   // MSB
                    *p++ = compmap[(((unsigned char*)data)[palmap_2ch1[n] + 1] << 4) + (r & 0x0f)];
                }


            } else {
                short* p = (short*)sound;
                for (int n = 0; n < info.samples; ++n) {
                    register int r = ((unsigned char*)data)[ntscmap_2ch1[n] + 1]; // LSB
                    *p++ = compmap[(((unsigned char*)data)[ntscmap_2ch1[n]] << 4) + (r >> 4)];   // MSB
                    *p++ = compmap[(((unsigned char*)data)[ntscmap_2ch1[n] + 1] << 4) + (r & 0x0f)];
                }
            }
            break;

        case 44100:
        case 48000:

            /* this can be optimized significantly */

            if (IsPAL()) {
                for (int n = 0; n < info.samples; ++n) {
                    *s++ = ((char*)data)[palmap_ch1[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[palmap_ch1[n]];     /* MSB */
                    *s++ = ((char*)data)[palmap_ch2[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[palmap_ch2[n]];     /* MSB */
                }
            } else {
                for (int n = 0; n < info.samples; ++n) {
                    *s++ = ((char*)data)[ntscmap_ch1[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[ntscmap_ch1[n]];     /* MSB */
                    *s++ = ((char*)data)[ntscmap_ch2[n] + 1]; /* LSB */
                    *s++ = ((char*)data)[ntscmap_ch2[n]];     /* MSB */
                }
            }
            break;

            /* we can't handle any other format in the moment */

        default:
            info.samples = 0;
        }
    }
	else {
        info.samples = 0;
	}

#endif
    return info.samples * info.channels * 2;
}

#ifdef HAVE_LIBDV

void Frame::SetPreferredQuality( )
{
	decoder->quality = DV_QUALITY_BEST;
}

/** retrieves the audio data from the frame
 
    The DV frame contains audio data mixed in the video data blocks, 
    which can be retrieved easily using this function.
 
    The audio data consists of 16 bit, two channel audio samples (a 16 bit word for channel 1, followed by a 16 bit word
    for channel 2 etc.)
 
    \param channels an array of buffers of audio data, one per channel, up to four channels
    \return the number of bytes put into the buffer, or 0 if no audio data could be retrieved */

int Frame::ExtractAudio(int16_t **channels) const
{
    AudioInfo info;

    if (GetAudioInfo(info) == true)
    {
		if(!dv_decode_full_audio( decoder, data, channels))
			memset(channels, 0, info.samples*info.channels*2);
    }
	else
		info.samples = 0;
	
	return info.samples * info.channels * 2;
}

void Frame::ExtractHeader(void)
{
	dv_parse_header(decoder, data);
	dv_parse_packs(decoder, data);
}

void Frame::GetUpperField( void *image, int bpp )
{
	register int width = GetWidth( ) * bpp;
	register int height = GetHeight( );
	for ( register int i = 0; i < height; i += 2 )
		memcpy( (uint8_t *)image + width * ( i + 1 ), (uint8_t *)image + width * i, width );
}

void Frame::GetLowerField( void *image, int bpp )
{
	register int width = GetWidth( ) * bpp;
	register int height = GetHeight( );
	for ( register int i = 0; i < height; i += 2 )
		memcpy( (uint8_t *)image + width * i, (uint8_t *)image + width * (i + 1), width );
}

int Frame::ExtractRGB(void *rgb)
{
#if defined(HAVE_LIBAVCODEC)
	if ( IsPAL() )
	{
		AVFrame frame;
		int got_picture;
	
		avcodec_decode_video( &libavcodec, &frame, &got_picture, data, GetFrameSize());
	
		AVPicture dest;
		dest.data[0] = static_cast<uint8_t *>(rgb);
		dest.linesize[0] = GetWidth() * 3;
	
		img_convert( &dest, PIX_FMT_RGB24, reinterpret_cast<AVPicture*>(&frame), libavcodec.pix_fmt, GetWidth(), GetHeight());
	} else {
#endif
		guchar *pixels[3];
		gint pitches[3];
	
		pixels[0] = (guchar*)rgb;
		pixels[1] = NULL;
	
	
		pixels[2] = NULL;
	
		pitches[0] = 720 * 3;
		pitches[1] = 0;
		pitches[2] = 0;
	
		dv_decode_full_frame(decoder, data, e_dv_color_rgb, pixels, pitches);
#if defined(HAVE_LIBAVCODEC)
	}
#endif
    return 0;
}

int Frame::ExtractPreviewRGB(void *rgb, bool deinterlace)
{
	ExtractRGB( rgb );
 	if ( deinterlace) // Preferences::getInstance().deinterlacePreview )
		Deinterlace( ( uint8_t* ) rgb, ( uint8_t* ) rgb, GetWidth() * 3, GetHeight() );
	return 0;
}

int Frame::ExtractYUV(void *yuv)
{
#if defined(HAVE_LIBAVCODEC)
    AVFrame frame;
    int got_picture;

    avcodec_decode_video( &libavcodec, &frame, &got_picture, data, GetFrameSize() );

    uint8_t* dest = static_cast<uint8_t*>(yuv);
    int width = GetWidth(), height = GetHeight();

    if ( libavcodec.pix_fmt == PIX_FMT_YUV420P ) // PAL
    {
        for ( int y = 0; y < height; ++y )
        {
            uint8_t* Y = frame.data[0] + y * frame.linesize[0];
            uint8_t* Cb = frame.data[1] + (y / 2) * frame.linesize[1];
            uint8_t* Cr = frame.data[2] + (y / 2) * frame.linesize[2];

            for ( int x = 0; x < width; x += 2 )
            {
				dest[ 0 ] = Y[ 0 ];
				dest[ 1 ] = Cb[ 0 ];
				dest[ 2 ] = Y[ 1 ];
				dest[ 3 ] = Cr[ 0 ];
				
                dest += 4;
                Y += 2;
                ++Cb;
                ++Cr;
            }
        }
    }
    else // libavcodec.pix_fmt == PIX_FMT_YUV411P // NTSC
    {
        for ( int y = 0; y < height; ++y )
        {
            uint8_t* Y = frame.data[0] + y * frame.linesize[0];
            uint8_t* Cb = frame.data[1] + y * frame.linesize[1];
            uint8_t* Cr = frame.data[2] + y * frame.linesize[2];

            for(int x = 0; x < width; x += 4)
            {
				dest[ 0 ] = Y[ 0 ];
				dest[ 1 ] = Cb[ 0 ];
				dest[ 2 ] = Y[ 1 ];
				dest[ 3 ] = Cr[ 0 ];
				dest[ 4 ] = Y[ 2 ];
				dest[ 5 ] = Cb[ 0 ];
				dest[ 6 ] = Y[ 3 ];
				dest[ 7 ] = Cr[ 0 ];

                dest += 8;
                Y += 4;
                ++Cb;
                ++Cr;
            }
        }
    }
#else
    guchar *pixels[3];
    gint pitches[3];

    pixels[0] = (guchar*)yuv;
    pitches[0] = decoder->width * 2;

    dv_decode_full_frame(decoder, data, e_dv_color_yuv, pixels, pitches);
#endif

    return 0;
}

int Frame::ExtractYUV420(uint8_t *yuv, uint8_t *output[ 3 ])
{
#if defined(HAVE_LIBAVCODEC)
    AVFrame frame;
    int got_picture;

    avcodec_decode_video( &libavcodec, &frame, &got_picture, data, GetFrameSize() );

    int width = GetWidth(), height = GetHeight();

    if ( libavcodec.pix_fmt == PIX_FMT_YUV420P ) // PAL
    {
		int h2 = height / 2;
		int w2 = width / 2;

		uint8_t *Y = output[ 0 ];
		uint8_t *Cb = output[ 1 ];
		uint8_t *Cr = output[ 2 ];
		uint8_t *fY = frame.data[ 0 ];
		uint8_t *fCb = frame.data[ 1 ];
		uint8_t *fCr = frame.data[ 2 ];

		for ( int y = 0; y < height; y ++ )
		{
			memcpy( Y, fY, width );
			Y += width;
			fY += frame.linesize[ 0 ];
			if ( y < h2 )
			{
				memcpy( Cb, fCb, w2 );
				memcpy( Cr, fCr, w2 );
				Cb += w2;
				Cr += w2;
				fCb += frame.linesize[ 1 ];
				fCr += frame.linesize[ 2 ];
			}
		}
    }
    else // libavcodec.pix_fmt == PIX_FMT_YUV411P // NTSC
    {
		int h2 = height / 2;
		int w4 = width / 4;

		uint8_t *Y = output[ 0 ];
		uint8_t *Cb = output[ 1 ];
		uint8_t *Cr = output[ 2 ];
		uint8_t *fY = frame.data[ 0 ];
		uint8_t *fCb = frame.data[ 1 ];
		uint8_t *fCr = frame.data[ 2 ];

		for ( int y = 0; y < height; y ++ )
		{
			memcpy( Y, fY, width );
			Y += width;
			fY += frame.linesize[ 0 ];
			if ( y % 2 == 0 )
			{
				for ( int x = 0; x < w4; x ++ )
				{
					*Cb ++ = fCb[ x ];
					*Cb ++ = fCb[ frame.linesize[ 1 ] + x ];
					*Cr ++ = fCr[ x ];
					*Cr ++ = fCr[ frame.linesize[ 2 ] + x ];
				}
				fCb += 2 * frame.linesize[ 1 ];
				fCr += 2 * frame.linesize[ 2 ];
			}
		}
    }
#else
    guchar *pixels[3];
    gint pitches[3];
    int width = GetWidth(), height = GetHeight();

    pixels[0] = (guchar*)yuv;
    pitches[0] = decoder->width * 2;

    dv_decode_full_frame(decoder, data, e_dv_color_yuv, pixels, pitches);

	int w2 = width/2;
	uint8_t *y = output[0];
	uint8_t *cb = output[1];
	uint8_t *cr = output[2];
	uint8_t *p = yuv;

	for ( int i = 0; i < height; i += 2 ) 
	{
		/* process two scanlines (one from each field, interleaved) */
		for ( int j = 0; j < w2; j++ ) 
		{
			/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
			*(y++) =  *(p++);
			*(cb++) = *(p++);
			*(y++) =  *(p++);
			*(cr++) = *(p++);
		}
		/* process next two scanlines (one from each field, interleaved) */
		for ( int j = 0; j < w2; j++ ) 
		{
			/* skip every second line for U and V */
			*(y++) = *(p++);
			p++;
			*(y++) = *(p++);
			p++;
		}
	}
#endif
	return 0;
}

int Frame::ExtractPreviewYUV(void *yuv, bool deinterlace)
{
	ExtractYUV( yuv );
 	if ( deinterlace ) // Preferences::getInstance().deinterlacePreview )
		Deinterlace( ( uint8_t* ) yuv, ( uint8_t* ) yuv, GetWidth() * 2, GetHeight() );
	return 0;
}

/* Linear Blend filter - C version contributed by Rogerio Brito.
   This algorithm has the same interface as the other functions.

   The destination "screen" (pdst) is constructed from the source
   screen (psrc[0]) line by line.

   The i-th line of the destination screen is the average of 3 lines
   from the source screen: the (i-1)-th, i-th and (i+1)-th lines, with
   the i-th line having weight 2 in the computation.

   Remarks:
   * each line on pdst doesn't depend on previous lines;
   * due to the way the algorithm is defined, the first & last lines of the
     screen aren't deinterlaced.

*/
void Frame::Deinterlace( uint8_t *pdst, uint8_t *psrc, int stride, int height )
{
    register int x, y;
    register uint8_t *l0, *l1, *l2, *l3;

    l0 = pdst;      /* target line */
    l1 = psrc;      /* 1st source line */
    l2 = l1 + stride;   /* 2nd source line = line that follows l1 */
    l3 = l2 + stride;   /* 3rd source line = line that follows l2 */

    /* Copy the first line */
    memcpy( l0, l1, stride );
    l0 += stride;

    for (y = 1; y < height-1; ++y)
    {
        /* computes avg of: l1 + 2*l2 + l3 */
        for ( x = 0; x < stride; ++x )
            l0[x] = ( l1[ x ] + ( l2[ x ] << 1 ) + l3[ x ] ) >> 2;

        /* updates the line pointers */
        l1 = l2;
        l2 = l3;
        l3 += stride;
        l0 += stride;
    }

    /* Copy the last line */
    memcpy( l0, l1, stride );
}

/** Get the frame aspect ratio.

	Indicates whether frame aspect ration is normal (4:3) or wide (16:9).

    \return true if the frame is wide (16:9), false if unknown or normal.
*/
bool Frame::IsWide( void ) const
{
	return dv_format_wide(decoder) > 0;
}


/** Get the frame image width.

    \return the width in pixels.
*/
int Frame::GetWidth()
{
	return decoder->width;
}


/** Get the frame image height.

    \return the height in pixels.
*/
int Frame::GetHeight()
{
	return decoder->height;
}


/** Set the RecordingDate of the frame.

    This updates the calendar date and time and the timecode.
    However, timecode is derived from the time in the datetime
    parameter and frame number. Use SetTimeCode for more control
    over timecode.

    \param datetime A simple time value containing the
           RecordingDate and time information. The time in this
           structure is automatically incremented by one second
           depending on the frame parameter and updatded.
    \param frame A zero-based running frame sequence/serial number.
           This is used both in the timecode as well as a timestamp on
           dif block headers.
*/
void Frame::SetRecordingDate( time_t *datetime, int frame )
{
	dv_encode_metadata( data, IsPAL(), IsWide(), datetime, frame );
}

/** Set the TimeCode of the frame.

    This function takes a zero-based frame counter and automatically
    derives the timecode.

    \param frame The frame counter.
*/
void Frame::SetTimeCode( int frame )
{
	dv_encode_timecode( data, IsPAL(), frame );
}


bool Frame::EncodeAudio( AudioInfo &info, int16_t **channels )
{
	dv_encoder_t *encoder = dv_encoder_new( FALSE, FALSE, FALSE );
#ifdef LIBDV_HAS_SAMPLE_CALCULATOR
	encoder->samples_this_frame = info.samples;
#endif
	int result = dv_encode_full_audio( encoder, channels, info.channels, info.frequency, data );
	dv_encoder_free( encoder );
	return ( result != -1 );
}

#endif

/** Constructor for the resampler.
*/

AudioResample::AudioResample( int rate ) : output_rate( rate ) 
{
	input = new int16_t[ 20480 ];
	output = new int16_t[ 20480 ];
}

/** Destructor for the resampler.
*/

AudioResample::~AudioResample() {
	delete input;
	delete output;
}

/** Frame handler.
*/

void AudioResample::Resample( Frame &frame ) 
{
  //  cout << "Resample -----------------" << endl;
	if ( output_rate != 0 ) 
	{
		frame.ExtractAudio( input );
		AudioInfo info;
		frame.GetAudioInfo( info );
		/*
		cerr << "Audio info: " << endl
		     << "  dec-..->frequency: " 
		     << info.frequency << endl
		     << "  samples: " 
		     << info.samples << endl;
		*/
		if ( output_rate != info.frequency ) 
		{
			Resample( input, 
					  info.frequency, 
					  info.channels,
					  info.samples );
		}
		else 
		{
			memcpy( output, input, info.samples * 4 );
			size = info.samples * 4;
		}
	}
	else 
	{
		size = 0;
	}
	// cout << "size: " << size << endl;
}

/** Constructor for fast resampler
 */

FastAudioResample::FastAudioResample( ) : AudioResample( 0 ) {
}

FastAudioResample::FastAudioResample( int output_rate ) : AudioResample( output_rate ) {
}

/** Destructor for fast resampler
 */

FastAudioResample::~FastAudioResample() {
}

/** Simple up and down resampler for people unable to handle 48khz audio. 
    Also fixes mixed projects with 32khz and 48khz audio sampling (yippee!).  
*/

void FastAudioResample::Resample( int16_t *input, int input_rate, int channels, int samples )
{
	float ratio = (float)output_rate / (float)input_rate;
	size = (int)( (float)samples * ratio );

	int rounding = 1 << 15;
	unsigned int xfactor = ( samples << 16 ) / size;
	unsigned int xmax = xfactor * size;
	unsigned int i = 0;
	unsigned int o = 0;
	size *= 2 * channels;

	for ( unsigned int xft = 0; xft < xmax; xft += xfactor ) {
		i = ( ( xft + rounding ) >> 16 ) * channels;
		memcpy( &output[ o ], &input[ i ], 2 * channels );
		o += channels;
	}

}
