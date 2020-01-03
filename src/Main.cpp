/*
  catdv (C) Craig Drummond, 2007 craig.p.drummond@gmail.com

  ----

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License version 2 as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "Clip.h"

static void usage(char *app)
{
    std::cerr << "Usage:" << app << "[options] <smil/dv/kdenlive>" << std::endl
              << std::endl
              << "    --info                 Print information" << std::endl
              << "    --subtitles [file]     Output subtitles" << std::endl
              << "    --adjust <hours>       Adjust subtitle subtitle timestamp" << std::endl
              << "    --format <format>      Subtitle format - default " << CClipList::subtitleFormat << std::endl
              << "    --simplesmil [file]    Simple SMIL file" << std::endl
              << "    --smil [file]          SMIL file" << std::endl
              << "    --yuv [file]           YUV file" << std::endl
              << "    --deinterlace <level>  Specify deinterlace of YUV" << std::endl
              << "                             0 - no deinterlacing" << std::endl
              << "                             1 - bad deinterlacing" << std::endl
              << "                             2 - experimental 4:1:1 subsampling" << std::endl
              << "    --wav [file]           WAV file" << std::endl
              << "    --dvdauthor <file>     Create DVD author XML file" << std::endl
              << "                           Chapter names are output to <file>.chapters" << std::endl
              << "    --spumux <file>        Create spumux XML file" << std::endl
              << "    --kmf <file>           Create KMediaFactory XML file" << std::endl
              << "    --dv [file]            Raw DV file" << std::endl
              << "    --coverpic <file>      1st frame coverted to 1:1" << std::endl
              << "    --menupic <file>       1st frame" << std::endl
              << "    --progress             Display progress to stderr" << std::endl
              << "    --help                 Display this help" << std::endl;
}

enum Mode
{
    None       = 0x0000,
    Dv         = 0x0001,
    Info       = 0x0002,
    SimpleSmil = 0x0004,
    Smil       = 0x0008,
    Wav        = 0x0010,
    Yuv        = 0x0020,
    DvdAuthor  = 0x0040,
    Subtitles  = 0x0080,
    MenuPic    = 0x0100,
    CoverPic   = 0x0200,
    Spumux     = 0x0400,
    Kmf        = 0x0800,
    Help       = 0x1000,
};

int main(int argc, char **argv)
{
    static struct option opts[] =
    {
        {"info",        no_argument,       NULL, 'i'},
        {"subtitles",   optional_argument, NULL, 's'},
        {"adjust",      required_argument, NULL, 'a'},
        {"format",      required_argument, NULL, 'f'},
        {"simplesmil",  optional_argument, NULL, 'x'},
        {"smil",        optional_argument, NULL, 'z'},
        {"yuv",         optional_argument, NULL, 'y'},
        {"wav",         optional_argument, NULL, 'w'},
        {"dvdauthor",   required_argument, NULL, 'd'},
        {"spumux",      required_argument, NULL, 'S'},
        {"kmf",         required_argument, NULL, 'k'},
        {"dv",          optional_argument, NULL, 'v'},
        {"deinterlace", required_argument, NULL, 'p'},
        {"coverpic",    required_argument, NULL, 'c'},
        {"menupic",     required_argument, NULL, 'm'},
        {"progress",    no_argument,       NULL, 'P'},
        {"help",        no_argument,       NULL, 'h'},
        {0,             0,                 0,    0  }
    };

    QString subFile('-'),
            wavFile('-'),
            yuvFile('-'),
            simpleSmilFile('-'),
            smilFile('-'),
            dvdAuthorFile,
            dvFile('-'),
            coverpicFile,
            menupicFile,
            spumuxFile,
            kmfFile;
    int     mode=None,
            adjust=0,
            stdOut=0;

    for(;;)
    {
        int currentIndex(0),
            ch=getopt_long(argc, argv, "is::f:x::z::d::v::hy::w::p:m:c:S:Pk:", opts, &currentIndex);

        if (-1==ch)
            break;

        switch(ch)
        {
            case 'i':
                mode|=Info;
                stdOut++;
                break;
            case 's':
                mode|=Subtitles;
                if(optarg && strcmp(optarg, "-"))
                    subFile=optarg;
                else
                    stdOut++;
                break;
            case 'a':
                adjust=atoi(optarg);
                break;
            case 'y':
                mode|=Yuv;
                if(optarg && strcmp(optarg, "-"))
                    yuvFile=optarg;
                else
                    stdOut++;
                break;
            case 'w':
                mode|=Wav;
                if(optarg && strcmp(optarg, "-"))
                    wavFile=optarg;
                else
                    stdOut++;
                break;
            case 'f':
                CClipList::subtitleFormat=new char[strlen(optarg)+1];
                strcpy(CClipList::subtitleFormat, optarg);
                break;
            case 'x':
                mode|=SimpleSmil;
                if(optarg && strcmp(optarg, "-"))
                    simpleSmilFile=optarg;
                else
                    stdOut++;
                break;
            case 'z':
                mode|=Smil;
                if(optarg && strcmp(optarg, "-"))
                    smilFile=optarg;
                else
                    stdOut++;
                break;
            case 'd':
                mode|=DvdAuthor;
                dvdAuthorFile=optarg;
                break;
            case 'v':
                mode|=Dv;
                if(optarg && strcmp(optarg, "-"))
                    dvFile=optarg;
                else
                    stdOut++;
                break;
            case 'p':
                CClipList::deinterlace=strlen(optarg)==1 && isdigit(optarg[0]) ? atoi(optarg) : 100;
                break;
            case 'm':
                menupicFile=optarg;
                mode|=MenuPic;
                break;
            case 'c':
                coverpicFile=optarg;
                mode|=CoverPic;
                break;
            case 'S':
                spumuxFile=optarg;
                mode|=Spumux;
                break;
            case 'k':
                kmfFile=optarg;
                mode|=Kmf;
                break;
            case 'P':
                CClipList::displayProgress=true;
                break;
            case 'h':
            case '?':
                mode|=Help;
        }
    }
                    
    if(optind >= argc || None==mode || mode&Help || CClipList::deinterlace<0 || CClipList::deinterlace>2 ||
       "-"==dvdAuthorFile || "-"==menupicFile || "-"==coverpicFile || "-"==spumuxFile)
        usage(argv[0]);
    else if(stdOut>1)
        std::cerr << "ERROR: Only one file may be redirected to stdout" << std::endl;
    else
    {
        CClipList clips(argv[optind]);

        for(int index=optind+1; index<argc; ++index)
        {
            CClipList other(argv[index]);

            if(other.totalFrames() && other.check())
            {
                CClipList::iterator it(other.begin()),
                                    end(other.end());

                for(; it!=end; ++it)
                    clips.addClip(*it);
            }
        }

        if(clips.totalFrames() && clips.check())
        {
            if(mode&Info)
            {
                CClipList::iterator it(clips.begin());

                fprintf(stdout, "%s-%s-%i-%f", (*it).typeStr(),
                                               (*it).formatStr(),
                                               clips.totalFrames(),
                                               ((double)clips.totalFrames())/(*it).frameRate());
            }
            if(mode&Dv)
                clips.outputDv(dvFile);
            if(mode&(Subtitles|DvdAuthor|Wav|Yuv))
                clips.output(mode&Subtitles ? subFile : QString(),
                             mode&DvdAuthor ? dvdAuthorFile : QString(),
                             mode&Wav ? wavFile : QString(),
                             mode&Yuv ? yuvFile : QString(),
                             mode&Kmf ? kmfFile : QString(),
                             adjust);
            if(mode&SimpleSmil)
                clips.outputSmil(true, simpleSmilFile);
            if(mode&Smil)
                clips.outputSmil(false, smilFile);
            if(mode&CoverPic)
                clips.saveFrame(coverpicFile, false, true);
            if(mode&MenuPic)
                clips.saveFrame(menupicFile, false, false);
            if(mode&Spumux)
                clips.outputSpumux(spumuxFile, mode&Subtitles && "-"!=subFile ? subFile : QString());
            return clips.totalFrames();
        }
        else
            std::cerr << "Failed to load input file(s)" << std::endl;
    }

    return 0;
}
