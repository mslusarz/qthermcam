#ifndef mainwin_h
#define mainwin_h

#include <QMainWindow>

class QAction;
class QFileDialog;
class QLineEdit;
class QSpinBox;
class QSplitter;
class QTextEdit;
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
	QSpinBox *minX, *maxX, *minY, *maxY;

	QSplitter *splitter;
	TempView *tempView;

	QTextEdit *textEdit;

	int x, y;

	float temp_object, temp_ambient;

	QFileDialog *imageFileDialog, *dataFileDialog;

	QTimer *settingsTimer;

	void prepareDataFileDialog();

	void createActions();
	void createMenus();
	void createToolBar();
	void createStatusBar();

	void resetStatusBar();

	void closeEvent(QCloseEvent *event);
	bool eventFilter(QObject *obj, QEvent *event);

	public:
	MainWin(QString path);

	public slots:
	/* toolbar actions - device */
	void doConnect();
	void doDisconnect();
	void scanImage();
	void scanningStopped();

	/* toolbar actions - app */
	void loadData();
	void saveData();
	void saveImage();
	void clearLog();

	void loadDataFileSelected(const QString &file);
	void saveDataFileSelected(const QString &file);
	void imageFileSelected(const QString &file);

	/* TempView */
	void updateFOV(int xmin, int xmax, int ymin, int ymax);
	void splitterMoved(int pos, int index);
	void imageClicked(const QPoint &p);

	/* ThermCam */
	void scannerReady(int xmin, int xmax, int ymin, int ymax);
	void scannerMoved_X(int x);
	void scannerMoved_Y(int y);
	void objectTemperatureRead(int x, int y, float temp);
	void ambientTemperatureRead(float temp);

	/* misc */
	void about();

	void saveSettings();
	void saveSettingsLater();

	void log(const QString &msg);
	void logError(const QString &msg);
};

}

#endif
