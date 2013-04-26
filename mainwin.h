#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QPushButton;
class QSocketNotifier;
class QLineEdit;
class QTextEdit;

class MainWin : public QMainWindow
{
	Q_OBJECT
	QPushButton *connectButton, *disconnectButton;
	QLineEdit *pathEdit;
	QTextEdit *textEdit;
	int fd;
	QSocketNotifier *notifier;
	void sendCommand(QString cmd);
	void log(const QString &txt);
	QByteArray buffer;
	int min_x, max_x, min_y, max_y;
	int x, y;
	float temp_object, temp_ambient;
	void moveX(int newPos);
	void moveY(int newPos);
	void resetStatusBar();

	public:
	MainWin(QString path);

	bool eventFilter(QObject *obj, QEvent *event);

	public slots:
	void doConnect();
	void doDisconnect();
	void fdActivated(int);
};

#endif
