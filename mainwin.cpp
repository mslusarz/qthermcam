#include "mainwin.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QSocketNotifier>
#include <QLineEdit>
#include <QTextEdit>
#include <QDesktopWidget>
#include <QRect>
#include <QStatusBar>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

MainWin::MainWin(QString path) : QMainWindow(), fd(-1), notifier(NULL), min_x(90), max_x(90), min_y(90), max_y(90),
		x(90), y(90), temp_object(-1000), temp_ambient(-1000)
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

	textEdit = new QTextEdit(central);
	textEdit->setReadOnly(true);
	textEdit->installEventFilter(this);
	centralLay->addWidget(textEdit);

	setStatusBar(new QStatusBar());

	central->setLayout(centralLay);

	QRect deskRect = QDesktopWidget().screenGeometry();
	setGeometry(100, 100, deskRect.width() - 200, deskRect.height() - 200);
}

void MainWin::log(const QString &txt)
{
	textEdit->append(txt);
}

void MainWin::doConnect()
{
	QString path = pathEdit->text();
	fd = open(path.toLocal8Bit().constData(), O_RDWR);
	if (fd == -1)
	{
		log(path + ": " + QString(strerror(errno)));
		return;
	}
	log(path + ": connected");
	statusBar()->showMessage("connected");

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

	log(pathEdit->text() + ": disconnected");
	statusBar()->showMessage("disconnected");
}

void MainWin::resetStatusBar()
{
	statusBar()->showMessage(QString("x: %1, y: %2, object: %3, ambient: %4")
			.arg(x).arg(y).arg(temp_object).arg(temp_ambient));
}

void MainWin::fdActivated(int)
{
	char c;
	int r = read(fd, &c, 1);
	if (r == 1)
	{
		if (c == '\n')
		{
			QString msg = QString(buffer);
			if (!msg.startsWith("I") && !msg.startsWith("E"))
				log("next message has invalid format!");
			else if (msg.startsWith("Idims:"))
			{
				QStringList dims = msg.split(":").takeLast().split(",");
				min_x = dims[0].toInt();
				max_x = dims[1].toInt();
				min_y = dims[2].toInt();
				max_y = dims[3].toInt();
			}
			else if (msg.startsWith("Ix: "))
			{
				bool ok;
				int tmpx = msg.mid(3).toInt(&ok);
				if (ok)
					x = tmpx;
				resetStatusBar();
			}
			else if (msg.startsWith("Iy: "))
			{
				bool ok;
				int tmpy = msg.mid(3).toInt(&ok);
				if (ok)
					y = tmpy;
				resetStatusBar();
			}
			else if (msg.startsWith("Itemp "))
			{
				bool ok;
				QString t = msg.mid(6);
				QStringList tt = t.split(":");

				float temp = tt[1].toFloat(&ok);
				if (ok)
				{
					if (tt[0] == QString("object"))
						temp_object = temp;
					else if (tt[0] == QString("ambient"))
						temp_ambient = temp;
					resetStatusBar();
				}
				else
					log("invalid temp format");
			}
			else if (msg.startsWith("Isetup finished"))
			{
				sendCommand("px90!py90!t!");
			}
			log("line: " + msg);
			buffer.truncate(0);
		}
		else if (c != '\r')
			buffer.append(c);
	}
}

void MainWin::sendCommand(QString cmd)
{
	int r = write(fd, cmd.toAscii().constData(), cmd.length());
	if (r == 0)
		log("cannot send command");
	else if (r < 0)
		log("write: " + QString(strerror(errno)));
	else
		log(QString("command '%1' sent: %2").arg(cmd).arg(r));
}

void MainWin::moveX(int newPos)
{
	if (newPos < min_x)
		newPos = min_x;
	if (newPos > max_x)
		newPos = max_x;
	sendCommand(QString("px%1!").arg(newPos));
}

void MainWin::moveY(int newPos)
{
	if (newPos < min_y)
		newPos = min_y;
	if (newPos > max_y)
		newPos = max_y;
	sendCommand(QString("py%1!").arg(newPos));
}

bool MainWin::eventFilter(QObject *obj, QEvent *_event)
{
	if (fd != -1 && _event->type() == QEvent::KeyPress)
	{
		QKeyEvent *event = static_cast<QKeyEvent *>(_event);
		int speed = 10;

		if ((event->modifiers() & Qt::ShiftModifier) && event->key() != Qt::Key_Shift)
			speed = 20;
		else if ((event->modifiers() & Qt::ControlModifier) && event->key() != Qt::Key_Control)
			speed = 1;
		else if ((event->modifiers() & Qt::AltModifier) && event->key() != Qt::Key_Alt)
			speed = 180;

		if (event->key() == Qt::Key_Up)
			moveY(y + speed);
		else if (event->key() == Qt::Key_Down)
			moveY(y - speed);
		else if (event->key() == Qt::Key_Left)
			moveX(x - speed);
		else if (event->key() == Qt::Key_Right)
			moveX(x + speed);
		else if (event->key() == Qt::Key_Space)
			sendCommand("t!");
		return true;
	}
	return QObject::eventFilter(obj, _event);
}
