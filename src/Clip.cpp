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

#include "Clip.h"
#include "Misc.h"
#include "Frame.h"
#include "Wav.h"
#include "YUV420Extractor.h"
#include "BufferedWriter.h"
#include <iostream>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QTime>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtXml/QDomNode>
#include <QtXml/QDomNodeList>
#include <QtGui/QImage>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"

static Frame frame;

const int    CClip::constPalFrameSize=144000;
const int    CClip::constNtscFrameSize=120000;
const double CClip::constPalFps=25.0;
const double CClip::constNtscFps=29.97;
char *       CClipList::subtitleFormat="%d/%m/%G|%H:%M:%S";
bool         CClipList::displayProgress=false;
int          CClipList::deinterlace=0;

static double toSeconds(const QString &s)
{
    // hh  :  mm  :  ss.sss
    int first=s.indexOf(':'),
        second=first>-1 ? s.indexOf(':', first+1) : -1;

    if(second>-1)
        return (s.left(first).toInt()*60*60)+
               (s.mid(first+1, 2).toInt()*60)+
               (s.mid(second+1).toDouble());

    return 0.0;
}

static QString toPath(const QString &d, const QString &f)
{
    return '/'!=f[0]
        ? d+QChar('/')+f
        : f;
}

static FILE * openFile(const QString &f)
{
    if("-"==f)
        return stdout;

    FILE *file=fopen(QFile::encodeName(f).constData(), "w");

    if(!file)
    {
        std::cerr << "ERROR: Failed to create " << QFile::encodeName(f).constData() << std::endl;
        exit(-1);
    }

    return file;
}

static void checkFile(CBufferedWriter *f)
{
    if(f && !*f)
    {
        std::cerr << "ERROR: Failed to create " << QFile::encodeName(f->name()).constData() << std::endl;
        exit(-1);
    }
}

static void closeFile(FILE *f)
{
    if(f)
    {
        fflush(f);
        if(f!=stdout)
            fclose(f);
    }
}

QString onlyDate(const QString &str)
{
    int pos=str.indexOf('|');

    return -1==pos ? str : str.left(pos);
}

QString onlyTime(const QString &str)
{
    int pos=str.indexOf('|');

    return -1==pos ? str : str.mid(pos+1);
}

QString timeStr(int64_t frames, double frameRate)
{
    double seconds=((double)frames)/frameRate;
    int    hrs=(seconds/3600.0),
           mins=((seconds-(hrs*3600.0))/60.0);
    double secs=(seconds-((hrs*3600.0)+(mins*60.0)));

    return QString().sprintf("%02d:%02d:%05.3f", hrs, mins, secs);
}

CClip::CClip(const QString &fn, double f, double t, const QString &ch)
     : itsFd(-1),
       itsFileName(fn),
       itsChapter(ch),
       itsFrom(-1),
       itsTo(-1),
       itsCurrent(-1),
       itsLength(-1)
{
    int64_t fSize=init();

    reset();
    if(fSize)
    {
        // Now check range...
        if(f>=0.0 && t>=0.0)
        {
            itsFrom=(int64_t)((f*frameRate())+0.5);
            itsTo=(int64_t)((t*frameRate())+0.5);

            int64_t fromByte=itsFrom*frameSize(),
                    toByte=itsTo*frameSize();

            if(fromByte<=fSize && toByte<=fSize)
                itsLength=(itsTo-itsFrom)+1; // from..to is inclusive!
        }
        else
        {
            itsFrom=0;
            itsTo=itsLength=fSize/frameSize();
        }
    }
}

CClip::CClip(const QString &fn, int64_t f, int64_t t, const QString &ch)
     : itsFd(-1),
       itsFileName(fn),
       itsChapter(ch),
       itsFrom(f),
       itsTo(t),
       itsCurrent(-1),
       itsLength(-1)
{
    int64_t fSize=init();

    reset();
    if(fSize)
    {
        // Now check range...
        if(itsFrom>=0 && itsTo>=0)
        {
            int64_t fromByte=itsFrom*frameSize(),
                    toByte=itsTo*frameSize();

            if(fromByte<=fSize && toByte<=fSize)
                itsLength=(itsTo-itsFrom)+1; // from..to is inclusive!
        }
        else
        {
            itsFrom=0;
            itsTo=itsLength=fSize/frameSize();
        }
    }
}

bool CClip::similar(const CClip &other) const
{
    return itsType==other.itsType && /*itsProgressive==other.itsProgressive &&*/ itsFormat==other.itsFormat;
}

