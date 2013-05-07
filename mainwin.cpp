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

#include <QAction>
#include <QApplication>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenuBar>
#include <QPushButton>
#include <QRect>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>

#include "tempview.h"
#include "thermcam.h"

using namespace QThermCam;

MainWin::MainWin(QString path) : QMainWindow(), thermCam(NULL), minX(NULL), splitter(NULL), tempView(NULL), x(-1), y(-1),
		temp_object(-1000), temp_ambient(-1000), imageFileDialog(NULL), dataFileDialog(NULL)
{
	thermCam = new ThermCam(this);
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

	scanningStopped();

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
	connect(thermCam, SIGNAL(scannerReady(int, int, int, int)), this, SLOT(scannerReady(int, int, int, int)));
	connect(thermCam, SIGNAL(scannerMoved_X(int)), this, SLOT(scannerMoved_X(int)));
	connect(thermCam, SIGNAL(scannerMoved_Y(int)), this, SLOT(scannerMoved_Y(int)));
	connect(thermCam, SIGNAL(objectTemperatureRead(int, int, float)), this, SLOT(objectTemperatureRead(int, int, float)));
	connect(thermCam, SIGNAL(ambientTemperatureRead(float)), this, SLOT(ambientTemperatureRead(float)));
	connect(thermCam, SIGNAL(scanningStopped()), this, SLOT(scanningStopped()));

	connect(thermCam, SIGNAL(debug(const QString &)), this, SLOT(log(const QString &)));
	connect(thermCam, SIGNAL(info(const QString &)), this, SLOT(log(const QString &)));
	connect(thermCam, SIGNAL(warning(const QString &)), this, SLOT(logError(const QString &)));
	connect(thermCam, SIGNAL(error(const QString &)), this, SLOT(logError(const QString &)));
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
	connect(stopScanAction, SIGNAL(triggered()), thermCam, SLOT(stopScanning()));

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

void MainWin::doConnect()
{
	QString path = pathEdit->text();

	log(tr("%1: connecting").arg(path));

	if (!thermCam->doConnect(path))
		return;

	log(tr("%1: connected").arg(path));
	statusBar()->showMessage(tr("connected"));

	connectAction->setEnabled(false);
	disconnectAction->setEnabled(true);

	pathEdit->setEnabled(false);

	minX->setEnabled(true);
	maxX->setEnabled(true);
	minY->setEnabled(true);
	maxY->setEnabled(true);
}

void MainWin::doDisconnect()
{
	thermCam->doDisconnect();

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

bool MainWin::eventFilter(QObject *obj, QEvent *_event)
{
	if (!thermCam->scanInProgress() && thermCam->connected() && _event->type() == QEvent::KeyPress)
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
			thermCam->sendCommand_moveY(y + speed);
		else if (event->key() == Qt::Key_Down)
			thermCam->sendCommand_moveY(y - speed);
		else if (event->key() == Qt::Key_Left)
			thermCam->sendCommand_moveX(x - speed);
		else if (event->key() == Qt::Key_Right)
			thermCam->sendCommand_moveX(x + speed);
		else if (event->key() == Qt::Key_Space)
		{
			thermCam->sendCommand_readObjectTemp();
			thermCam->sendCommand_readAmbientTemp();
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

	scanAction->setEnabled(false);
	stopScanAction->setEnabled(true);
	disconnectAction->setEnabled(false);

	saveImageAction->setEnabled(true);

	thermCam->scanImage(minX->value(), maxX->value(), minY->value(), maxY->value());
}

void MainWin::scanningStopped()
{
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
	if (thermCam->connected())
		doDisconnect();
	QMainWindow::closeEvent(event);
}

void MainWin::imageClicked(const QPoint &p)
{
	if (!thermCam->connected() || thermCam->scanInProgress() ||
			p.x() < minX->value() || p.x() > maxX->value() || p.y() < minY->value() || p.y() > maxY->value())
		return;
	thermCam->sendCommand_moveX(p.x());
	thermCam->sendCommand_moveY(p.y());
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

void MainWin::scannerReady(int xmin, int xmax, int ymin, int ymax)
{
	minX->setRange(xmin, xmax);
	maxX->setRange(xmin, xmax);
	minY->setRange(ymin, ymax);
	maxY->setRange(ymin, ymax);

	scanAction->setEnabled(true);
}

void MainWin::scannerMoved_X(int x)
{
	this->x = x;
	tempView->highlightPoint(x, y);
	if (!thermCam->scanInProgress())
		tempView->refreshView();
	resetStatusBar();
}

void MainWin::scannerMoved_Y(int y)
{
	this->y = y;
	tempView->highlightPoint(x, y);
	if (!thermCam->scanInProgress())
		tempView->refreshView();
	resetStatusBar();
}

void MainWin::objectTemperatureRead(int x, int y, float temp)
{
	temp_object = temp;

	if (thermCam->scanInProgress())
	{
		tempView->setTemperature(x, y, temp);

		if (x == maxX->value())
		{
			tempView->refreshImage(minY->value(), y);
			tempView->refreshView();
			qApp->processEvents();
		}
	}

	resetStatusBar();
}

void MainWin::ambientTemperatureRead(float temp)
{
	temp_ambient = temp;
	resetStatusBar();
}
