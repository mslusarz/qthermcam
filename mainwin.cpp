#include "mainwin.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QSocketNotifier>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

MainWin::MainWin(const char *_addr) : QMainWindow(), fd(-1), notifier(NULL), addr(_addr)
{
	if (!addr)
		addr = "/dev/ttyACM0";
	QWidget *w = new QWidget(this);
	setCentralWidget(w);
	connectButton = new QPushButton("connect");
	disconnectButton = new QPushButton("disconnect");
	disconnectButton->setEnabled(false);
	connect(connectButton,    SIGNAL(clicked()), this, SLOT(doConnect()));
	connect(disconnectButton, SIGNAL(clicked()), this, SLOT(doDisconnect()));
	QHBoxLayout *lay = new QHBoxLayout();
	lay->addWidget(connectButton);
	lay->addWidget(disconnectButton);
	w->setLayout(lay);
}

void MainWin::doConnect()
{
	fd = open(addr, O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return;
	}
	notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(fdActivated(int)));

	connectButton->setEnabled(false);
	disconnectButton->setEnabled(true);
}

void MainWin::doDisconnect()
{
	disconnect(notifier, NULL, this, NULL);
	delete notifier;
	::close(fd);
	fd = -1;

	connectButton->setEnabled(true);
	disconnectButton->setEnabled(false);
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

void MainWin::handleEvent(QKeyEvent *event)
{
	if (fd == -1)
		return;

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
}

void MainWin::keyPressEvent(QKeyEvent *event)
{
	handleEvent(event);
	QMainWindow::keyPressEvent(event);
}

void MainWin::keyReleaseEvent(QKeyEvent *event)
{
	QMainWindow::keyReleaseEvent(event);
}

