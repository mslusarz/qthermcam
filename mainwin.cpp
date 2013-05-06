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
#include <QToolBar>
#include <QAction>
#include <QMenuBar>

#include "tempview.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

using namespace QThermCam;

MainWin::MainWin(QString path) : QMainWindow(), minX(NULL), fd(-1), notifier(NULL), min_x(90), max_x(90), min_y(90), max_y(90),
		x(90), y(90), temp_object(-1000), temp_ambient(-1000), scanInProgress(false), tempView(NULL), splitter(NULL),
		imageFileDialog(NULL), dataFileDialog(NULL)
{
	QSettings settings;

	createActions();
	createMenus();
	createToolBar();
	createStatusBar();

	QFrame *leftPanel = new QFrame(this);
	leftPanel->setFrameShape(QFrame::StyledPanel);
	QGridLayout *leftPanelLayout = new QGridLayout(leftPanel);

	leftPanelLayout->addWidget(new QLabel(tr("Path"),  leftPanel), 0, 0, Qt::AlignRight);
	leftPanelLayout->addWidget(new QLabel(tr("Min X"), leftPanel), 1, 0, Qt::AlignRight);
	leftPanelLayout->addWidget(new QLabel(tr("Max X"), leftPanel), 2, 0, Qt::AlignRight);
	leftPanelLayout->addWidget(new QLabel(tr("Min Y"), leftPanel), 3, 0, Qt::AlignRight);
	leftPanelLayout->addWidget(new QLabel(tr("Max Y"), leftPanel), 4, 0, Qt::AlignRight);

	pathEdit = new QLineEdit(path, leftPanel);
	pathEdit->installEventFilter(this);
	leftPanelLayout->addWidget(pathEdit, 0, 1);

	stopScanning();

	scanAction->setEnabled(false);
	disconnectAction->setEnabled(false);
	saveImageAction->setEnabled(false);

	minX = new QSpinBox(leftPanel);
	minX->setRange(0, 180);
	minX->setEnabled(false);
	leftPanelLayout->addWidget(minX, 1, 1);

	maxX = new QSpinBox(leftPanel);
	maxX->setRange(0, 180);
	maxX->setEnabled(false);
	leftPanelLayout->addWidget(maxX, 2, 1);

	minY = new QSpinBox(leftPanel);
	minY->setRange(0, 180);
	minY->setEnabled(false);
	leftPanelLayout->addWidget(minY, 3, 1);

	maxY = new QSpinBox(leftPanel);
	maxY->setRange(0, 180);
	maxY->setEnabled(false);
	leftPanelLayout->addWidget(maxY, 4, 1);

	QWidget *spacer = new QWidget(leftPanel);
	spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
	leftPanelLayout->addWidget(spacer);

	updateFOV(settings.value("xmin").toInt(), settings.value("xmax").toInt(),
			  settings.value("ymin").toInt(), settings.value("ymax").toInt());
	leftPanel->setLayout(leftPanelLayout);

	splitter = new QSplitter(this);
	setCentralWidget(splitter);

	splitter->addWidget(leftPanel);

	tempView = new TempView();
	splitter->addWidget(tempView);
	connect(tempView, SIGNAL(leftMouseButtonClicked(const QPoint &)), this, SLOT(imageClicked(const QPoint &)));
	connect(tempView, SIGNAL(error(const QString &)), this, SLOT(logError(const QString &)));
	connect(tempView, SIGNAL(updateFOV(int, int, int, int)), this, SLOT(updateFOV(int, int, int, int)));

	textEdit = new QTextEdit(splitter);
	textEdit->setReadOnly(true);
	textEdit->installEventFilter(this);
	splitter->addWidget(textEdit);

	settingsTimer = new QTimer(this);
	settingsTimer->setSingleShot(true);
	connect(settingsTimer, SIGNAL(timeout()), this, SLOT(saveSettings()));

	if (!restoreGeometry(settings.value("geometry").toByteArray()))
	{
		QRect deskRect = QDesktopWidget().screenGeometry();
		setGeometry(100, 100, deskRect.width() - 200, deskRect.height() - 200);
	}

	if (!splitter->restoreState(settings.value("splitterSizes").toByteArray()))
	{
		QRect geo = geometry();
		QList<int> sizes;
		sizes.append(1);
		sizes.append(geo.width() * 75 / 100);
		sizes.append(geo.width() * 25 / 100);
		splitter->setSizes(sizes);
	}

	restoreState(settings.value("windowState").toByteArray());

	connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(splitterMoved(int, int)));
}

