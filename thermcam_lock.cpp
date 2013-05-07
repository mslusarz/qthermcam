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

#include <QFileInfo>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

using namespace QThermCam;

bool ThermCam::lockDevice(const QString &devicePath, QString &err)
{
	err = QString::null;
	// locking algo should work with arduino ide

	// likely broken WRT races, who cares?
	QFileInfo deviceInfo(devicePath);
	if (!deviceInfo.exists())
	{
		err = tr("%1: device does not exist").arg(devicePath);
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
			err = tr("cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
			return false;
		}

		char buf[101];
		r = read(lfd, buf, 100);
		if (r < 0)
		{
			err = tr("Cannot read from lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
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
				err = tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno));
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
					err = tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno));
					return false;
				}
				// clean state
			}
			else
			{
				err = tr("Device is locked by process %1").arg(pid);
				return false;
			}
		}
	}

	lfd = open(lockfilePathLocal.constData(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IRGRP | S_IROTH);
	if (lfd == -1)
	{
		err = tr("Cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
		return false;
	}
	char buf[100];
	sprintf(buf, "%10d\n", (int)getpid()); // exactly how rxtx does it
	int len = strlen(buf);

	r = write(lfd, buf, len);
	if (r != len)
	{
		err = tr("Cannot write to lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
		::close(lfd);
		if (unlink(lockfilePathLocal.constData()))
			err = tr("Cannot remove lock file! %1").arg(strerror(errno));
		return false;
	}

	::close(lfd);

	return true;
}

void ThermCam::unlockDevice(const QString &devicePath, QString &err)
{
	QFileInfo deviceInfo(devicePath);
	err = QString::null;

	QString lockFilePath = "/var/lock/LCK.." + deviceInfo.fileName();
	QByteArray lockfilePathLocal = lockFilePath.toLocal8Bit();
	QFileInfo lockFileInfo(lockFilePath);
	int lfd, r;

	if (!lockFileInfo.exists())
	{
		err = tr("Someone unlocked the device behind our back!");
		return;
	}

	lfd = open(lockfilePathLocal.constData(), O_RDONLY);
	if (lfd == -1)
	{
		err = tr("Cannot open lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
		return;
	}

	char buf[101];
	r = read(lfd, buf, 100);
	if (r < 0)
	{
		err = tr("Cannot read from lock file %1: %2").arg(lockFilePath).arg(strerror(errno));
		::close(lfd);
		return;
	}

	buf[r] = 0;
	int pid = -1;
	sscanf(buf, "%10d\n", &pid);
	if (pid <= 0)
	{
		if (unlink(lockfilePathLocal.constData()))
			err = tr("Cannot remove stale lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno));
	}
	else
	{
		if (pid == getpid())
		{
			if (unlink(lockfilePathLocal.constData()))
				err = tr("Cannot remove lock file %1: %2").arg(lockfilePathLocal.constData()).arg(strerror(errno));
		}
		else
			err = tr("Someone unlocked the device behind our back AND locked it, new process is %1").arg(pid);
	}
}
