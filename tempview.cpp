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
#include "tempview.h"

#include <QImage>
#include <QMouseEvent>
#include <QToolTip>
#include <QPainter>
#include <QDomDocument>

TempView::TempView(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f), buffer(NULL), tmin(999),
	tmax(-999), xmin(0), xmax(0), ymin(0), ymax(0), dataWidth(0), dataHeight(0), cacheImage(NULL),
	xhighlight(-1), yhighlight(-1)
{
	setMouseTracking(true);
	setAlignment(Qt::AlignCenter);
}

void TempView::setBuffer(int _xmin, int _xmax, int _ymin, int _ymax)
{
	xmin = _xmin;
	xmax = _xmax;
	ymin = _ymin;
	ymax = _ymax;

	dataWidth = xmax - xmin + 1;
	dataHeight = ymax - ymin + 1;

	delete buffer;
	buffer = new float[dataWidth * dataHeight];

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

	refreshView();
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

	QImage tempImage;

	if (width() > dataWidth)
	{
		int newWidth = qMax(dataWidth, width() - 10);
		int newHeight = qMax(dataHeight, height() - 10);

		// round down to the nearest multiple of data[Width|Height]
		newWidth = int(newWidth / dataWidth) * dataWidth;
		newHeight = int(newHeight / dataHeight) * dataHeight;

		tempImage = cacheImage->scaled(newWidth, newHeight, Qt::KeepAspectRatio/*, Qt::SmoothTransformation*/);
	}
	else
		tempImage = cacheImage->copy();

	// highlight "point"
	int image_xhighlight = xhighlight - xmin;
	int image_yhighlight = yhighlight - ymin;
	if (image_xhighlight >= 0 && image_xhighlight < dataWidth &&
		image_yhighlight >= 0 && image_yhighlight < dataHeight)
	{
		int xscale = tempImage.width() / dataWidth;
		int yscale = tempImage.height() / dataHeight;
		int wrect = xscale - 1;
		int hrect = yscale - 1;
		if (wrect == 0)
			wrect++;
		if (hrect == 0)
			hrect++;
		QPainter p(&tempImage);
		p.setPen(QColor(255,255,255));
		p.drawRect(image_xhighlight * xscale, tempImage.height() - (image_yhighlight + 1) * yscale, wrect, hrect);
	}

	setPixmap(QPixmap::fromImage(tempImage));
}

QPoint TempView::getPoint(QMouseEvent *event)
{
	const QPixmap *p = pixmap();
	if (!p)
		return QPoint(-1, -1);
	int ph = p->height();
	int pw = p->width();

	int xpos = event->x() - (width() - pw) / 2 - 2;
	int ypos = event->y() - (height() - ph) / 2 - 2;

	if (xpos < 0 || xpos >= pw || ypos < 0 || ypos >= ph)
		return QPoint(-1, -1);

	xpos = xpos * dataWidth / pw;
	ypos = ypos * dataHeight / ph;

	if (xpos >= dataWidth || ypos >= dataHeight)
		return QPoint(-1, -1);
	ypos = dataHeight - ypos - 1;

	return QPoint(xpos, ypos);
}

void TempView::mouseMoveEvent(QMouseEvent *event)
{
	QPoint p = getPoint(event);
	if (p.x() < 0)
	{
		QToolTip::showText(event->globalPos(), "", this);
		QLabel::mouseMoveEvent(event);
		return;
	}

	QString s = QString::number(buffer[p.y() * dataWidth + p.x()]);
	//s.sprintf("%d %d %f", p.x(), p.y(), buffer[p.y() * dataWidth + p.x()]);
	QToolTip::showText(event->globalPos(), s, this);
	QLabel::mouseMoveEvent(event);
}

void TempView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
		QPoint p = getPoint(event);
		if (p.x() >= 0)
		{
			p += QPoint(xmin, ymin);
			emit leftMouseButtonClicked(p);
		}
	}

	QLabel::mousePressEvent(event);
}

void TempView::resizeEvent(QResizeEvent *event)
{
	refreshView();
	QLabel::resizeEvent(event);
}

void TempView::highlightPoint(int x, int y)
{
	xhighlight = x;
	yhighlight = y;
}

