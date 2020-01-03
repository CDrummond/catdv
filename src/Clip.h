#ifndef CLIP_H
#define CLIP_H

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

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <stdint.h>

class QFile;
class QTextStream;

class CClip
{
    public:

    enum Type
    {
        Pal,
        Ntsc
    };

    enum Format
    {
        Normal,
        Widescreen/*,
        Letterbox*/
    };

    static const int    constPalFrameSize;
    static const int    constNtscFrameSize;
    static const double constPalFps;
    static const double constNtscFps;

    CClip(const QString &fn=QString(), double f=-1.0, double t=-1, const QString &ch=QString());
    CClip(const QString &fn, int64_t f, int64_t t, const QString &ch=QString());
    ~CClip()                              { reset(); }

    bool isOk() const                     { return -1!=itsLength; }
    bool similar(const CClip &other) const;

    const QString & fileName() const      { return itsFileName; }
    const QString & chapter() const       { return itsChapter; }
    int64_t         from() const          { return itsFrom; }
    int64_t         to() const            { return itsTo; }
    int64_t         length() const        { return itsLength; }
    QString         duration() const;
    Type            type() const          { return itsType; }
    const char *    typeStr() const       { return Pal==itsType ? "pal" : "ntsc"; }
    Format          format() const        { return itsFormat; }
    const char *    formatStr() const     { return Normal==itsFormat ? "4:3" : "16:9"; }
    //bool            isProgressive() const { return itsProgressive; }
    double          frameRate() const     { return Pal==itsType ? constPalFps : constNtscFps; }
    int             frameSize() const     { return Pal==itsType ? constPalFrameSize : constNtscFrameSize; }
    void            reset();
    unsigned char * nextFrame();

    private:

    int64_t         init();

    private:

    int     itsFd;
    QString itsFileName,
            itsChapter;
    int64_t itsFrom,
            itsTo,
            itsCurrent,
            itsLength;
    Type    itsType;
    Format  itsFormat;
    //bool    itsProgressive;
};

class CClipList : public QList<CClip>
{
    public:

    static char *subtitleFormat;
    static bool displayProgress;
    static int  deinterlace;

    CClipList() : itsTotalFrames(0), itsCurrentClip(end()) { }
    CClipList(const QString &f)                            { load(f); }

    bool            save(const QString &file=QString()) const;
    bool            load(const QString &file);
    bool            addClip(const CClip &clip);
    void            clearList();
    bool            check() const;
    void            copy(const CClipList &other);
    int             totalFrames() const { return itsTotalFrames; }
    const QString & fileName() const    { return itsFileName; }
    QString         duration() const;

    void            outputSmil(bool simple, const QString &file);
    void            outputSpumux(const QString &file, const QString subFile=QString());
    void            output(const QString &subFile, const QString &dvdAuthorFile,
                           const QString &wavFile, const QString &yuvFile,
                           const QString &kmfFile, int adjust);
    void            outputDv(const QString &file);
    void            saveFrame(const QString &file, bool toGray, bool squareAspect);
    void            reset() { itsCurrentClip=begin(); itsEnd=end(); }
    unsigned char * nextFrame();

    private:

    void            saveToStream(QTextStream &str, bool simple) const;
    bool            loadKdenlive(const QString &file);
    bool            loadKino(const QString &file);
    bool            loadDv(const QString &file);
    const QString & currentChapterName(int64_t frame);

    private:

    int                     itsTotalFrames;
    iterator                itsCurrentClip,
                            itsEnd;
    mutable QString         itsFileName;
    QHash<int64_t, QString> itsChapters;
};

#endif