QString CClip::duration() const
{
    return QTime().addSecs((int)((itsLength/frameRate())+0.5)).toString();
}

void CClip::reset()
{
    if(-1!=itsFd)
        close(itsFd);
    itsFd=-1;
    itsCurrent=-1;
}

unsigned char * CClip::nextFrame()
{
    if(!itsFileName.isEmpty())
    {
        if(itsFd<0)
        {
            itsFd=open64(QFile::encodeName(itsFileName).constData(), O_RDONLY);
            if(itsFd<0 || (0!=itsFrom && -1==lseek64(itsFd, frameSize()*itsFrom, SEEK_SET)))
            {
                close(itsFd);
                itsFd=-1;
            }
            itsCurrent=itsFrom;
        }

        if(itsFd>=0 && itsCurrent++<=itsTo)
        {
            // Speed up disk access by reading blocks of 'constMaxNumFrames' frames...
            static const int     constMaxNumFrames=100;
            static unsigned char frameBuffer[constMaxNumFrames*constPalFrameSize];
            static int64_t       bufferNumFrames=0;
            static int64_t       bufferPos=bufferNumFrames;

            if(++bufferPos<bufferNumFrames)
                return &frameBuffer[frameSize()*bufferPos];
            else
            {
                bufferPos=0;
                bufferNumFrames=(itsTo-itsCurrent)+2; // +2 as we increment current above!

                if(bufferNumFrames>constMaxNumFrames)
                    bufferNumFrames=constMaxNumFrames;

                int64_t toRead=frameSize()*bufferNumFrames;

                if(bufferNumFrames>0 && toRead==::read(itsFd, frameBuffer, toRead))
                    return &frameBuffer[0];
            }
        }

        reset();
    }

    return 0L;
}

int64_t CClip::init()
{
    int64_t size=0;
    if(!itsFileName.isEmpty())
    {
        int fd=open64(QFile::encodeName(itsFileName).constData(), O_RDONLY|O_LARGEFILE);

        if(-1!=fd)
        {
            unsigned char frameBuffer[constPalFrameSize];

            if(constPalFrameSize==::read(fd, frameBuffer, constPalFrameSize))
            {
                frame.data=frameBuffer;
                frame.ExtractHeader();
                itsType=frame.IsPAL() ? Pal : Ntsc;
                itsFormat=frame.IsWide() ? Widescreen : Normal;

                struct stat64 statbuf;
                if(0==fstat64(fd, &statbuf))
                    size=statbuf.st_size;
            }
            close(fd);
        }
    }
    return size;
}

bool CClipList::save(const QString &f) const
{
    if(!f.isEmpty() || !itsFileName.isEmpty())
    {
        QFile file(f.isEmpty() ? itsFileName : f);

        if(file.open(QIODevice::WriteOnly))
        {
            QTextStream str(&file);

            saveToStream(str, false);
            return true;
        }
    }

    return false;
}

bool CClipList::load(const QString &f)
{
    clearList();
    return (Misc::checkExt(f, "dv") && loadDv(f)) ||
           (Misc::checkExt(f, "kdenlive") && loadKdenlive(f)) ||
           ((Misc::checkExt(f, "smil") || Misc::checkExt(f, "kino")) && loadKino(f));
}

bool CClipList::addClip(const CClip &clip)
{
    if(count())
    {
        QList<CClip>::Iterator thisFirst(begin());

        if(!(*thisFirst).similar(clip))
            return false;
    }

    itsTotalFrames+=clip.length();
    append(clip);
    return true;
}

void CClipList::clearList()
{
    clear();
    itsTotalFrames=0;
    itsFileName=QString();
    reset();
}

bool CClipList::check() const
{
    QList<CClip>::ConstIterator first(begin()),
                                     e(end());

    if(first!=e)
    {
        QList<CClip>::ConstIterator it(first);

        ++it;
        for(; it!=e; ++it)
            if(!(*it).similar(*first))
                return false;
    }

    return true;
}

void CClipList::copy(const CClipList &other)
{
    *this=other;
    itsTotalFrames=other.totalFrames();
}

QString CClipList::duration() const
{
    QList<CClip>::ConstIterator first(begin());

    return QTime().addSecs(first!=end()
                            ? (int)((itsTotalFrames/(*first).frameRate())+0.5)
                            : 0)
                           .toString();
}

