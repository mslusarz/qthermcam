#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QLineEdit;
class QTextEdit;
class QSpinBox;
class QSplitter;
class QFileDialog;
class QAction;
class QToolBar;

namespace QThermCam
{
class TempView;
class ThermCam;

class MainWin : public QMainWindow
{
	Q_OBJECT
	ThermCam *thermCam;

	QToolBar *fileToolbar, *deviceToolbar;
	QMenu *fileMenu, *deviceMenu, *helpMenu;

	QAction *connectAction, *disconnectAction, *scanAction, *stopScanAction;
	QAction *loadAction, *saveAction, *saveImageAction;
	QAction *exitAction, *aboutAction, *aboutQtAction, *clearLogAction;

	QLineEdit *pathEdit;
	QTextEdit *textEdit;
	QSpinBox *minX, *maxX, *minY, *maxY;
	int x, y;
	float temp_object, temp_ambient;
	void resetStatusBar();
	TempView *tempView;
	QSplitter *splitter;
	QFileDialog *imageFileDialog, *dataFileDialog;

	bool lockDevice();
	void unlockDevice();

	QTimer *settingsTimer;
	void closeEvent(QCloseEvent *event);
	void prepareDataFileDialog();

	void createActions();
	void createMenus();
	void createToolBar();
	void createStatusBar();

	public:
	MainWin(QString path);

	bool eventFilter(QObject *obj, QEvent *event);
	void saveSettingsLater();

	public slots:
	void doConnect();
	void doDisconnect();
	void scanImage();
	void clearLog();
	void splitterMoved(int pos, int index);
	void saveImage();
	void imageFileSelected(const QString &file);
	void saveSettings();
	void imageClicked(const QPoint &p);
	void loadData();
	void saveData();
	void loadDataFileSelected(const QString &file);
	void saveDataFileSelected(const QString &file);
	void log(const QString &msg);
	void logError(const QString &msg);
	void updateFOV(int xmin, int xmax, int ymin, int ymax);
	void about();
	void scannerReady(int xmin, int xmax, int ymin, int ymax);
	void scannerMoved_X(int x);
	void scannerMoved_Y(int y);
	void objectTemperatureRead(int x, int y, float temp);
	void ambientTemperatureRead(float temp);
	void scanningStopped();
};

}

#endif
