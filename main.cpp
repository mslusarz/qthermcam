#include <QApplication>
#include "mainwin.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	MainWin win(argc > 1 ? argv[1] : NULL);
	win.show();
	return app.exec();
}
