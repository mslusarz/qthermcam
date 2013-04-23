#include "mainwin.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QSocketNotifier>
#include <QLineEdit>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

MainWin::MainWin(QString path) : QMainWindow(), fd(-1), notifier(NULL)
{
	QWidget *central = new QWidget(this);
	setCentralWidget(central);
	pathEdit = new QLineEdit(path, central);
	pathEdit->installEventFilter(this);
	QVBoxLayout *centralLay = new QVBoxLayout();
	centralLay->addWidget(pathEdit);

	QWidget *buttons = new QWidget(central);
	connectButton = new QPushButton("connect", buttons);
	disconnectButton = new QPushButton("disconnect", buttons);
	disconnectButton->setEnabled(false);
	connectButton->installEventFilter(this);
	disconnectButton->installEventFilter(this);

	connect(connectButton,    SIGNAL(clicked()), this, SLOT(doConnect()));
	connect(disconnectButton, SIGNAL(clicked()), this, SLOT(doDisconnect()));

	QHBoxLayout *buttonsLay = new QHBoxLayout();
	buttonsLay->addWidget(connectButton);
	buttonsLay->addWidget(disconnectButton);
	buttons->setLayout(buttonsLay);

	centralLay->addWidget(buttons);
	central->setLayout(centralLay);
}

void MainWin::doConnect()
{
	fd = open(pathEdit->text().toLocal8Bit().constData(), O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return;
	}
	notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(fdActivated(int)));

	connectButton->setEnabled(false);
	disconnectButton->setEnabled(true);
	pathEdit->setEnabled(false);
}

void MainWin::doDisconnect()
{
	disconnect(notifier, NULL, this, NULL);
	delete notifier;
	::close(fd);
	fd = -1;

	connectButton->setEnabled(true);
	disconnectButton->setEnabled(false);
	pathEdit->setEnabled(true);
}

void MainWin::fdActivated(int)
{
	char c;
	int r = read(fd, &c, 1);
	if (r == 1)
		printf("%c", c);
}

void MainWin::sendCommand(char cmd)
{
	int r = write(fd, &cmd, 1);
	if (r == 0)
		fprintf(stderr, "huh?\n");
	else if (r < 0)
		perror("write");
	else
		printf("command '%c' sent: %d\n", cmd, r);
}

bool MainWin::eventFilter(QObject *obj, QEvent *_event)
{
	if (fd != -1 && _event->type() == QEvent::KeyPress)
	{
		QKeyEvent *event = static_cast<QKeyEvent *>(_event);
		if (event->modifiers() & Qt::ShiftModifier)
			sendCommand('9');
		else if (event->modifiers() & Qt::ControlModifier)
			sendCommand('1');
		else if (event->modifiers() & Qt::AltModifier)
			sendCommand('x');

		if (event->key() == Qt::Key_Up)
			sendCommand('u');
		else if (event->key() == Qt::Key_Down)
			sendCommand('d');
		else if (event->key() == Qt::Key_Left)
			sendCommand('l');
		else if (event->key() == Qt::Key_Right)
			sendCommand('r');
		else if (event->key() == Qt::Key_Space)
			sendCommand('t');
		return true;
	}
	return QObject::eventFilter(obj, _event);
}
