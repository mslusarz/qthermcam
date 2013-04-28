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
#include <QSpinBox>
#include <QSplitter>
#include <QLabel>
#include <QSizePolicy>
#include <QImage>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

MainWin::MainWin(QString path) : QMainWindow(), fd(-1), notifier(NULL), min_x(90), max_x(90), min_y(90), max_y(90),
		x(90), y(90), temp_object(-1000), temp_ambient(-1000), scanInProgress(false), values(NULL), curMin(0),
		curMax(0), image(NULL)
{
	QWidget *central = new QWidget(this);
	setCentralWidget(central);
	pathEdit = new QLineEdit(path, central);
	pathEdit->installEventFilter(this);
	QVBoxLayout *centralLay = new QVBoxLayout();
	centralLay->addWidget(pathEdit);

	QWidget *buttons = new QWidget(central);

	connectButton = new QPushButton("connect", buttons);
	connectButton->installEventFilter(this);
	connect(connectButton,    SIGNAL(clicked()), this, SLOT(doConnect()));

	disconnectButton = new QPushButton("disconnect", buttons);
	disconnectButton->setEnabled(false);
	disconnectButton->installEventFilter(this);
	connect(disconnectButton, SIGNAL(clicked()), this, SLOT(doDisconnect()));

	QPushButton *clearButton = new QPushButton("clear", buttons);
	connect(clearButton, SIGNAL(clicked()), this, SLOT(clearLog()));

	scanButton = new QPushButton("scan image", buttons);
	scanButton->setEnabled(false);
	scanButton->installEventFilter(this);
	connect(scanButton, SIGNAL(clicked()), this, SLOT(scanImage()));

	minX = new QSpinBox(buttons);
	minX->setPrefix("min x: ");
	minX->setRange(0, 180);
	minX->setEnabled(false);

	maxX = new QSpinBox(buttons);
	maxX->setPrefix("max x: ");
	maxX->setRange(0, 180);
	maxX->setValue(180);
	maxX->setEnabled(false);

	minY = new QSpinBox(buttons);
	minY->setPrefix("min y: ");
	minY->setRange(0, 180);
	minY->setEnabled(false);

	maxY = new QSpinBox(buttons);
	maxY->setPrefix("max y: ");
	maxY->setRange(0, 180);
	maxY->setValue(180);
	maxY->setEnabled(false);

	QHBoxLayout *buttonsLay = new QHBoxLayout();
	buttonsLay->addWidget(connectButton);
	buttonsLay->addWidget(disconnectButton);
	buttonsLay->addWidget(clearButton);
	buttonsLay->addWidget(scanButton);
	buttonsLay->addWidget(minX);
	buttonsLay->addWidget(maxX);
	buttonsLay->addWidget(minY);
	buttonsLay->addWidget(maxY);

	buttons->setLayout(buttonsLay);

	centralLay->addWidget(buttons);

	QSplitter *splitter = new QSplitter(central);
	splitter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	textEdit = new QTextEdit(central);
	textEdit->setReadOnly(true);
	textEdit->installEventFilter(this);
	splitter->addWidget(textEdit);

	imageLabel = new QLabel();
	splitter->addWidget(imageLabel);

	centralLay->addWidget(splitter);

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
	scanButton->setEnabled(true);

	minX->setEnabled(true);
	maxX->setEnabled(true);
	minY->setEnabled(true);
	maxY->setEnabled(true);
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
	scanButton->setEnabled(false);
	minX->setEnabled(false);
	maxX->setEnabled(false);
	minY->setEnabled(false);
	maxY->setEnabled(false);

	log(pathEdit->text() + ": disconnected");
	statusBar()->showMessage("disconnected");
}

void MainWin::clearLog()
{
	textEdit->clear();
}

void MainWin::resetStatusBar()
{
	statusBar()->showMessage(QString("x: %1, y: %2, object: %3, ambient: %4")
			.arg(x).arg(y).arg(temp_object).arg(temp_ambient));
}

QRgb MainWin::getColor(int level)
{
	level = 1024 - level;
	if (level < 256)
		return qRgb(255, 255 - level, 0);
	if (level < 512)
		return qRgb(255, 0, level - 256);
	if (level < 768)
		return qRgb(255 - (level - 512), 0, 255);
	return qRgb(0, level - 768, 255);
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
			log("line: " + msg);
			if (!msg.startsWith("I") && !msg.startsWith("E"))
				log("above message has invalid format!");
			else if (msg.startsWith("Idims:"))
			{
				QStringList dims = msg.split(":").takeLast().split(",");
				min_x = dims[0].toInt();
				max_x = dims[1].toInt();
				min_y = dims[2].toInt();
				max_y = dims[3].toInt();

				minX->setRange(min_x, max_x);
				maxX->setRange(min_x, max_x);
				minY->setRange(min_y, max_y);
				maxY->setRange(min_y, max_y);
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
					if (scanInProgress)
					{
						if (temp_object < curMin)
							curMin = temp_object;
						if (temp_object > curMax)
							curMax = temp_object;
						values[image->width() * (y - minY->value()) + (x - minX->value())] = temp_object;

						if (x == maxX->value())
						{
							for (int i = 0; i < qMin(image->height(), y - minY->value() + 1); ++i)
							{
								QRgb *line = (QRgb *)image->scanLine(image->height() - i - 1);
								for (int j = 0; j < image->width(); ++j)
									line[j] = getColor((values[i * image->width() + j] - curMin) * 1024 / (curMax - curMin));
							}
							imageLabel->setPixmap(QPixmap::fromImage(*image).scaledToWidth(imageLabel->width()));

							if (y == maxY->value())
							{
								scanInProgress = false;
							}
							else
							{
								moveY(y + 1);
								moveX(minX->value());
								readObjectTemp();
							}
						}
						else
						{
							moveX(x + 1);
							readObjectTemp();
						}
					}
				}
				else
					log("invalid temp format");
			}
			else if (msg.startsWith("Isetup finished"))
			{
				sendCommand("px90!py90!to!ta!");
			}
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

void MainWin::readObjectTemp()
{
	sendCommand("to!");
}

void MainWin::readAmbientTemp()
{
	sendCommand("ta!");
}

bool MainWin::eventFilter(QObject *obj, QEvent *_event)
{
	if (!scanInProgress && fd != -1 && _event->type() == QEvent::KeyPress)
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
		{
			readObjectTemp();
			readAmbientTemp();
		}
		return true;
	}
	return QObject::eventFilter(obj, _event);
}

void MainWin::scanImage()
{
	QSize sz = QSize(maxX->value() - minX->value() + 1, maxY->value() - minY->value() + 1);
	if (!image || image->size() != sz)
	{
		delete image;
		image = new QImage(sz, QImage::Format_ARGB32);
		image->fill(QColor(0, 0, 0));
		imageLabel->setPixmap(QPixmap::fromImage(*image).scaledToWidth(imageLabel->width()));
		delete values;
		values = new float[sz.width() * sz.height()];
		curMin = 9999;
		curMax = -9999;
	}
	else
	{
		image->fill(QColor(0, 0, 0));
	}

	moveY(minY->value());
	moveX(minX->value());
	readObjectTemp();
	scanInProgress = true;
}

