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

#include "Misc.h"
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Misc
{

bool checkExt(const QString &fname, const QString &ext)
{
    QString extension('.'+ext);

    return fname.length()>extension.length()
            ? 0==fname.mid(fname.length()-extension.length()).compare(extension)
            : false;
}

bool check(const QString &path, bool file, bool checkW)
{
    struct stat info;
    QByteArray  pathC(QFile::encodeName(path));

    return 0==lstat(pathC, &info) &&
           (file ? (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))
                 : S_ISDIR(info.st_mode)) &&
           (!checkW || 0==::access(pathC, W_OK));
}

QString dirSyntax(const QString &d)
{
    if(!d.isEmpty())
    {
        QString ds(d);

        ds.replace("//", "/");

        int slashPos(ds.lastIndexOf('/'));

        if(slashPos!=(((int)ds.length())-1))
            ds.append('/');

        return ds;
    }

    return d;
}

QString getDir(const QString &f)
{
    QString d(f);

    int slashPos(d.lastIndexOf('/'));

    if(slashPos!=-1)
        d.remove(slashPos+1, d.length());

    return dirSyntax(d);
}

QString getFile(const QString &f)
{
    QString d(f);

    int slashPos=d.lastIndexOf('/');

    if(slashPos!=-1)
        d.remove(0, slashPos+1);

    return d;
}

}