void MainWin::createActions()
{
	// device actions
	connectAction = new QAction(QIcon::fromTheme("call-start"), tr("&Connect"), this);
	connectAction->setStatusTip(tr("Connects to device"));
	connect(connectAction, SIGNAL(triggered()), this, SLOT(doConnect()));

	disconnectAction = new QAction(QIcon::fromTheme("call-stop"), tr("&Disconnect"), this);
	disconnectAction->setStatusTip(tr("Disconnects from currently connected device"));
	connect(disconnectAction, SIGNAL(triggered()), this, SLOT(doDisconnect()));

	scanAction = new QAction(QIcon::fromTheme("media-record"), tr("Scan image"), this);
	scanAction->setStatusTip(tr("Starts scanning"));
	connect(scanAction, SIGNAL(triggered()), this, SLOT(scanImage()));

	stopScanAction = new QAction(QIcon::fromTheme("process-stop"), tr("Stop scanning"), this);
	stopScanAction->setStatusTip(tr("Stops scanning"));
	connect(stopScanAction, SIGNAL(triggered()), this, SLOT(stopScanning()));

	// application internal actions
	clearLogAction = new QAction(QIcon::fromTheme("edit-clear"), tr("Clear log"), this);
	connect(clearLogAction, SIGNAL(triggered()), this, SLOT(clearLog()));

	// file actions
	loadAction = new QAction(QIcon::fromTheme("document-open"), tr("Load file"), this);
	loadAction->setStatusTip(tr("Loads previously saved data file"));
	connect(loadAction, SIGNAL(triggered()), this, SLOT(loadData()));

	saveAction = new QAction(QIcon::fromTheme("document-save"), tr("Save file"), this);
	saveAction->setStatusTip(tr("Saves currently scanned data file"));
	connect(saveAction, SIGNAL(triggered()), this, SLOT(saveData()));

	saveImageAction = new QAction(QIcon::fromTheme("document-save"), tr("Save image"), this);
	saveImageAction->setStatusTip(tr("Saves currently scanned data file as image"));
	connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));

	exitAction = new QAction(QIcon::fromTheme("window-close"), tr("E&xit"), this);
	connect(exitAction, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

	aboutAction = new QAction(QIcon::fromTheme("help-about"), tr("About"), this);
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

	aboutQtAction = new QAction(QIcon::fromTheme("help-about"), tr("About Qt"), this);
	connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void MainWin::createMenus()
{
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(loadAction);
	fileMenu->addAction(saveAction);
	fileMenu->addAction(saveImageAction);
	fileMenu->addSeparator();
	fileMenu->addAction(clearLogAction);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAction);

	deviceMenu = menuBar()->addMenu(tr("&Device"));
	deviceMenu->addAction(connectAction);
	deviceMenu->addAction(disconnectAction);
	deviceMenu->addSeparator();
	deviceMenu->addAction(scanAction);
	deviceMenu->addAction(stopScanAction);

	menuBar()->addSeparator();

	helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(aboutAction);
	helpMenu->addAction(aboutQtAction);
}

void MainWin::createToolBar()
{
	deviceToolbar = addToolBar(tr("Device"));
	deviceToolbar->setObjectName("device_toolbar");
	deviceToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	deviceToolbar->addAction(connectAction);
	deviceToolbar->addAction(disconnectAction);
	deviceToolbar->addSeparator();
	deviceToolbar->addAction(scanAction);
	deviceToolbar->addAction(stopScanAction);

	fileToolbar = addToolBar(tr("File"));
	fileToolbar->setObjectName("file_toolbar");
	fileToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	fileToolbar->addAction(loadAction);
	fileToolbar->addAction(saveAction);
	fileToolbar->addAction(saveImageAction);
	fileToolbar->addSeparator();
	fileToolbar->addAction(clearLogAction);
}

