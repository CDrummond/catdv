#ifndef _BUFFERED_FILE_
#define _BUFFERED_FILE_

#include <QtCore/QString>

class CBufferedWriter
{
    public:
    
    CBufferedWriter(const QString &name);
    ~CBufferedWriter();

    operator bool()    { return -1!=itsFd; }

    const QString & name() const { return itsName; }
    bool write(unsigned char data);
    bool write(unsigned char *data, unsigned int size);
    bool flush();
    bool seekToStart();

    private:
    
    int           itsFd;
    QString       itsName;
    unsigned int  itsCurrentPos;
    unsigned char *itsBuffer;
};

#endif
