#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QPushButton;
class QSocketNotifier;
class QLineEdit;

class MainWin : public QMainWindow
{
	Q_OBJECT
	QPushButton *connectButton, *disconnectButton;
	QLineEdit *pathEdit;
	int fd;
	QSocketNotifier *notifier;
	void sendCommand(char cmd);

	public:
	MainWin(QString path);

	bool eventFilter(QObject *obj, QEvent *event);

	public slots:
	void doConnect();
	void doDisconnect();
	void fdActivated(int);
};

#endif
