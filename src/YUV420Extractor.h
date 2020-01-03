/*
 * YUV420Extractor.h -- General collection of kino legacy class extensions
 * Copyright (C) 2002 Charles Yates <charles.yates@pandora.be>
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

#ifndef _YUV420_EXTRACTOR_
#define _YUV420_EXTRACTOR_

class Frame;
class CBufferedWriter;

/** Interface to provide a YUV420 extraction method for Frame objects. The 
    GetExtractor factory method provides an object which corresponds to the 
    deinterlacing type requested.
*/

class YUV420Extractor
{
    public:

    static YUV420Extractor *GetExtractor( CBufferedWriter &out, int deinterlace_type = 0 );

    YUV420Extractor(CBufferedWriter &out) : f(out) { }

    virtual bool Initialise( Frame & ) = 0;
    virtual bool Output( Frame & ) = 0;
    virtual bool Flush( ) = 0;

    protected:

    CBufferedWriter &f;
};

#endif
