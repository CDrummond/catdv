#include "BufferedWriter.h"
#include <QtCore/QFile>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const int constBufferSize=10*1024*1024;
static bool flushed=false;

CBufferedWriter::CBufferedWriter(const QString &name)
               : itsFd("-"==name ? fileno(stdout) : open(QFile::encodeName(name).constData(), O_RDWR|O_CREAT|O_TRUNC, 0644)),
                 itsName(name),
                 itsCurrentPos(0),
                 itsBuffer(itsFd ? new unsigned char [constBufferSize] : 0L)
{
}

CBufferedWriter::~CBufferedWriter()
{
    flush();
    if("-"!=itsName)
        close(itsFd);
    delete itsBuffer;
}

bool CBufferedWriter::write(unsigned char data)
{
    if(itsCurrentPos+1>=constBufferSize && !flush())
        return false;
    itsBuffer[itsCurrentPos++]=data;
    return true;
}

bool CBufferedWriter::write(unsigned char *data, unsigned int size)
{
    if(itsCurrentPos+size>=constBufferSize && !flush())
        return false;

    if(size>constBufferSize)
        return size==::write(itsFd, data, size);

    memcpy(&itsBuffer[itsCurrentPos], data, size);
    itsCurrentPos+=size;
    return true;
}

bool CBufferedWriter::flush()
{
    if(itsCurrentPos && itsCurrentPos!=::write(itsFd, itsBuffer, itsCurrentPos))
        return false;

    itsCurrentPos=0;
    return true;
}

bool CBufferedWriter::seekToStart()
{
    return "-"!=itsName && flush() && 0==lseek(itsFd, 0, SEEK_SET);
}