void MainWin::createStatusBar()
{
	statusBar()->showMessage(tr("Ready"));
}

void MainWin::about()
{
	// TODO: implement
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
		logError(tr("%1: device does not exist").arg(devicePath));
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
			logError(tr("cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
			return false;
		}

		char buf[101];
		r = read(lfd, buf, 100);
		if (r < 0)
		{
			logError(tr("Cannot read from lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
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
				logError(tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno)));
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
					logError(tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno)));
					return false;
				}
				// clean state
			}
			else
			{
				logError(tr("Device is locked by process %1").arg(pid));
				return false;
			}
		}
	}

	lfd = open(lockfilePathLocal.constData(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IRGRP | S_IROTH);
	if (lfd == -1)
	{
		logError(tr("Cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
		return false;
	}
	char buf[100];
	sprintf(buf, "%10d\n", (int)getpid()); // exactly how rxtx does it
	int len = strlen(buf);

	r = write(lfd, buf, len);
	if (r != len)
	{
		logError(tr("Cannot write to lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
		::close(lfd);
		if (unlink(lockfilePathLocal.constData()))
			logError(tr("Cannot remove lock file! %1").arg(strerror(errno)));
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
		logError(tr("Someone unlocked the device behind our back!"));
		return;
	}

	lfd = open(lockfilePathLocal.constData(), O_RDONLY);
	if (lfd == -1)
	{
		logError(tr("Cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
		return;
	}

	char buf[101];
	r = read(lfd, buf, 100);
	if (r < 0)
	{
		logError(tr("Cannot read from lock file %1: %2").arg(lockFilePath).arg(strerror(errno)));
		::close(lfd);
		return;
	}

	buf[r] = 0;
	int pid = -1;
	sscanf(buf, "%10d\n", &pid);
	if (pid <= 0)
	{
		if (unlink(lockfilePathLocal.constData()))
			logError(tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno)));
	}
	else
	{
		if (pid == getpid())
		{
			if (unlink(lockfilePathLocal.constData()))
				logError(tr("Cannot remove lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno)));
		}
		else
			logError(tr("Someone unlocked the device behind our back AND locked it, new process is %1").arg(pid));
	}
}

void MainWin::doConnect()
{
	QString path = pathEdit->text();

	log(tr("%1: connecting").arg(path));

	if (!lockDevice())
		return;

	QByteArray pathLocal = path.toLocal8Bit();
	fd = open(pathLocal.constData(), O_RDWR | O_NOCTTY); // +O_NONBLOCK?
	if (fd == -1)
	{
		logError(path + ": " + QString(strerror(errno)));
		return;
	}

	struct termios argp;

	if (tcgetattr(fd, &argp))
	{
		logError(path + " tcgetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		return;
	}

	log(tr("Current port settings:"));
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
		logError(path + " tcsetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		return;
	}

	log(tr("%1: connected").arg(path));
	statusBar()->showMessage(tr("connected"));

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(fdActivated(int)));

	connectAction->setEnabled(false);
	disconnectAction->setEnabled(true);

	pathEdit->setEnabled(false);
	scanAction->setEnabled(true);

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

	disconnectAction->setEnabled(false);
	connectAction->setEnabled(true);

	pathEdit->setEnabled(true);
	scanAction->setEnabled(false);
	minX->setEnabled(false);
	maxX->setEnabled(false);
	minY->setEnabled(false);
	maxY->setEnabled(false);

	log(tr("%1: disconnected").arg(pathEdit->text()));
	statusBar()->showMessage(tr("disconnected"));
}

void MainWin::clearLog()
{
	textEdit->clear();
}

void MainWin::resetStatusBar()
{
	statusBar()->showMessage(tr("x: %1, y: %2, object: %3, ambient: %4")
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
			if (msg.startsWith("E"))
				logError(tr("Line: %1").arg(msg));
			else
				log(tr("Line: %1").arg(msg));
			if (!msg.startsWith("I") && !msg.startsWith("E"))
				logError(tr("Above message has invalid format!"));
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
				{
					x = tmpx;
					tempView->highlightPoint(x, y);
					if (!scanInProgress)
						tempView->refreshView();
					resetStatusBar();
				}
			}
			else if (msg.startsWith("Iy: "))
			{
				bool ok;
				int tmpy = msg.mid(3).toInt(&ok);
				if (ok)
				{
					y = tmpy;
					tempView->highlightPoint(x, y);
					if (!scanInProgress)
						tempView->refreshView();
					resetStatusBar();
				}
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
					logError(tr("Invalid temp format"));
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
		logError(tr("Cannot send command"));
	else if (r < 0)
		logError(tr("write: %1").arg(strerror(errno)));
	else
		log(tr("Command '%1' sent: %2").arg(cmd).arg(r));
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

	tempView->setBuffer(minX->value(), maxX->value(), minY->value(), maxY->value());
	tempView->setMinimumWidth(sz.width());

	minX->setEnabled(false);
	maxX->setEnabled(false);
	minY->setEnabled(false);
	maxY->setEnabled(false);

	saveSettings();

	moveY(minY->value());
	moveX(minX->value());
	readObjectTemp();

	scanAction->setEnabled(false);
	stopScanAction->setEnabled(true);
	disconnectAction->setEnabled(false);

	saveImageAction->setEnabled(true);

	scanInProgress = true;
}

void MainWin::stopScanning()
{
	scanInProgress = false;

	disconnectAction->setEnabled(true);
	stopScanAction->setEnabled(false);
	scanAction->setEnabled(true);

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
	if (!imageFileDialog)
	{
		imageFileDialog = new QFileDialog(this, tr("Choose file name"));
		imageFileDialog->setNameFilter(tr("All image files (*.png *.jpg *.bmp *.ppm *.tiff *.xbm *.xpm)"));
	}
	tempView->highlightPoint(-1, -1);
	tempView->refreshView();
	imageFileDialog->open(this, SLOT(imageFileSelected(const QString &)));
}

void MainWin::imageFileSelected(const QString &file_)
{
	QString file = file_;
	QString s = QFileInfo(file).suffix();
	if (s != "png" && s != "jpg" && s != "bmp" && s != "ppm" && s != "tiff" && s != "xbm" && s != "xpm")
		file += ".png";

	if (tempView->pixmap()->save(file))
		log(tr("File %1 saved").arg(file));
	else
		logError(tr("Saving to file %1 failed").arg(file));
}

void MainWin::saveSettings()
{
	QSettings settings;
	settings.setValue("xmin", minX->value());
	settings.setValue("xmax", maxX->value());
	settings.setValue("ymin", minY->value());
	settings.setValue("ymax", maxY->value());
	settings.setValue("splitterSizes", splitter->saveState());
	settings.setValue("geometry", saveGeometry());
	settings.setValue("windowState", saveState());
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

void MainWin::prepareDataFileDialog()
{
	if (!dataFileDialog)
	{
		dataFileDialog = new QFileDialog(this, tr("Choose file name"));
		dataFileDialog->setNameFilter(tr("QThermCam data files (*.qtcd)"));
	}
}

void MainWin::loadData()
{
	prepareDataFileDialog();
	dataFileDialog->open(this, SLOT(loadDataFileSelected(const QString &)));
}

void MainWin::saveData()
{
	prepareDataFileDialog();
	dataFileDialog->open(this, SLOT(saveDataFileSelected(const QString &)));
}

void MainWin::loadDataFileSelected(const QString &file)
{
	if (tempView->loadFromFile(file))
		saveImageAction->setEnabled(true);
}

void MainWin::saveDataFileSelected(const QString &file_)
{
	QString file = file_;
	if (!file.endsWith(".qtcd"))
		file += ".qtcd";
	tempView->saveToFile(file);
}

void MainWin::logError(const QString &msg)
{
	log("<b><font color=\"red\">" + msg + "</font></b>");
}

void MainWin::updateFOV(int xmin, int xmax, int ymin, int ymax)
{
	minX->setValue(xmin);
	maxX->setValue(xmax);
	minY->setValue(ymin);
	maxY->setValue(ymax);
}
