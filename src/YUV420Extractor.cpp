#include <config.h>
#include <iostream>
#include <string>
using std::string;

#ifdef __linux__
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "YUV420Extractor.h"
#include "Frame.h"
#include "BufferedWriter.h"

static const char *aspect_tag(int height, bool wide)
{
    if (height == 576) 
    {
        if (wide)
            return " A118:81";  // PAL 16:9 display
        else
            return " A59:54";   // PAL 4:3 display
    }
    else 
    {
        if (wide)
            return " A40:33";   // NTSC 16:9 display
        else
            return " A10:11";   // NTSC 4:3 display
    }           
}

/** Extracts the YUV frames and outputs them.
*/
static char header[128];

class ExtendedYUV420Extractor : public YUV420Extractor
{
    public:
        ExtendedYUV420Extractor(CBufferedWriter &out) : YUV420Extractor(out) { }

        bool Initialise( Frame &frame )
        {
            width = frame.GetWidth( );
            height = frame.GetHeight( );

            pitches[ 0 ] = width * 2;
            pitches[ 1 ] = 0;
            pitches[ 2 ] = 0;

            output[ 0 ] = new uint8_t[ width * height ];
            output[ 1 ] = new uint8_t[ width * height / 4 ];
            output[ 2 ] = new uint8_t[ width * height / 4 ];

// Define space for a uncompressed YUV422 PAL frame (being the maxiumum)
//  NOTE: uncompressed 4:2:2 is 720*576*2 not 720*576*4 - why waste space?
            input = new uint8_t[720 * 576 * 2];

            // Output the header
            sprintf(header, "YUV4MPEG2 W%d H%d F%s Ib%s %s\n",
                    width, height, height == 576 ? "25:1" : "30000:1001",
                    aspect_tag(height, frame.IsWide()),
                    height == 576 ? "C420paldv" : "C420mpeg2");
            f.write((unsigned char *)header, strlen(header));
            /*
            std::cout << "YUV4MPEG2 W" << width 
                      << " H" << height 
                      << " F" << ( height == 576 ? "25:1" : "30000:1001" ) 
                      << " Ib"
                       << aspect_tag(height, frame.IsWide())
                      << (height == 576 ? " C420paldv" : " C420mpeg2")
                      << std::endl;
            */
            return input != NULL;
        }

        bool Output( Frame &frame )
        {
            Extract( frame );
            //std::cout << "FRAME" << std::endl;
            return f.write((unsigned char *)"FRAME\n", 6) &&
                   f.write( output[0], width * height) &&
                   f.write( output[1], width * height / 4) &&
                   f.write( output[2], width * height / 4);
        }

        bool Flush( )
        {
            delete output[ 0 ];
            delete output[ 1 ];
            delete output[ 2 ];
            delete input;
            return true;
        }

    protected:

        int width;
        int height;
        int pitches[ 3 ];
        uint8_t *output[ 3 ];
        uint8_t *input;

        virtual void Extract( Frame &frame )
        {
            frame.decoder->quality = DV_QUALITY_BEST;
            frame.ExtractYUV420( input, output );
        }
};

/** Provides deinterlaced output.
*/

#define SCALEBITS 8
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)        ((int) ((x) * (1L<<SCALEBITS) + 0.5))

class ExtendedYUV420CruftyExtractor : public ExtendedYUV420Extractor
{
    public:
        ExtendedYUV420CruftyExtractor(CBufferedWriter &out) : ExtendedYUV420Extractor(out) { }

        virtual void Extract( Frame &frame )
        {
            int r, g, b, r1, g1, b1;

            frame.decoder->quality = DV_QUALITY_BEST;

            frame.ExtractRGB( input );

            uint8_t *lum = output[0];
            uint8_t *cb = output[1];
            uint8_t *cr = output[2];

            int wrap = width;
            int wrap3 = width * 3;
            uint8_t *p = input;

            for( int y = 0; y < height; y += 2 ) 
            {
                for( int x = 0; x < width; x += 2 ) 
                {
                    r = p[0];
                    g = p[1];
                    b = p[2];
                    r1 = r;
                    g1 = g;
                    b1 = b;
                    lum[width] = lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g + 
                              FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
                    r = p[3];
                    g = p[4];
                    b = p[5];
                    r1 += r;
                    g1 += g;
                    b1 += b;
                    lum[width + 1] = lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g + 
                              FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
                    lum += wrap;
        
                    cb[0] = ((- FIX(0.16874) * r1 - FIX(0.33126) * g1 + 
                              FIX(0.50000) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 1)) + 128;
                    cr[0] = ((FIX(0.50000) * r1 - FIX(0.41869) * g1 - 
                             FIX(0.08131) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 1)) + 128;

                    cb++;
                    cr++;
                    p += 6;
                    lum += -wrap + 2;
                }
                p += wrap3;
                lum += wrap;
            }
        }
};

