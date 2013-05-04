#ifndef TEMPVIEW_H_
#define TEMPVIEW_H_

#include <QLabel>

class TempView : public QLabel
{
	Q_OBJECT
	float *buffer;
	float tmin, tmax;
	int xmin, xmax, ymin, ymax;
	int dataWidth, dataHeight;
	QImage *cacheImage;
	QPoint getPoint(QMouseEvent *event);

public:
	TempView(QWidget *parent = 0, Qt::WindowFlags f = 0);

	void setBuffer(float *temps, int xmin, int xmax, int ymin, int ymax);

	void setTemperature(int x, int y, float temp);

	void refreshImage(int ymin, int ymax);

public slots:
	void refreshView();
signals:
	void leftMouseButtonClicked(const QPoint &p);
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
};


#endif /* TEMPVIEW_H_ */