static int timeDiff(struct tm *a, struct tm *b, bool checkSeconds)
{
    return a->tm_year!=b->tm_year ||
           a->tm_mon!=b->tm_mon ||
           a->tm_mday!=b->tm_mday ||
           a->tm_hour!=b->tm_hour ||
           a->tm_min!=b->tm_min ||
           (checkSeconds && a->tm_sec!=b->tm_sec);
}

static const char * displayTime(CBufferedWriter *sub, int from, int to, struct tm *tm, int adjust)
{
    static time_t    t=time(NULL);
    static struct tm *now=localtime(&t);

    // Check that the timestamp is actually valid! 
    if (tm->tm_year<99 || tm->tm_year>now->tm_year || // Year needs to be in the range 1999->now+1 (+1 in case of leap year)
        tm->tm_hour<0 || tm->tm_hour>23 ||
        tm->tm_min<0 || tm->tm_min>59 ||
        tm->tm_sec<0 || tm->tm_sec>59)
        return 0L;

    static const int  constDateStrLen=64;
    static char       dateStr[constDateStrLen+1];

    tm->tm_hour+=adjust;
    strftime(dateStr, constDateStrLen, CClipList::subtitleFormat, tm);

    if(sub)
    {
        char subStr[256];
        sprintf(subStr, "{%d}{%d}%s\n", from, to, dateStr);
        sub->write((unsigned char *)subStr, strlen(subStr));
    }
    
    return dateStr;
}

void CClipList::outputSmil(bool simple, const QString &file)
{
    FILE        *f=openFile(file);
    QTextStream str(f, QIODevice::WriteOnly);
    saveToStream(str, simple);
    closeFile(f);
}

void CClipList::outputSpumux(const QString &file, const QString subFile)
{
    FILE              *f=openFile(file);
    QTextStream       str(f, QIODevice::WriteOnly);
    ConstIterator     firstClip(begin());
        
    str << "<subpictures>" << endl
        << "  <stream>" << endl
        << "    <textsub filename=\"";
    if(subFile.isEmpty())
        str << "SUB_FILE";
    else
        str << subFile;
    str << "\" characterset=\"ISO8859-1\" " << endl
        << "      fontsize=\"24.0\" ";
        //<< "font=\"" << fontFile << "\" ";
    str << endl
        << "      horizontal-alignment=\"left\" vertical-alignment=\"bottom\" " << endl
        << "      left-margin=\"60\" right-margin=\"60\" top-margin=\"20\" bottom-margin=\"30\" " << endl
        << "      subtitle-fps=\"1\" movie-fps=\"" << (*firstClip).frameRate() << "\" movie-width=\"720\" " << endl
        << "      movie-height=\"" << (CClip::Ntsc==(*firstClip).type() ? 480 : 576)-2 << "\" />" << endl
        << "  </stream>" << endl
        << "</subpictures>" << endl;
    closeFile(f);
}

struct Title
{
    Title(const QString &n, const QString &p) : name(n), pos(p) { }

    QString name,
            pos;
};

struct Chapter
{
    Chapter(const QString &s, int f=0) : str(s), frame(f) { }

    bool operator==(const Chapter &o) const
    {
        return str==o.str;
    }
    
    QString str;
    int     frame;
};

static QString toString(const QList<Chapter> &chapters)
{
    QString                       ch;
    QTextStream                   str(&ch, QIODevice::WriteOnly);    
    QList<Chapter>::ConstIterator it(chapters.begin()),
                                  end(chapters.end());
            
    for(; it!=end; ++it)
    {
        str << (*it).str;
        if((it+1)!=end)
            str << ",";
    }
    
    return ch;
}

