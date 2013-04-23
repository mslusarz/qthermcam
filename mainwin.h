#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QPushButton;
class QSocketNotifier;

class MainWin : public QMainWindow
{
	Q_OBJECT
	QPushButton *connectButton, *disconnectButton;
	int fd;
	QSocketNotifier *notifier;
	const char *addr;
	void handleEvent(QKeyEvent *);
	void sendCommand(char cmd);

	public:
	MainWin(const char *addr);

	virtual void keyPressEvent(QKeyEvent * event);
	virtual void keyReleaseEvent(QKeyEvent * event);

	public slots:
	void doConnect();
	void doDisconnect();
	void fdActivated(int);
};

#endif
