/*
    Copyright 2013 Marcin Slusarz <marcin.slusarz@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <QApplication>
#include <QFile>
#include <QString>
#include "mainwin.h"

int main(int argc, char *argv[])
{
	QString path;
	QApplication app(argc, argv);
	if (argc > 1)
		path = QString(argv[1]);
	else
	{
		for (int i = 0; i < 9; ++i)
		{
			QString path_ = "/dev/ttyACM" + QString::number(i);
			if (QFile::exists(path_))
			{
				path = path_;
				break;
			}
		}
	}
	if (path.isEmpty())
		path = "/dev/tty?";

	MainWin win(path);
	win.show();
	return app.exec();
}
