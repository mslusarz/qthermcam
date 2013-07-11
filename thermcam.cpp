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

#include "thermcam.h"

#include <QSocketNotifier>
#include <QStringList>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>

using namespace QThermCam;

static QString describeTermiosInfo(const struct termios &argp);

ThermCam::ThermCam(QObject *parent) : QObject(parent), fd(-1), notifier(NULL), xmin(-1), xmax(-1), ymin(-1), ymax(-1), x(-1), y(-1)
{
	scan.inProgress = false;
}

bool ThermCam::doConnect(const QString &path)
{
	QString err;
	if (!lockDevice(path, err))
	{
		emit error(err);
		return false;
	}

	QByteArray pathLocal = path.toLocal8Bit();
	fd = open(pathLocal.constData(), O_RDWR | O_NOCTTY); // +O_NONBLOCK?
	if (fd == -1)
	{
		emit error(path + ": " + QString(strerror(errno)));
		unlockDevice(devicePath, err);
		if (err != QString::null)
			emit error(err);
		return false;
	}

	struct termios argp;

	if (tcgetattr(fd, &argp))
	{
		emit error(path + " tcgetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		unlockDevice(devicePath, err);
		if (err != QString::null)
			emit error(err);
		return false;
	}

	emit info(tr("Current port settings:"));
	emit info(describeTermiosInfo(argp));

	argp.c_iflag = 0;
	argp.c_oflag = 0;
	argp.c_cflag = 0;
	argp.c_lflag = 0;

	argp.c_iflag |= INPCK;

	argp.c_cflag |= CS8;
	argp.c_cflag |= CREAD;
	argp.c_cflag |= CLOCAL;

	cfsetspeed(&argp, B115200);

	emit info("new port settings: ");
	emit info(describeTermiosInfo(argp));

	if (tcsetattr(fd, TCSANOW, &argp))
	{
		emit error(path + " tcsetattr: " + QString(strerror(errno)));
		::close(fd);
		fd = -1;
		unlockDevice(devicePath, err);
		if (err != QString::null)
			emit error(err);
		return false;
	}

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(fdActivated(int)));

	devicePath = path;

	return true;
}

void ThermCam::doDisconnect()
{
	sendCommand("moff!");

	disconnect(notifier, NULL, this, NULL);
	delete notifier;
	notifier = NULL;
	::close(fd);
	fd = -1;

	QString err;
	unlockDevice(devicePath, err);
	if (err != QString::null)
		emit error(err);

	devicePath = QString::null;
}

bool ThermCam::sendCommand(const QByteArray &cmd)
{
	int r = write(fd, cmd.constData(), cmd.length());
	if (r <= 0)
		emit error(tr("Cannot send command"));
	else if (r < 0)
		emit error(tr("write: %1").arg(strerror(errno)));
	else
		emit debug(tr("Command '%1' sent: %2").arg(cmd.constData()).arg(r));
	return r > 0;
}

bool ThermCam::sendCommand_readObjectTemp()
{
	return sendCommand("to!");
}

bool ThermCam::sendCommand_readAmbientTemp()
{
	return sendCommand("ta!");
}

bool ThermCam::sendCommand_moveX(int newPos)
{
	if (newPos < xmin)
		newPos = xmin;
	if (newPos > xmax)
		newPos = xmax;
	return sendCommand(QString("px%1!").arg(newPos).toAscii());
}

bool ThermCam::sendCommand_moveY(int newPos)
{
	if (newPos < ymin)
		newPos = ymin;
	if (newPos > ymax)
		newPos = ymax;
	return sendCommand(QString("py%1!").arg(newPos).toAscii());
}

