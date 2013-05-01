#include "tempview.h"

#include <QImage>

TempView::TempView(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f), buffer(NULL), tmin(999),
	tmax(-999), xmin(0), xmax(0), ymin(0), ymax(0), dataWidth(0), dataHeight(0), cacheImage(NULL)
{

}

void TempView::setBuffer(float *temps, int _xmin, int _xmax, int _ymin, int _ymax)
{
	buffer = temps;

	xmin = _xmin;
	xmax = _xmax;
	ymin = _ymin;
	ymax = _ymax;

	dataWidth = xmax - xmin + 1;
	dataHeight = ymax - ymin + 1;

	for(int i = 0; i < dataWidth * dataHeight; ++i)
		buffer[i] = -1000;
	tmin = 999;
	tmax = -999;

	if (cacheImage)
	{
		delete cacheImage;
		cacheImage = NULL;
	}

	if (!cacheImage)
		cacheImage = new QImage(dataWidth, dataHeight, QImage::Format_ARGB32);

	cacheImage->fill(QColor(0, 0, 0));

	setPixmap(QPixmap::fromImage(*cacheImage));
}

void TempView::setTemperature(int x, int y, float temp)
{
	tmin = qMin(tmin, temp);
	tmax = qMax(tmax, temp);
	buffer[dataWidth * (y - ymin) + (x - xmin)] = temp;
}

static QRgb getColor(int level)
{
	level = 1024 - level;
	if (level < 256)
		return qRgb(255, 255 - level, 0);
	if (level < 512)
		return qRgb(255, 0, level - 256);
	if (level < 768)
		return qRgb(255 - (level - 512), 0, 255);
	return qRgb(0, level - 768, 255);
}

void TempView::refreshImage(int _ymin, int _ymax)
{
	for (int y = _ymin - ymin; y < _ymax - ymin + 1; ++y)
	{
		QRgb *line = (QRgb *)cacheImage->scanLine(dataHeight - y - 1);
		for (int x = 0; x < dataWidth; ++x)
			line[x] = getColor((buffer[y * dataWidth + x] - tmin) * 1024 / (tmax - tmin));
	}
}

void TempView::refreshView()
{
	if (!cacheImage)
		return;

	if (width() > dataWidth)
	{
		int newWidth = qMax(dataWidth, width() - 10);
		int newHeight = qMax(dataHeight, height() - 10);
		QPixmap pix = QPixmap::fromImage(*cacheImage).scaled(newWidth, newHeight, Qt::KeepAspectRatio/*, Qt::SmoothTransformation*/);
		setPixmap(pix);
	}
	else
		setPixmap(QPixmap::fromImage(*cacheImage));
}