void TempView::saveToFile(const QString &file)
{
	QDomDocument doc("qtdc");
	QDomElement root = doc.createElement("qtdc");
	doc.appendChild(root);

	QDomElement fov = doc.createElement("fov");
	fov.setAttribute("xmin", xmin);
	fov.setAttribute("xmax", xmax);
	fov.setAttribute("ymin", ymin);
	fov.setAttribute("ymax", ymax);
	root.appendChild(fov);

	QDomElement highlight = doc.createElement("highlight");
	highlight.setAttribute("x", xhighlight);
	highlight.setAttribute("y", yhighlight);
	root.appendChild(highlight);

	// make it easy to parse, also by external tools
	QDomElement data = doc.createElement("data");
	for(int y = 0; y < dataHeight; ++y)
	{
		QDomElement row = doc.createElement("row");
		row.setAttribute("y", y + ymin);

		for (int x = 0; x < dataWidth; ++x)
		{
			QDomElement col = doc.createElement("col");
			col.setAttribute("x", x + xmin);
			float f = buffer[y * dataWidth + x];
			if (f == -1000)
				col.setAttribute("val", "");
			else
				col.setAttribute("val", buffer[y * dataWidth + x]);
			row.appendChild(col);
		}
		data.appendChild(row);
	}
	root.appendChild(data);

	QFile f(file);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		emit error(QString("Cannot open file %1 for writing: %2").arg(file).arg(f.errorString()));
		return;
	}

	qint64 r = f.write(doc.toString().toUtf8());
	if (r == -1)
	{
		emit error(QString("Cannot write to file %1: %2").arg(file).arg(f.errorString()));
		f.close();
		return;
	}

	f.close();
}

void TempView::loadFromFile(const QString &file)
{
	QDomDocument doc("qtcd");
	QFile f(file);

	if (!f.open(QIODevice::ReadOnly))
	{
		emit error(QString("Cannot open file %1 for reading: %2").arg(file).arg(f.errorString()));
		return;
	}

	QString err;
	int line, col;
	if (!doc.setContent(&f, &err, &line, &col))
	{
		f.close();
		emit error(QString("Cannot parse file %1, error: %2, line: %3, col: %4").arg(file).arg(f.errorString()).arg(line).arg(col));
		return;
	}
	f.close();

	QDomElement docElem = doc.documentElement();

	QDomElement fov = docElem.firstChildElement("fov");
	int xmin = fov.attribute("xmin", "0").toInt();
	int xmax = fov.attribute("xmax", "180").toInt();
	int ymin = fov.attribute("ymin", "0").toInt();
	int ymax = fov.attribute("ymax", "180").toInt();

	emit updateFOV(xmin, xmax, ymin, ymax);
	setBuffer(xmin, xmax, ymin, ymax);

	QDomElement highlight = docElem.firstChildElement("highlight");
	int xhl = highlight.attribute("x", "-1").toInt();
	int yhl = highlight.attribute("y", "-1").toInt();

	highlightPoint(xhl, yhl);

	QDomElement data = docElem.firstChildElement("data");
	QDomElement row = data.firstChildElement("row");
	while (!row.isNull())
	{
		int y = row.attribute("y", "-1").toInt();
		if (y < ymin || y > ymax)
		{
			emit error(QString("Invalid row number: %1").arg(y));
			return;
		}
		QDomElement col = row.firstChildElement("col");
		while (!col.isNull())
		{
			QString tstr = col.attribute("val", "");
			QString xstr = col.attribute("x", "-1");

			if (!tstr.isEmpty() && !xstr.isEmpty())
			{
				bool okt, okx;
				float t = tstr.toFloat(&okt);
				int x = xstr.toInt(&okx);
				if (okt && okx)
				{
					if (x < xmin || x > xmax)
					{
						emit error(QString("Invalid column number: %1 in row %2").arg(x).arg(y));
						return;
					}
					setTemperature(x, y, t);
				}
				else
				{
					emit error(QString("Unable to parse x or temperature in row: %1").arg(y));
					return;
				}
			}

			col = col.nextSiblingElement("col");
		}

		row = row.nextSiblingElement("row");
	}

	refreshImage(ymin, ymax);
	refreshView();
}