void ThermCam::fdActivated(int fd)
{
	char c;
	int r = read(fd, &c, 1);
	if (r != 1)
		return;

	if (c == '\r')
		return;

	if (c != '\n')
	{
		buffer.append(c);
		return;
	}

	QString msg = QString(buffer);
	if (msg.startsWith("E"))
		emit error(tr("Line: %1").arg(msg));
	else
		emit info(tr("Line: %1").arg(msg));

	if (!msg.startsWith("I") && !msg.startsWith("E"))
		emit error(tr("Above message has invalid format!"));
	else if (msg.startsWith("Idims:"))
	{
		QStringList dims = msg.split(":").takeLast().split(",");
		xmin = dims[0].toInt();
		xmax = dims[1].toInt();
		ymin = dims[2].toInt();
		ymax = dims[3].toInt();

		emit scannerReady(xmin, xmax, ymin, ymax);
	}
	else if (msg.startsWith("Ix: "))
	{
		bool ok;
		int tmpx = msg.mid(3).toInt(&ok);
		if (ok)
		{
			x = tmpx;
			emit scannerMoved_X(x);
		}
	}
	else if (msg.startsWith("Iy: "))
	{
		bool ok;
		int tmpy = msg.mid(3).toInt(&ok);
		if (ok)
		{
			y = tmpy;
			emit scannerMoved_Y(y);
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
				emit objectTemperatureRead(x, y, temp);
			else if (tt[0] == QString("ambient"))
				emit ambientTemperatureRead(temp);

			if (scan.inProgress)
			{
				if (x == scan.xmax)
				{
					if (y == scan.ymax)
						stopScanning();
					else
					{
						sendCommand_moveY(y + 1);
						sendCommand_moveX(scan.xmin);
						sendCommand_readObjectTemp();
					}
				}
				else
				{
					sendCommand_moveX(x + 1);
					sendCommand_readObjectTemp();
				}
			}
		}
		else
			emit error(tr("Invalid temp format"));
	}
	else if (msg.startsWith("Isetup finished"))
	{
		sendCommand("mon!px90!py90!to!ta!");
	}
	buffer.truncate(0);
}

void ThermCam::scanImage(int xmin, int xmax, int ymin, int ymax)
{
	scan.xmin = xmin;
	scan.xmax = xmax;
	scan.ymin = ymin;
	scan.ymax = ymax;

	scan.inProgress = true;
	sendCommand("jd!"); // joystick disable
	sendCommand_moveY(scan.ymin);
	sendCommand_moveX(scan.xmin);
	sendCommand_readObjectTemp();
}

void ThermCam::stopScanning()
{
	scan.inProgress = false;
	sendCommand("je!"); // joystick enable
	emit scanningStopped();
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

static QString describeTermiosInfo(const struct termios &argp)
{
	QStringList ret;
	speed_t speed;
	unsigned int i;
	QString out;
	QStringList outlist;

	outlist.append(out.sprintf("iflag:  0x%08x", argp.c_iflag));
	for (i = 0; i < ARRAY_SIZE(iflags); ++i)
		if (argp.c_iflag & iflags[i].flag)
			outlist.append(iflags[i].desc);
	ret.append(outlist.join(" "));

	outlist.clear();
	outlist.append(out.sprintf("oflag:  0x%08x", argp.c_oflag));
	for (i = 0; i < ARRAY_SIZE(oflags); ++i)
		if (argp.c_oflag & oflags[i].flag)
			outlist.append(oflags[i].desc);
	ret.append(outlist.join(" "));

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
	ret.append(outlist.join(" "));

	outlist.clear();
	outlist.append(out.sprintf("lflag:  0x%08x", argp.c_lflag));
	for (i = 0; i < ARRAY_SIZE(lflags); ++i)
		if (argp.c_lflag & lflags[i].flag)
			outlist.append(lflags[i].desc);
	ret.append(outlist.join(" "));

	out.sprintf("cline:  0x%08x", argp.c_line);
	ret.append(out);

	speed = cfgetispeed(&argp);
	out.sprintf("ispeed: 0x%08x ", speed);
	for (i = 0; i < ARRAY_SIZE(speeds); ++i)
		if (speed == speeds[i].flag)
		{
			out.append(speeds[i].desc);
			break;
		}
	ret.append(out);

	speed = cfgetospeed(&argp);
	out.sprintf("ospeed: 0x%08x ", speed);
	for (i = 0; i < ARRAY_SIZE(speeds); ++i)
		if (speed == speeds[i].flag)
		{
			out.append(speeds[i].desc);
			break;
		}
	ret.append(out);

	return ret.join("\n");
}
