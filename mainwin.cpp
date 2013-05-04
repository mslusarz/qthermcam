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
#include <QList>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QSettings>

#include "tempview.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

MainWin::MainWin(QString path) : QMainWindow(), minX(NULL), fd(-1), notifier(NULL), min_x(90), max_x(90), min_y(90), max_y(90),
		x(90), y(90), temp_object(-1000), temp_ambient(-1000), scanInProgress(false), values(NULL), tempView(NULL),
		splitter(NULL), fileDialog(NULL)
{
	QWidget *central = new QWidget(this);
	setCentralWidget(central);
	pathEdit = new QLineEdit(path, central);
	pathEdit->installEventFilter(this);
	QVBoxLayout *centralLay = new QVBoxLayout();
	centralLay->addWidget(pathEdit);

	QWidget *buttons = new QWidget(central);

	connectionButton = new QPushButton("Connect to device", buttons);
	connectionButton->installEventFilter(this);
	connect(connectionButton,    SIGNAL(clicked()), this, SLOT(doConnect()));

	QPushButton *clearButton = new QPushButton("Clear log", buttons);
	connect(clearButton, SIGNAL(clicked()), this, SLOT(clearLog()));

	scanButton = new QPushButton(buttons);
	scanButton->setEnabled(false);
	scanButton->installEventFilter(this);
	stopScanning();

	saveButton = new QPushButton("Save image", buttons);
	saveButton->setEnabled(false);
	saveButton->installEventFilter(this);
	connect(saveButton, SIGNAL(clicked()), this, SLOT(saveImage()));

	QSettings settings;

	minX = new QSpinBox(buttons);
	minX->setPrefix("Min x: ");
	minX->setRange(0, 180);
	minX->setEnabled(false);
	minX->setValue(settings.value("xmin").toInt());

	maxX = new QSpinBox(buttons);
	maxX->setPrefix("Max x: ");
	maxX->setRange(0, 180);
	maxX->setValue(180);
	maxX->setEnabled(false);
	maxX->setValue(settings.value("xmax").toInt());

	minY = new QSpinBox(buttons);
	minY->setPrefix("Min y: ");
	minY->setRange(0, 180);
	minY->setEnabled(false);
	minY->setValue(settings.value("ymin").toInt());

	maxY = new QSpinBox(buttons);
	maxY->setPrefix("Max y: ");
	maxY->setRange(0, 180);
	maxY->setValue(180);
	maxY->setEnabled(false);
	maxY->setValue(settings.value("ymax").toInt());

	QHBoxLayout *buttonsLay = new QHBoxLayout();
	buttonsLay->addWidget(connectionButton);
	buttonsLay->addWidget(clearButton);
	buttonsLay->addWidget(scanButton);
	buttonsLay->addWidget(saveButton);
	buttonsLay->addWidget(minX);
	buttonsLay->addWidget(maxX);
	buttonsLay->addWidget(minY);
	buttonsLay->addWidget(maxY);

	buttons->setLayout(buttonsLay);

	centralLay->addWidget(buttons);

	splitter = new QSplitter(central);
	splitter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	textEdit = new QTextEdit(central);
	textEdit->setReadOnly(true);
	textEdit->installEventFilter(this);
	splitter->addWidget(textEdit);

	tempView = new TempView();
	splitter->addWidget(tempView);
	connect(tempView, SIGNAL(leftMouseButtonClicked(const QPoint &)), this, SLOT(imageClicked(const QPoint &)));

	if (!splitter->restoreState(settings.value("splitterSizes").toByteArray()))
	{
		QList<int> sizes;
		sizes.append(9999);
		sizes.append(0);
		splitter->setSizes(sizes);
	}

	connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(splitterMoved(int, int)));

	centralLay->addWidget(splitter);

	setStatusBar(new QStatusBar());

	central->setLayout(centralLay);

	settingsTimer = new QTimer(this);
	settingsTimer->setSingleShot(true);
	connect(settingsTimer, SIGNAL(timeout()), this, SLOT(saveSettings()));

	if (!restoreGeometry(settings.value("geometry").toByteArray()))
	{
		QRect deskRect = QDesktopWidget().screenGeometry();
		setGeometry(100, 100, deskRect.width() - 200, deskRect.height() - 200);
	}
}

void MainWin::log(const QString &txt)
{
	textEdit->append(txt);
}

