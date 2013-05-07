#ifndef THERMCAM_H_
#define THERMCAM_H_

#include <qobject.h>

class QSocketNotifier;

namespace QThermCam
{

class ThermCam : public QObject
{
	Q_OBJECT
	private:
	int fd;
	QSocketNotifier *notifier;
	QByteArray buffer;
	int xmin, xmax, ymin, ymax;
	int x, y;

	struct
	{
		int xmin, xmax, ymin, ymax;
		bool inProgress;
	} scan;

	bool sendCommand(const QByteArray &cmd);

	public:
	ThermCam(QObject *parent);

	bool doConnect(const QString &path);
	void doDisconnect();
	bool connected() { return fd != -1; }
	bool scanInProgress() { return scan.inProgress; }

	bool sendCommand_readObjectTemp();
	bool sendCommand_readAmbientTemp();
	bool sendCommand_moveX(int newPos);
	bool sendCommand_moveY(int newPos);

	public slots:
	void fdActivated(int fd);

	void scanImage(int xmin, int xmax, int ymin, int ymax);
	void stopScanning();

	signals:
	void scannerReady(int xmin, int xmax, int ymin, int ymax);

	void objectTemperatureRead(int x, int y, float temp);
	void ambientTemperatureRead(float temp);
	void scannerMoved_X(int x);
	void scannerMoved_Y(int y);

	void scanningStopped();

	void debug(const QString &msg);
	void info(const QString &msg);
	void warning(const QString &msg);
	void error(const QString &msg);
};

}
#endif /* THERMCAM_H_ */