/*
 * Extracts 4:1:1 data from NTSC DV and place into a YUV4MPEG2 stream.
 * Note: ONLY for NTSC DV material!
 *
 */
class ExtendedYUV411Extractor : public YUV420Extractor
{
    public:

        ExtendedYUV411Extractor(CBufferedWriter &out) : YUV420Extractor(out) { }

        bool Initialise( Frame &frame )
        {
            width = frame.GetWidth( );
            height = frame.GetHeight( );
    
            pitches[ 0 ] = width * 2;
            pitches[ 1 ] = 0;
            pitches[ 2 ] = 0;
    
            output[ 0 ] = new uint8_t[ width * height ];
            output[ 1 ] = new uint8_t[ width * height / 4 ];
            output[ 2 ] = new uint8_t[ width * height / 4 ];
    
// Define space for a uncompressed YUV422 PAL frame (being the maxiumum)
//  NOTE: uncompressed 4:2:2 is 720*576*2 not 720*576*4.  ALSO why PAL since
//        this is NTSC specific?
            input = new uint8_t[ 720 * 576 * 2 ];
    
            // Output the header.  4:1:1 is specific to NTSC so
            //  it is silly to check for PAL frame size and rate.
            sprintf(header, "YUV4MPEG2 W%d H%d F30000:1001 Ib%s C411\n",
                    width, height, aspect_tag(height, frame.IsWide()));
            f.write((unsigned char *)header, strlen(header));
            /*
            std::cout << "YUV4MPEG2 W" << width 
                       << " H" << height 
                       << " F30000:1001"
                       << " Ib"
                       << aspect_tag(height, frame.IsWide())
                       << " C411"
                       << std::endl;
            */
            return input != NULL;
        }
  
        bool Output( Frame &frame )
        {
            Extract( frame );
            //std::cout << "FRAME" << std::endl;
            return f.write((unsigned char *)"FRAME\n", 6) &&
                   f.write( output[0], width * height) &&
                   f.write( output[1], width * height / 4) &&
                   f.write( output[2], width * height / 4);
        }
  
        bool Flush( )
        {
            delete output[ 0 ];
            delete output[ 1 ];
            delete output[ 2 ];
            delete input;
            return true;
        }

    protected:
        int width;
        int height;
        int pitches[3];
        uint8_t *output[3];
        uint8_t *input;
  
        virtual void Extract(Frame &frame)
        {
            frame.decoder->quality = DV_QUALITY_BEST;
            frame.ExtractYUV( input );

            int w4 = width / 4;
            uint8_t *y = output[0];
            uint8_t *cb = output[1];
            uint8_t *cr = output[2];
            uint8_t *p = input;

            for (int i = 0; i < height; i++) 
            {
                /* process one scanline, 4 luma samples at a time:
                use every luma sample, skip every other chroma sample */
                for (int j = 0; j < w4; j++) 
                {
                    /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
                    *(y++) =  *(p++);
                    *(cb++) = *(p++);
                    *(y++) =  *(p++);
                    *(cr++) = *(p++);

                    *(y++) =  *(p++);
                    (p++);  // skip this Cb sample
                    *(y++) =  *(p++);
                    (p++);  // skip this Cr sample
                }
            }
        }
};  // ExtendedYUV411Extractor


/** Factory method to obtain the image extractor.
*/

YUV420Extractor *YUV420Extractor::GetExtractor( CBufferedWriter &out, int deinterlace_type )
{
    YUV420Extractor *extractor = NULL;

    switch ( deinterlace_type )
    {
        case 1:
            extractor = new ExtendedYUV420CruftyExtractor( out );
            break;
        case 2:
            extractor = new ExtendedYUV411Extractor( out );
            break;
        case 0:
        default:
            extractor = new ExtendedYUV420Extractor( out );
            break;
    }

    return extractor;
}
