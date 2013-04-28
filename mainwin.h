#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QPushButton;
class QSocketNotifier;
class QLineEdit;
class QTextEdit;
class QSpinBox;
class QImage;
class QLabel;
class QSplitter;

class MainWin : public QMainWindow
{
	Q_OBJECT
	QPushButton *connectionButton, *scanButton;
	QLineEdit *pathEdit;
	QTextEdit *textEdit;
	QSpinBox *minX, *maxX, *minY, *maxY;
	int fd;
	QSocketNotifier *notifier;
	void sendCommand(QString cmd);
	void log(const QString &txt);
	QByteArray buffer;
	int min_x, max_x, min_y, max_y;
	int x, y;
	float temp_object, temp_ambient;
	void readObjectTemp();
	void readAmbientTemp();
	void moveX(int newPos);
	void moveY(int newPos);
	void resetStatusBar();
	bool scanInProgress;
	float *values;
	float curMin, curMax;
	QImage *image;
	QLabel *imageLabel;
	QSplitter *splitter;
	QRgb getColor(int level);
	void refreshPixmap();

	public:
	MainWin(QString path);

	bool eventFilter(QObject *obj, QEvent *event);

	public slots:
	void doConnect();
	void doDisconnect();
	void fdActivated(int);
	void scanImage();
	void stopScanning();
	void clearLog();
	void splitterMoved(int pos, int index);
};

#endif
