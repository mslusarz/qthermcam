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