struct flags_desc
{
	unsigned int flag;
	QString desc;
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define _F(X) {X, #X}

static struct flags_desc iflags[] =
{
	_F(IGNBRK),	_F(IGNBRK),	_F(BRKINT),	_F(IGNPAR),
	_F(PARMRK),	_F(INPCK),	_F(ISTRIP),	_F(INLCR),
	_F(IGNCR),	_F(ICRNL),	_F(IUCLC),	_F(IXON),
	_F(IXANY),	_F(IXOFF),	_F(IMAXBEL),_F(IUTF8)
};

static struct flags_desc oflags[] =
{
	_F(OPOST),	_F(OLCUC),	_F(ONLCR),	_F(OCRNL),
	_F(ONOCR),	_F(ONLRET),	_F(OFILL),	_F(OFDEL),
	_F(NLDLY),	_F(CRDLY),	_F(TABDLY),	_F(BSDLY),
	_F(VTDLY),	_F(FFDLY)
};

static struct flags_desc cflags[] =
{
	_F(CSTOPB),	_F(CREAD),	_F(PARENB),	_F(PARODD),
	_F(HUPCL),	_F(CLOCAL),	_F(CIBAUD),	_F(CMSPAR),
	_F(CRTSCTS),_F(CBAUDEX)
};

static struct flags_desc lflags[] =
{
	_F(ISIG),	_F(ICANON),	_F(XCASE),	_F(ECHO),
	_F(ECHOE),	_F(ECHOK),	_F(ECHONL),	_F(ECHOCTL),
	_F(ECHOPRT),_F(ECHOKE),	_F(FLUSHO),	_F(NOFLSH),
	_F(TOSTOP),	_F(PENDIN),	_F(IEXTEN)
};

static struct flags_desc speeds[] =
{
	_F(B50),		_F(B75),		_F(B110),		_F(B134),
	_F(B150),		_F(B200),		_F(B300),		_F(B600),
	_F(B1200),		_F(B1800),		_F(B2400),		_F(B4800),
	_F(B9600),		_F(B19200),		_F(B38400),		_F(B57600),
	_F(B115200),	_F(B230400),	_F(B460800),	_F(B500000),
	_F(B576000),	_F(B921600),	_F(B1000000),	_F(B1152000),
	_F(B1500000),	_F(B2000000),	_F(B2500000),	_F(B3000000),
	_F(B3500000),	_F(B4000000)
};

void MainWin::dumpTermiosInfo(const struct termios &argp)
{
	speed_t speed;
	unsigned int i;
	QString out;
	QStringList outlist;

	outlist.append(out.sprintf("iflag:  0x%08x", argp.c_iflag));
	for (i = 0; i < ARRAY_SIZE(iflags); ++i)
		if (argp.c_iflag & iflags[i].flag)
			outlist.append(iflags[i].desc);
	log(outlist.join(" "));

	outlist.clear();
	outlist.append(out.sprintf("oflag:  0x%08x", argp.c_oflag));
	for (i = 0; i < ARRAY_SIZE(oflags); ++i)
		if (argp.c_oflag & oflags[i].flag)
			outlist.append(oflags[i].desc);
	log(outlist.join(" "));

	outlist.clear();
	outlist.append(out.sprintf("cflag:  0x%08x", argp.c_cflag));
	for (i = 0; i < ARRAY_SIZE(cflags); ++i)
		if (argp.c_cflag & cflags[i].flag)
			outlist.append(cflags[i].desc);
	if (argp.c_cflag & CBAUD)
		outlist.append(out.sprintf("CBAUD(%x)", argp.c_cflag & CBAUD));
	if (argp.c_cflag & CSIZE)
	{
		out.sprintf("CSIZE(0x%x = ", argp.c_cflag & CSIZE);
		switch (argp.c_cflag & CSIZE)
		{
			case CS5: out.append("CS5"); break;
			case CS6: out.append("CS6"); break;
			case CS7: out.append("CS7"); break;
			case CS8: out.append("CS8"); break;
			default: out.append("???"); break;
		}
		out.append(")");
		outlist.append(out);
	}
	log(outlist.join(" "));

	outlist.clear();
	outlist.append(out.sprintf("lflag:  0x%08x", argp.c_lflag));
	for (i = 0; i < ARRAY_SIZE(lflags); ++i)
		if (argp.c_lflag & lflags[i].flag)
			outlist.append(lflags[i].desc);
	log(outlist.join(" "));

	out.sprintf("cline:  0x%08x", argp.c_line);
	log(out);

	speed = cfgetispeed(&argp);
	out.sprintf("ispeed: 0x%08x ", speed);
	for (i = 0; i < ARRAY_SIZE(speeds); ++i)
		if (speed == speeds[i].flag)
		{
			out.append(speeds[i].desc);
			break;
		}
	log(out);

	speed = cfgetospeed(&argp);
	out.sprintf("ospeed: 0x%08x ", speed);
	for (i = 0; i < ARRAY_SIZE(speeds); ++i)
		if (speed == speeds[i].flag)
		{
			out.append(speeds[i].desc);
			break;
		}
	log(out);
}

bool MainWin::lockDevice()
{
	// locking algo should work with arduino ide

	// likely broken WRT races, who cares?
	QString devicePath = pathEdit->text();
	QFileInfo deviceInfo(devicePath);
	if (!deviceInfo.exists())
	{
		log(devicePath + ": device does not exist");
		return false;
	}

	QString lockFilePath = "/var/lock/LCK.." + deviceInfo.fileName();
	QByteArray lockfilePathLocal = lockFilePath.toLocal8Bit();
	QFileInfo lockFileInfo(lockFilePath);
	int lfd, r;

	if (lockFileInfo.exists())
	{
		lfd = open(lockfilePathLocal.constData(), O_RDONLY);
		if (lfd == -1)
		{
			log("cannot open lock file " + lockFilePath + ": " + QString(strerror(errno)));
			return false;
		}

		char buf[101];
		r = read(lfd, buf, 100);
		if (r < 0)
		{
			log("cannot read from lock file " + lockFilePath + ": " + QString(strerror(errno)));
			::close(lfd);
			return false;
		}

		buf[r] = 0;
		int pid = -1;
		sscanf(buf, "%10d\n", &pid);
		if (pid <= 0)
		{
			if (unlink(lockfilePathLocal.constData()))
			{
				log("cannot remove stale lock file " + lockfilePathLocal + ": " + QString(strerror(errno)));
				return false;
			}
			// clean state
		}
		else
		{
			r = kill(pid, 0);
			if (r == -1 && errno == ESRCH)
			{
				if (unlink(lockfilePathLocal.constData()))
				{
					log("cannot remove stale lock file " + lockfilePathLocal + ": " + QString(strerror(errno)));
					return false;
				}
				// clean state
			}
			else
			{
				log("device is locked by process " + QString::number(pid));
				return false;
			}
		}
	}

	lfd = open(lockfilePathLocal.constData(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IRGRP | S_IROTH);
	if (lfd == -1)
	{
		log("cannot open lock file " + lockFilePath + ": " + QString(strerror(errno)));
		return false;
	}
	char buf[100];
	sprintf(buf, "%10d\n", (int)getpid()); // exactly how rxtx does it
	int len = strlen(buf);

	r = write(lfd, buf, len);
	if (r != len)
	{
		log("cannot write to lock file " + lockFilePath + ": " + QString(strerror(errno)));
		::close(lfd);
		if (unlink(lockfilePathLocal.constData()))
			log("cannot remove lock file! " + QString(strerror(errno)));
		return false;
	}

	::close(lfd);

	return true;
}

void MainWin::unlockDevice()
{
	QString devicePath = pathEdit->text();
	QFileInfo deviceInfo(devicePath);

	QString lockFilePath = "/var/lock/LCK.." + deviceInfo.fileName();
	QByteArray lockfilePathLocal = lockFilePath.toLocal8Bit();
	QFileInfo lockFileInfo(lockFilePath);
	int lfd, r;

	if (!lockFileInfo.exists())
	{
		log("someone unlocked the device behind our back");
		return;
	}

	lfd = open(lockfilePathLocal.constData(), O_RDONLY);
	if (lfd == -1)
	{
		log("cannot open lock file " + lockFilePath + ": " + QString(strerror(errno)));
		return;
	}

	char buf[101];
	r = read(lfd, buf, 100);
	if (r < 0)
	{
		log("cannot read from lock file " + lockFilePath + ": " + QString(strerror(errno)));
		::close(lfd);
		return;
	}

	buf[r] = 0;
	int pid = -1;
	sscanf(buf, "%10d\n", &pid);
	if (pid <= 0)
	{
		if (unlink(lockfilePathLocal.constData()))
			log("cannot remove stale lock file " + lockfilePathLocal + ": " + QString(strerror(errno)));
	}
	else
	{
		if (pid == getpid())
		{
			if (unlink(lockfilePathLocal.constData()))
				log("cannot remove lock file " + lockfilePathLocal + ": " + QString(strerror(errno)));
		}
		else
			log("someone unlocked the device behind our back AND locked it, new process is " + QString::number(pid));
	}
}

void MainWin::doConnect()
{
	QString path = pathEdit->text();

	log(path + ": connecting");

	if (!lockDevice())
		return;

	QByteArray pathLocal = path.toLocal8Bit();
	fd = open(pathLocal.constData(), O_RDWR | O_NOCTTY); // +O_NONBLOCK?
	if (fd == -1)
	{
		log(path + ": " + QString(strerror(errno)));
		return;
	}

	struct termios argp;

	if (tcgetattr(fd, &argp))
	{
		log(path + " tcgetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		return;
	}

	log("current port settings: ");
	dumpTermiosInfo(argp);

	argp.c_iflag = 0;
	argp.c_oflag = 0;
	argp.c_cflag = 0;
	argp.c_lflag = 0;

	argp.c_iflag |= INPCK;

	argp.c_cflag |= CS8;
	argp.c_cflag |= CREAD;
	argp.c_cflag |= CLOCAL;

	cfsetspeed(&argp, B9600);

	log("new port settings: ");
	dumpTermiosInfo(argp);

	if (tcsetattr(fd, TCSANOW, &argp))
	{
		log(path + " tcsetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		return;
	}

	log(path + ": connected");
	statusBar()->showMessage("connected");

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(fdActivated(int)));

	connectionButton->setText("Disconnect from device");
	disconnect(connectionButton, SIGNAL(clicked()), this, SLOT(doConnect()));
	connect(connectionButton, SIGNAL(clicked()), this, SLOT(doDisconnect()));

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
	unlockDevice();

	connectionButton->setText("Connect to device");
	disconnect(connectionButton, SIGNAL(clicked()), this, SLOT(doDisconnect()));
	connect(connectionButton, SIGNAL(clicked()), this, SLOT(doConnect()));

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
						tempView->setTemperature(x, y, temp_object);

						if (x == maxX->value())
						{
							tempView->refreshImage(minY->value(), y);
							tempView->refreshView();
							qApp->processEvents();

							if (y == maxY->value())
								stopScanning();
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

	delete values;
	values = new float[sz.width() * sz.height()];

	tempView->setBuffer(values, minX->value(), maxX->value(), minY->value(), maxY->value());
	tempView->setMinimumWidth(sz.width());

	if (tempView->width() < 20)
	{
		QList<int> sizes;
		sizes.append(9999);
		sizes.append(1);
		splitter->setSizes(sizes);
	}
	splitter->setCollapsible(1, false);

	minX->setEnabled(false);
	maxX->setEnabled(false);
	minY->setEnabled(false);
	maxY->setEnabled(false);

	saveSettings();

	moveY(minY->value());
	moveX(minX->value());
	readObjectTemp();

	disconnect(scanButton, SIGNAL(clicked()), this, SLOT(scanImage()));
	scanButton->setText("Stop scanning");
	connect(scanButton, SIGNAL(clicked()), this, SLOT(stopScanning()));
	connectionButton->setEnabled(false);

	saveButton->setEnabled(true);

	scanInProgress = true;
}

void MainWin::stopScanning()
{
	scanInProgress = false;

	connectionButton->setEnabled(true);
	connect(scanButton, SIGNAL(clicked()), this, SLOT(scanImage()));
	scanButton->setText("Scan image");
	if (splitter)
		splitter->setCollapsible(1, true);
	if (tempView)
		tempView->setMinimumWidth(0);

	if (minX)
	{
		minX->setEnabled(true);
		maxX->setEnabled(true);
		minY->setEnabled(true);
		maxY->setEnabled(true);
	}
}

void MainWin::splitterMoved(int, int)
{
	tempView->refreshView();
	saveSettingsLater();
}

void MainWin::saveImage()
{
	if (!fileDialog)
	{
		fileDialog = new QFileDialog(this, "Choose file name");
		fileDialog->setNameFilter("All image files (*.png *.jpg *.bmp *.ppm *.tiff *.xbm *.xpm)");
	}
	fileDialog->open(this, SLOT(fileSelected(const QString &)));
}

void MainWin::fileSelected(const QString &file)
{
	if (tempView->pixmap()->save(file))
		log(QString("file %1 saved").arg(file));
	else
		log(QString("saving to file %1 failed").arg(file));
}

void MainWin::saveSettings()
{
	QSettings settings;
	settings.setValue("xmin", minX->value());
	settings.setValue("xmax", maxX->value());
	settings.setValue("ymin", minY->value());
	settings.setValue("ymax", maxY->value());
	const QPixmap *pix = tempView->pixmap();
	if (pix)
		settings.setValue("splitterSizes", splitter->saveState());
	settings.setValue("geometry", saveGeometry());
}

void MainWin::saveSettingsLater()
{
	settingsTimer->start(2000);
}

void MainWin::closeEvent(QCloseEvent *event)
{
	saveSettings();
	if (fd != -1)
		doDisconnect();
	QMainWindow::closeEvent(event);
}

void MainWin::imageClicked(const QPoint &p)
{
	if (fd == -1 || scanInProgress || p.x() < minX->value() || p.x() > maxX->value() || p.y() < minY->value() || p.y() > maxY->value())
		return;
	moveX(p.x());
	moveY(p.y());
}