void CClipList::output(const QString &subFile, const QString &dvdAuthorFile,
                       const QString &wavFile, const QString &yuvFile, const QString &kmfFile, int adjust)
{
    int64_t         frameCount(0),
                    lastFrame(0);
    struct tm       now;
    struct tm       lastTime;
    Wav             *wavExp=0L;
    YUV420Extractor *yuvExp=0L;
    FILE            *dvda=!dvdAuthorFile.isEmpty() ? openFile(dvdAuthorFile) : 0L,
                    *dvdc=dvda ? openFile(dvdAuthorFile+".chapters") : 0L,
                    *dvdt=dvda ? openFile(dvdAuthorFile+".title") : 0L,
                    *kmf=!kmfFile.isEmpty() ? openFile(kmfFile) : 0L;
    CBufferedWriter *wav=!wavFile.isEmpty() ? new CBufferedWriter(wavFile) : 0L,
                    *yuv=!yuvFile.isEmpty() ? new CBufferedWriter(yuvFile) : 0L,
                    *sub=!subFile.isEmpty() ? new CBufferedWriter(subFile) : 0L;
    QString         startDateTime,
                    endDateTime;
    ConstIterator   firstClip(begin());
    double          frameRate((*firstClip).frameRate());
    int             chapterGap=(itsTotalFrames<(4800*frameRate) ? 120 : 300)*frameRate, // If less than 80 mins, do chapter len=2mins, else 5mins.
                    minChapterGap=15*frameRate,
                    lastchapterFrame=-1;
    QList<Chapter>  chapters;
    QString         chapterName;
    QList<Title>    titles;
    int             currentProgress=0,
                    lastProgress=-1;
    FILE            *stdErr=displayProgress ? stderr : 0,
                    *devNull=fopen("/dev/null", "w");
    int             start=time(NULL);
    bool            secondsInSubtitles=sub && subtitleFormat && NULL!=strstr(subtitleFormat, "%S");

    frame.decoder->audio->error_log=devNull;
    frame.decoder->video->error_log=devNull;

    if(displayProgress)
        stderr=devNull;

    reset();

    checkFile(sub);
    checkFile(wav);
    checkFile(yuv);

    if(displayProgress)
        fprintf(stdErr, "  0%%     0fps");

    while((frame.data=nextFrame()))
    {
        frame.ExtractHeader();
                
        if((sub || dvda || kmf) && frame.GetRecordingDate(now) && timeDiff(&now, &lastTime, secondsInSubtitles))
        {
            const char *date=displayTime(sub, lastFrame, frameCount+1, &lastTime, adjust);

            if(date)
            {
                lastFrame=frameCount+1;
                if(dvda)
                {
                    if(startDateTime.isEmpty())
                        startDateTime=date;
                    endDateTime=date;
                }
            }
            memcpy(&lastTime, &now, sizeof(struct tm));
        }

        if(dvda || kmf)
        {
            QString cName(currentChapterName(frameCount));
            bool    emptyName=cName.isEmpty(),
                    diffName=!emptyName && cName!=chapterName;

            if(0==frameCount%chapterGap || diffName)
            {
                QString chapterStr(timeStr(frameCount, frameRate));
                bool    tooClose=lastchapterFrame>-1 && (frameCount-lastchapterFrame)<minChapterGap,
                        prevIsTitle=!titles.isEmpty() && chapters.indexOf(Chapter(titles.last().pos))==chapters.size()-1;

                if(tooClose && prevIsTitle && emptyName)
                    ;
                else
                {
                    if(tooClose && diffName && !prevIsTitle)
                        chapters.removeLast();

                    if((!tooClose || diffName) && !chapters.contains(Chapter(chapterStr)))
                    {
                        chapters.append(Chapter(chapterStr, frameCount));
                        lastchapterFrame=frameCount;
                    }

                    if(diffName)
                    {
                        chapterName=cName;
                        titles.append(Title(chapterName, chapterStr));
                    }
                }
            }
        }

        if(wav)
        {
            if(!wavExp)
            {
                wavExp = new Wav(*wav);
                if(!wavExp->Initialise(frame))
                {
                    std::cerr << "Failed to initialise audio file " << QFile::encodeName(wavFile).constData() << std::endl;
                    return;
                }
            }

            wavExp->Output(frame);
        }

        if(yuv)
        {
            if(!yuvExp)
            {
                yuvExp = YUV420Extractor::GetExtractor(*yuv, deinterlace);
                if(!yuvExp->Initialise(frame))
                {
                    std::cerr << "Failed to initialise yuv file " << QFile::encodeName(yuvFile).constData() << std::endl;
                    return;
                }
            }

            yuvExp->Output(frame);
        }
        
        frameCount++;

        if(displayProgress && itsTotalFrames)
        {
            currentProgress=(frameCount*100)/itsTotalFrames;
            if(currentProgress!=lastProgress)
            {
                int diff=time(NULL)-start;
                lastProgress=currentProgress;
                
                fprintf(stdErr, "\b\b\b\b\b\b\b\b\b\b\b\b\b%3d%%  %4dfps", currentProgress, diff ? frameCount/diff : frameCount);
            }
        }
    }

    if(displayProgress)
    {
        fprintf(stdErr, "\n");
        stderr=stdErr;
    }

    if(devNull)
        fclose(devNull);

    if((sub || dvda || kmf) && frameCount && lastFrame<totalFrames())
    {
        const char *date=displayTime(sub, lastFrame, totalFrames(), &lastTime, adjust);

        if(date && dvda)
        {
            if(startDateTime.isEmpty())
                startDateTime=date;
            endDateTime=date;
        }
    }

    if(kmf)
    {
        QTextStream str(kmf, QIODevice::WriteOnly);
        QString     startDate(onlyDate(startDateTime)),
                    endDate(onlyDate(endDateTime)),
                    title;

        if(startDate==endDate)
        {
            QString startTime(onlyTime(startDateTime)),
                    endTime(onlyTime(endDateTime));

            if(startTime==endTime)
                title=startDate;
            else
                title=startDate + " (" + startTime + " - " + endTime + ')';
        }
        else
            title=startDate + " - " + endDate;

        str << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl
            << "<!DOCTYPE kmf_project>" << endl
            << "<project title=\"" << title << "\" serial=\"1\" dir=\"OUTPUT_DIR\" type=\"DVD-PAL\" >" << endl
            << "  <media plugin=\"KMFImportVideo\" object=\"video\" >"  << endl
            << "    <video title=\"" << title << "\" aspect=\"1\" id=\"001_untitled\" >" << endl
            << "      <file path=\"VOB_FILE\" />" << endl;

        QList<Chapter>::ConstIterator it(chapters.begin()),
                                      end(chapters.end());
                                   
        for(int i=1; it!=end; ++it, ++i)
        {
            int next=(it+1)==end ? frameCount : (*(it+1)).frame;
            str << "        <cell length=\"" << timeStr(next-(*it).frame, frameRate) << "\" chapter=\"1\" name=\"" << i << "\" start=\"" << (*it).str << "\" />" << endl;
        }

        str << "        <audio language=\"en\" />" << endl;
        if(!subFile.isEmpty())
            str << "        <subtitle encoding=\"UTF-8\" language=\"en\" file=\"" << subFile << "\" align=\"65\" margins=\"80,80,20,30\">" << endl
                << "          <font size=\"28\" weight=\"750\" name=\"Courier New\" />" << endl
                << "        </subtitle>" << endl;
        str << "      </video>" << endl
            << "    </media>" << endl
            << "  <template plugin=\"KMFTemplateEngine\" object=\"Plain\" >" << endl
            << "    <custom_properties>" << endl
            << "      <properties widget=\"%KMFMenuPage%\" >" << endl
            << "        <property value=\"\" name=\"sound\" />" << endl
            << "        <property value=\"en\" name=\"language\" />" << endl
            << "        <property value=\"false\" name=\"direct_play\" />" << endl
            << "        <property value=\"1\" name=\"continue_to_next_title\" />" << endl
            << "      </properties>" << endl
            << "      <properties widget=\"background\" >" << endl
            << "        <property value=\"file://" CMAKE_INSTALL_PREFIX "/share/dv2dvd/images/PurpleAir.png\" name=\"background::url\" />" << endl
            << "      </properties>" << endl
            << "      <properties widget=\"label_.*\" >" << endl
            << "        <property value=\"#ffffff\" name=\"label::color\" />" << endl
            << "      </properties>" << endl
            << "      <properties widget=\"label_main\" >" << endl
            << "        <property value=\"Verdana,32,-1,5,75,0,0,0,0,0\" name=\"main::font\" />" << endl
            << "      </properties>" << endl
            << "      <properties widget=\"label_title.*\" >" << endl
            << "        <property value=\"Verdana,22,-1,5,50,0,0,0,0,0\" name=\"title::font\" />" << endl
            << "      </properties>" << endl
            << "    </custom_properties>" << endl
            << "  </template>" << endl
            << "  <output plugin=\"KMFOutput\" object=\"k3b\" />" << endl
            << "</project>" << endl;
    }

    if(dvda)
    {
        QTextStream                 str(dvda, QIODevice::WriteOnly),
                                    chap(dvdc, QIODevice::WriteOnly),
                                    title(dvdt, QIODevice::WriteOnly);
        QString                     buttons,
                                    registers,
                                    start(timeStr(0, frameRate));
        QList<Title>::ConstIterator it(titles.begin()),
                                    end(titles.end());

        chap << /*i18n(*/"Play"/*)*/ << endl;

        if(1!=titles.size() || (*it).pos!=timeStr(0, frameRate))
            for(int i=1; it!=end; ++it, ++i)
            {
                int cNum=chapters.indexOf(Chapter((*it).pos))+1;
                chap <<cNum << "##" << (*it).name << endl;
                buttons+=QString().sprintf("        <button> {g3=%d; jump title 1;} </button>\n", i);
                registers+=QString().sprintf("                if(g3 eq %d) jump chapter %d ;\n", i, cNum);
            }
        
        str << "<?xml version=\"1.0\"?>" << endl
            << "<dvdauthor>" << endl
            << "  <vmgm>" << endl
            << "    <menus>" << endl
            << "      <video format=\"" << (*firstClip).typeStr() << "\" aspect=\"" << (*firstClip).formatStr() << '\"'
                                        << (const char *)(CClip::Widescreen==(*firstClip).format() ? " widescreen=\"nopanscan\"/>" : "/>") << endl
            << "      <pgc entry=\"title\" >" << endl
            << "        <pre>{ button=1024; g3=0; } </pre>" << endl
            << "        <vob file=\"MENU_VOB\" pause=\"inf\"/>" << endl
            << "        <button> {g3=0; jump title 1;} </button>" << endl;
        if(!buttons.isEmpty())
            str << buttons;
        str << "      </pgc>" << endl
            << "    </menus>" << endl
            << "  </vmgm>" << endl
            << "  <titleset>" << endl
            << "    <titles>" << endl
            << "      <video format=\"" << (*firstClip).typeStr() << "\" aspect=\"" << (*firstClip).formatStr() << '\"'
                                        << (const char *)(CClip::Widescreen==(*firstClip).format() ? " widescreen=\"nopanscan\"/>" : "/>") << endl
            << "      <subpicture lang=\"en\"/>" << endl
            << "      <pgc>" << endl
            << "        <pre>" << endl
            << "            {" << endl
            << "                subtitle=62;" << endl
            << "                if(g3 eq 0) jump chapter 1 ;" << endl;
        if(!registers.isEmpty())
            str << registers;
        str << "            }" << endl
            << "        </pre>" << endl
            << "        <vob file=\"VIDEO_VOB\" chapters=\"" << toString(chapters) << "\" pause=\"30\"/>" << endl
            << "        <post> call vmgm menu 1; </post>" << endl
            << "      </pgc>" << endl
            << "    </titles>" << endl
            << "  </titleset>" << endl
            << "</dvdauthor>" << endl;

        QString startDate(onlyDate(startDateTime)),
                endDate(onlyDate(endDateTime));

        if(startDate==endDate)
        {
            QString startTime(onlyTime(startDateTime)),
                    endTime(onlyTime(endDateTime));

            if(startTime==endTime)
                title << startDate;
            else
                title << startDate << " (" << startTime << " - " << endTime << ')';
        }
        else
            title << startDate << " - " << endDate;
    }

    if(wavExp)
        wavExp->Flush();
    if(yuvExp)
        yuvExp->Flush();
    closeFile(dvda);
    closeFile(dvdc);
    closeFile(dvdt);
    closeFile(kmf);
    delete yuv;
    delete wav;
    delete sub;
    delete yuvExp;
    delete wavExp;
}

