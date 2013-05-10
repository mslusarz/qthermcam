#ifndef TEMPVIEW_H_
#define TEMPVIEW_H_

#include <QLabel>
#include <QHash>
#include <QPoint>
#include <QSize>

namespace QThermCam
{

class TempView : public QLabel
{
	Q_OBJECT
	float *buffer;
	float tmin, tmax;
	int xmin, xmax, ymin, ymax;
	int dataWidth, dataHeight;
	QImage *cacheImage;
	int xhighlight, yhighlight;
	QPoint getPoint(QMouseEvent *event);
	QHash<QPoint, QSize> showPoints;

public:
	TempView(QWidget *parent = 0, Qt::WindowFlags f = 0);

	~TempView();

	void setBuffer(int xmin, int xmax, int ymin, int ymax);

	void setTemperature(int x, int y, float temp);

	void refreshImage(int ymin, int ymax);

	void refreshImage();

	void highlightPoint(int x, int y);

	void saveToFile(const QString &file);

	bool loadFromFile(const QString &file);

	float minTemperature() { return tmin; }

	float maxTemperature() { return tmax; }

	int bufferWidth() { return dataWidth; }

	int bufferHeight() { return dataHeight; }

	int minimumX() { return xmin; }

	int maximumX() { return xmax; }

	int minimumY() { return ymin; }

	int maximumY() { return ymax; }

	void addStaticLabel(const QPoint &p);

	void removeStaticLabel(const QPoint &p);

	void clearStaticLabels();
public slots:
	void refreshView();
signals:
	void leftMouseButtonClicked(const QPoint &p);
	void error(const QString &s);
	void bufferSizeChanged(int xmin, int xmax, int ymin, int ymax);
	void temperatureSet(int x, int y, float t);
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
};

}

#endif /* TEMPVIEW_H_ */
