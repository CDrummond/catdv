#ifndef MISC_H
#define MISC_H

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

class QString;

namespace Misc
{
    extern bool checkExt(const QString &fname, const QString &ext);
    extern bool check(const QString &path, bool file, bool checkW);
    inline bool fExists(const QString &p)   { return check(p, true, false); }
    inline bool dExists(const QString &p)   { return check(p, false, false); }
    inline bool fWritable(const QString &p) { return check(p, true, true); }
    inline bool dWritable(const QString &p) { return check(p, false, true); }
    extern QString dirSyntax(const QString &d);
    extern QString getFile(const QString &d);
    extern QString getDir(const QString &d);
}

#endif