void CClipList::outputDv(const QString &file)
{
    CBufferedWriter out(file);
    unsigned char   *data=0L;
    ConstIterator   firstClip(begin());
    int             frameSize((*firstClip).frameSize());
    
    checkFile(&out);
    reset();

    while((data=nextFrame()))
        out.write(data, frameSize);
}

void CClipList::saveFrame(const QString &file, bool toGray, bool squareAspect)
{
    reset();

    if((frame.data=nextFrame()))
    {
        ConstIterator firstClip(begin());
        unsigned char rgb[720 * 576 * 3],
                      bgra[720 * 576 * 4];
        int           s,
                      d;

        frame.ExtractHeader();
        frame.ExtractPreviewRGB(rgb, CClip::Ntsc!=(*firstClip).type());

        for(s=0, d=0; s<frame.GetWidth() * frame.GetHeight() * 3; s+=3, d+=4)
        {
            bgra[d+3]=0xFF;
            if(toGray)
                bgra[d]=bgra[d+1]=bgra[d+2]=(0.3*rgb[s]) + (0.59*rgb[s+1]) + (0.11*rgb[s+2]);
            else
            {
                bgra[d]=rgb[s+2];
                bgra[d+1]=rgb[s+1];
                bgra[d+2]=rgb[s];
            }
        }
        QImage image(bgra, frame.GetWidth(), frame.GetHeight(), QImage::Format_RGB32);

        if(squareAspect)
        {
            QSize size(CClip::Normal==(*firstClip).format()
                        ? CClip::Ntsc==(*firstClip).type()
                            ? QSize(720, 540)
                            : QSize(768, 576)
                        : CClip::Ntsc==(*firstClip).type()
                            ? QSize(853, 480)
                            : QSize(1024, 576));

            image=image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        image.save(file);
    }
    else
    {
        std::cerr << "ERROR: Failed to parse 1st frame";
        exit(-2);
    }
}

unsigned char * CClipList::nextFrame()
{
    unsigned char *frame=0L;

    if(itsEnd!=itsCurrentClip)
    {
        frame=(*itsCurrentClip).nextFrame();

        if(!frame)
        {
            itsCurrentClip++;
            frame=nextFrame();
        }
    }

    return frame;
}

void CClipList::saveToStream(QTextStream &str, bool simple) const
{
    str << "<?xml version=\"1.0\"?>" << endl
        << "<smil xmlns=\"http://www.w3.org/2001/SMIL20/Language\"";
    if(simple) // smil2yuv barfs without this...
        str << " xmlns:smil2=\"http://www.w3.org/2001/SMIL20/Language\"";
    str << '>' << endl;

    if(simple)
        str << "  <seq>" << endl;

    QList<CClip>::ConstIterator it(begin()),
                                e(end());

    for(; it!=e; ++it)
    {
        if(!simple)
            str << "  <seq>" << endl;
        str << "    <video src=\"" << (*it).fileName() << "\" clipBegin=\"" << (*it).from() << "\" clipEnd=\"" << (*it).to() << "\"/>" << endl;
        if(!simple)
            str << "  </seq>" << endl;
    }

    if(simple)
        str << "  </seq>" << endl;
    str << "</smil>" << endl;
}

typedef QHash<QString, QString> KdenliveIdFileMap;

bool CClipList::loadKdenlive(const QString &f)
{
    QDomDocument doc("kdenlive");
    QFile        file(f);

    if(file.open(QIODevice::ReadOnly) && doc.setContent(&file))
    {
        QDomNodeList kdenlivedoc(doc.elementsByTagName("kdenlivedoc"));
        double       fps(0.0);

        if(1==kdenlivedoc.count())
        {
            QDomNode     kdenlivedocNode(kdenlivedoc.item(0));
            QDomElement  kdenlivedocElem(kdenlivedocNode.toElement());
            QDomNodeList profileinfo(kdenlivedocElem.elementsByTagName("profileinfo"));

            if(1==profileinfo.count())
            {
                QDomNode    profileinfoNode(profileinfo.item(0));
                QDomElement profileinfoElem(profileinfoNode.toElement());

                if(profileinfoElem.hasAttribute("frame_rate_num") && profileinfoElem.hasAttribute("frame_rate_den"))
                {
                    fps=profileinfoElem.attribute("frame_rate_num").toDouble()/
                        profileinfoElem.attribute("frame_rate_den").toDouble();

                    if(fps>5.0)
                    {
                        // Extract chapters...
                        QDomNodeList guides(kdenlivedocElem.elementsByTagName("guides"));

                        if(1==guides.count())
                        {
                            QDomNode     guidesNode(guides.item(0));
                            QDomElement  guidesElem(guidesNode.toElement());
                            QDomNodeList guideList(doc.elementsByTagName("guide"));

                            for(int i=0; i<guideList.count(); ++i)
                            {
                                QDomNode    guideNode(guideList.item(i));
                                QDomElement guideElem(guideNode.toElement());

                                if(guideElem.hasAttribute("comment") && guideElem.hasAttribute("time"))
                                    itsChapters[(int64_t)(guideElem.attribute("time").toDouble()*fps)]=guideElem.attribute("comment");
                            }
                        }
                    }
                }
            }

            if(fps<5.0)
                return false;

            QDomNodeList mlt(doc.elementsByTagName("mlt"));

            if(1==mlt.count())
            {
                QDomNode    mltNode(mlt.item(0));
                QDomElement mltElem(mltNode.toElement());

                if(mltElem.hasAttribute("root"))
                {
                    QString root(mltElem.attribute("root"));
                    
                    // Extract filelist...
                    QDomNodeList      producers(doc.elementsByTagName("producer"));
                    KdenliveIdFileMap ids;

                    for(int i=0; i<producers.count(); ++i)
                    {
                        QDomNode     producerNode(producers.item(i));
                        QDomElement  producerElem(producerNode.toElement());

                        if(producerElem.hasAttribute("id"))
                        {
                            QDomNodeList properties(producerElem.elementsByTagName("property"));

                            for(int j=0; j<properties.count(); ++j)
                            {
                                QDomNode    propertyNode(properties.item(j));
                                QDomElement propertyElem(propertyNode.toElement());

                                if(propertyElem.hasAttribute("name") &&
                                    "resource"==propertyElem.attribute("name"))
                                {
                                    QString resource(propertyElem.text());
                                    
                                    ids[producerElem.attribute("id")]='/'==resource[0]
                                                                        ? resource
                                                                        : (root+'/'+resource);
                                    break;
                                }
                            }
                        }
                    }

                    if(ids.count())
                    {
                        // Extract timeline...
                        QDomNodeList playlists(doc.elementsByTagName("playlist"));

                        for(int i=0; i<playlists.count(); ++i)
                        {
                            QDomNode    playlistNode(playlists.item(i));
                            QDomElement playlistElem(playlistNode.toElement());

                            if(playlistElem.hasAttribute("id") &&
                                "black_track"!=playlistElem.attribute("id"))
                            {
                                QDomNodeList parts(playlistNode.childNodes());

                                if(parts.count())
                                {
                                    for(int j=0; j<parts.count(); ++j)
                                    {
                                        QDomNode    partNode(parts.item(j));
                                        QDomElement partElem(partNode.toElement());

                                        if("entry"==partElem.tagName())
                                        {
                                            if(partElem.hasAttribute("in") &&
                                                partElem.hasAttribute("out") &&
                                                partElem.hasAttribute("producer"))
                                            {
                                                CClip clip(ids[partElem.attribute("producer")],
                                                            (int64_t)partElem.attribute("in").toLongLong(),
                                                            (int64_t)partElem.attribute("out").toLongLong());

                                                if(clip.isOk())
                                                {
                                                    append(clip);
                                                    itsTotalFrames+=clip.length();
                                                }
                                            }
                                        }
                                        //else if("blank"==partElem.tagName())
                                    }
                                    // For now, only handle 1 track...
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return 0!=itsTotalFrames;
}

bool CClipList::loadKino(const QString &f)
{
    QDomDocument doc("kino");
    QFile        file(f);

    if(file.open(QIODevice::ReadOnly) && doc.setContent(&file))
    {
        QDomNodeList videos(doc.elementsByTagName("video"));
        QString      dir(QFileInfo(file).absoluteDir().path());

        for(int i=0; i<videos.count(); ++i)
        {
            QDomNode    videoNode(videos.item(i));
            QDomElement videoElement(videoNode.toElement());

            if(videoElement.hasAttribute("src") && videoElement.hasAttribute("clipBegin") && videoElement.hasAttribute("clipEnd"))
            {
                if(-1!=videoElement.attribute("clipBegin").indexOf(':'))
                {
                    CClip clip(toPath(dir, videoElement.attribute("src")),
                               toSeconds(videoElement.attribute("clipBegin")),
                               toSeconds(videoElement.attribute("clipEnd")),
                               videoNode.parentNode().toElement().attribute("title"));

                    if(clip.isOk())
                    {
                        append(clip);
                        itsTotalFrames+=clip.length();
                    }
                }
                else
                {
                    CClip clip(toPath(dir, videoElement.attribute("src")),
                               (int64_t)videoElement.attribute("clipBegin").toLongLong(),
                               (int64_t)videoElement.attribute("clipEnd").toLongLong(),
                               videoNode.parentNode().toElement().attribute("title"));

                    if(clip.isOk())
                    {
                        append(clip);
                        itsTotalFrames+=clip.length();
                    }
                }
            }
        }
    }

    if(0!=itsTotalFrames)
        itsFileName=f;
    return 0!=itsTotalFrames;
}

bool CClipList::loadDv(const QString &f)
{
    CClip clip(QFileInfo(f).absoluteFilePath());

    if(clip.isOk())
    {
        append(clip);
        itsTotalFrames+=clip.length();
    }

    return 0!=itsTotalFrames;
}

const QString & CClipList::currentChapterName(int64_t frame)
{
    return (*itsCurrentClip).chapter().isEmpty() && itsChapters.contains(frame)
            ? itsChapters[frame]
            : (*itsCurrentClip).chapter();
}
