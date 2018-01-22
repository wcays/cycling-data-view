#include "plotwindow.h"
#include "datalog.h"
#include "user.h"
#include "googlemapwindow.h"
#include "datastatisticswindow.h"
#include "dataprocessing.h"
#include "hrzoneitem.h"
#include "qwtcustomplotpicker.h"
#include "qwtcustomplotzoomer.h"
#include "colours.h"

#include <qwt_plot_picker.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_canvas.h>
#include <qwt_picker_machine.h>
#include <qwt_painter.h>
#include <qwt_scale_widget.h>
#include <qwt_scale_engine.h>
#include <qwt_text.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>

#include <QComboBox.h>
#include <QBoxLayout.h>
#include <QCheckBox.h>
#include <QLabel.h>
#include <QComboBox.h>
#include <QSpinBox.h>
#include <QMessageBox.h>

#include <cassert>
#include <iostream>
#include <sstream>

/******************************************************/
class XAxisScaleDraw: public QwtScaleDraw
{
public:
	XAxisScaleDraw(QString& type):
	  _type(type)
	  {}
 
	virtual QwtText label(double v) const
    {
		if (_type.compare("time")==0)
		{
			return DataProcessing::minsFromSecs(v);
		}
		else if (_type.compare("dist")==0)
		{
			return DataProcessing::kmFromMeters(v);
		}
		else
			return QString("error");
    }

private:
	const QString _type;
};

/******************************************************/
QwtCustomPlotZoomer::QwtCustomPlotZoomer(int x_axis, int y_axis, QWidget* canvas, bool do_replot):
	QwtPlotZoomer(x_axis,y_axis,canvas,do_replot)
{}

/******************************************************/
bool QwtCustomPlotZoomer::accept(QPolygon& p) const
{
	if ( p.count() < 2 )
		return false;

	// Reject zooms which are too small
	if (abs(p[0].x() - p[1].x()) < 10)
		return false;

	// Set the zoom rect to be top to bottm, irrespective of what the user selects in y axis
	p[0].setY(0);
	p[1].setY(canvas()->size().height());
	return true;
}

/******************************************************/
void QwtCustomPlotZoomer::drawRubberBand(QPainter* painter) const
{
	if ( rubberBand() < UserRubberBand )
		QwtPlotPicker::drawRubberBand( painter );
	else
	{
		if ( !isActive() || rubberBandPen().style() == Qt::NoPen )
			return;

		QPolygon p = selection();

		if ( p.count() < 2 )
			return;

		const QPoint& pt1 = p[0];
		const QPoint& pt2 = p[1];

		const int end = canvas()->size().height();
		painter->drawLine(pt1.x(), 0, pt1.x(), end);
		painter->drawLine(pt2.x(), 0, pt2.x(), end);
		painter->fillRect(QRect(pt1.x(), 0, pt2.x() - pt1.x(), end), QBrush(QColor("black"), Qt::Dense7Pattern));
	}
}

/******************************************************/
QwtCustomPlotPicker::QwtCustomPlotPicker(
	int x_axis, int y_axis, 
	boost::shared_ptr<DataLog> data_log, 
	QWidget* canvas, 
	boost::shared_ptr<QCheckBox> hr_cb,
	boost::shared_ptr<QCheckBox> speed_cb,
	boost::shared_ptr<QCheckBox> alt_cb,
	boost::shared_ptr<QCheckBox> cadence_cb,
	boost::shared_ptr<QCheckBox> power_cb,
	boost::shared_ptr<QCheckBox> temp_cb):
	QwtPlotPicker(x_axis,y_axis,QwtPlotPicker::UserRubberBand, QwtPicker::AlwaysOn, canvas),
	_data_log(data_log),
	_x_axis_units(DistAxis),
	_hr_cb(hr_cb),
	_speed_cb(speed_cb),
	_alt_cb(alt_cb),
	_cadence_cb(cadence_cb),
	_power_cb(power_cb),
	_temp_cb(temp_cb)
{}

/******************************************************/
void QwtCustomPlotPicker::setDataLog(
	boost::shared_ptr<DataLog> data_log)
{
	_data_log = data_log;
}

/******************************************************/
void QwtCustomPlotPicker::drawTracker(QPainter* painter) const
{
	return;
}

/******************************************************/
void QwtCustomPlotPicker::drawRubberBand(QPainter* painter) const
{
	if ( rubberBand() < UserRubberBand )
		QwtPlotPicker::drawRubberBand( painter );
	else
	{
		if ( !isActive() || rubberBandPen().style() == Qt::NoPen )
			return;

		QPolygon p = selection();

		if ( p.count() != 1 )
			return;

		const QPoint& pt1 = p[0];
		const double x_val = plot()->invTransform(QwtPlot::xBottom,pt1.x()); // determine x value (time or dist)
		
		// Draw vertical line where curser is
		const int bot = 400;
		const int top = 0;
		painter->drawLine(pt1.x(), bot, pt1.x(), top); 

		// Set pen and fond for all coming text
		painter->setPen(QColor(Qt::black));
		painter->setFont(QFont("Helvetica", 8, QFont::Bold));
		
		// Compute the index based on the x value
		int idx;
		if (_x_axis_units == TimeAxis)
		{
			painter->drawText(QPoint(pt1.x()-65, 10), "time: " + DataProcessing::minsFromSecs(x_val));
			idx = _data_log->indexFromTime(x_val);
		}
		else if (_x_axis_units == DistAxis)
		{
			painter->drawText(QPoint(pt1.x()-60, 10), "dist: " + DataProcessing::kmFromMeters(x_val,2));
			idx = _data_log->indexFromDist(x_val);
		}
		
		// Using the index, determine the curve values
		const double hr = (int)_data_log->heartRateFltd(idx);
		const QPoint pt1_hr(pt1.x(),plot()->transform(QwtPlot::yLeft,hr));
		const double speed = _data_log->speedFltd(idx);
		const QPoint pt1_speed(pt1.x(),plot()->transform(QwtPlot::yLeft,speed));
		const double alt = (int)_data_log->altFltd(idx);
		const QPoint pt1_alt(pt1.x(),plot()->transform(QwtPlot::yRight,alt));
		const double cadence = (int)_data_log->cadenceFltd(idx);
		const QPoint pt1_cadence(pt1.x(),plot()->transform(QwtPlot::yLeft,cadence));
		const double power = _data_log->powerFltd(idx);
		const QPoint pt1_power(pt1.x(),plot()->transform(QwtPlot::yLeft,power));
		const double temp = _data_log->temp(idx);
		const QPoint pt1_temp(pt1.x(),plot()->transform(QwtPlot::yLeft,temp));

		// Draw highlights on all curves
		const QPoint offset(8,-5);
		const QwtPlotItemList item_list = plot()->itemList(QwtPlotCurve::Rtti_PlotCurve);
		for (int i = 0; i < item_list.size(); ++i) 
		{
			if (item_list.at(i)->title().text() == "Heart Rate" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_hr.x(), pt1_hr.y(), pt1_hr.x()+6, pt1_hr.y());
				painter->drawText(pt1_hr + offset, QString::number(hr,'g',3));
			}
			if (item_list.at(i)->title().text() == "Speed" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_speed.x(), pt1_speed.y(), pt1_speed.x()+6, pt1_speed.y());
				painter->drawText(pt1_speed + offset, QString::number(speed,'g',3));
			}
			if (item_list.at(i)->title().text() == "Elevation" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_alt.x(), pt1_alt.y(), pt1_alt.x()+6, pt1_alt.y());
				painter->drawText(pt1_alt + offset, QString::number(alt,'g',4));
			}
			if (item_list.at(i)->title().text() == "Cadence" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_cadence.x(), pt1_cadence.y(), pt1_cadence.x()+6, pt1_cadence.y());
				painter->drawText(pt1_cadence + offset, QString::number(cadence,'g',3));
			}
			if (item_list.at(i)->title().text() == "Power" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_power.x(), pt1_power.y(), pt1_power.x()+6, pt1_power.y());
				painter->drawText(pt1_power + offset, QString::number(power,'g',3));
			}
			if (item_list.at(i)->title().text() == "Temp" && item_list.at(i)->isVisible())
			{
				painter->drawLine(pt1_temp.x(), pt1_temp.y(), pt1_temp.x()+6, pt1_temp.y());
				painter->drawText(pt1_temp + offset, QString::number(temp,'g',3));
			}

			// Draw current values on graph labels
			if (_hr_cb->isChecked())
				_hr_cb->setText("Heart Rate " + QString::number(hr,'g',3).leftJustified(3,' ') + " bpm");
			else
				_hr_cb->setText("Heart Rate");
			if (_speed_cb->isChecked())
				_speed_cb->setText("Speed " + QString::number(speed,'g',3) + " km/h");
			else
				_speed_cb->setText("Speed");
			if (_alt_cb->isChecked())
				_alt_cb->setText("Elevation " + QString::number(alt,'g',4).leftJustified(4,' ') + " m");
			else
				_alt_cb->setText("Elevation");
			if (_cadence_cb->isChecked())
				_cadence_cb->setText("Cadence " + QString::number(cadence,'g',3) + " rpm");
			else
				_cadence_cb->setText("Cadence");
			if (_power_cb->isChecked())
				_power_cb->setText("Power " + QString::number(power,'g',3) + " W");
			else
				_power_cb->setText("Power");
			if (_temp_cb->isChecked())
				_temp_cb->setText("Temp " + QString::number(temp,'g',3) + " C");
			else
				_temp_cb->setText("Temp");
		}
	}
}

/******************************************************/
void QwtCustomPlotPicker::xAxisUnitsChanged(int units)
{
	_x_axis_units = (AxisUnits)units;
}

/******************************************************/
PlotWindow::PlotWindow(
	boost::shared_ptr<GoogleMapWindow> google_map, 
	boost::shared_ptr<DataStatisticsWindow> stats_view)
{
	// Create the plot
	_plot = new QwtPlot();

	// Create HR zone markers
	const QColor hr_zone_colours[5] = 
		{HR_ZONE1_COLOUR, HR_ZONE2_COLOUR, HR_ZONE3_COLOUR, HR_ZONE4_COLOUR, HR_ZONE5_COLOUR};
	_hr_zone_markers.resize(5);
	for (unsigned int i=0; i < _hr_zone_markers.size(); ++i)
	{
		_hr_zone_markers[i] = new HRZoneItem;

		_hr_zone_markers[i]->attach(_plot);
		_hr_zone_markers[i]->hide();
		
		// Set the fill colour
		QColor c(hr_zone_colours[i]);
		c.setAlpha(40);
		_hr_zone_markers[i]->setColour(c);
	}

	// Create lap markers
	_lap_markers.resize(0);
	
	// Connect this window to the google map
	connect(this, SIGNAL(setMarkerPosition(int)), google_map.get(), SLOT(setMarkerPosition(int)));
	connect(this, SIGNAL(beginSelection(int)), google_map.get(), SLOT(beginSelection(int)));
	connect(this, SIGNAL(endSelection(int)), google_map.get(), SLOT(endSelection(int)));
	connect(this, SIGNAL(zoomSelection(int,int)), google_map.get(), SLOT(zoomSelection(int,int)));
	connect(this, SIGNAL(deleteSelection()), google_map.get(), SLOT(deleteSelection()));
	connect(this, SIGNAL(panSelection(int)), google_map.get(), SLOT(moveSelection(int)));
	connect(this, SIGNAL(panAndHoldSelection(int)), google_map.get(), SLOT(moveAndHoldSelection(int)));
	connect(this, SIGNAL(updateDataView()), google_map.get(), SLOT(definePathColour()));

	// Connect this window to the statistical viewer
	connect(this, SIGNAL(zoomSelection(int,int)), stats_view.get(), SLOT(displaySelectedRideStats(int,int)));
	connect(this, SIGNAL(panAndHoldSelection(int)), stats_view.get(), SLOT(moveSelection(int)));
	connect(this, SIGNAL(deleteSelection()), stats_view.get(), SLOT(deleteSelection()));
	connect(this, SIGNAL(updateDataView()), stats_view.get(), SLOT(displayCompleteRideStats()));
	
	// Setup the axis
	_plot->enableAxis(QwtPlot::yRight,true);
	_plot->setAxisAutoScale(QwtPlot::xBottom,true);

	QwtText axis_text;
	QFont font = _plot->axisFont(QwtPlot::xBottom);
	font.setPointSize(8);
	axis_text.setFont(font);

	axis_text.setText("HR (bpm) Speed (km/h) Cadence (rpm)\nPower (W) Temp (C)");
	_plot->setAxisTitle(QwtPlot::yLeft,axis_text);

	axis_text.setText("Elevation (m)");
	_plot->setAxisTitle(QwtPlot::yRight,axis_text);

	axis_text.setText("Distance (km)");
	_plot->setAxisTitle(QwtPlot::xBottom,axis_text);

	// Define the curves to plot
	QColor c;

	_curve_hr = new QwtPlotCurve("Heart Rate");
	c = HR_COLOUR;
	_curve_hr->setPen(c);
	_curve_hr->setYAxis(QwtPlot::yLeft);

	_curve_cadence = new QwtPlotCurve("Cadence");
	c = CADENCE_COLOUR;
	_curve_cadence->setPen(c);
	_curve_cadence->setYAxis(QwtPlot::yLeft);

	_curve_speed = new QwtPlotCurve("Speed");
	c = SPEED_COLOUR;
	_curve_speed->setPen(c);
	_curve_speed->setYAxis(QwtPlot::yLeft);

	_curve_power = new QwtPlotCurve("Power");
	c = POWER_COLOUR;
	_curve_power->setPen(c);
	_curve_power->setYAxis(QwtPlot::yLeft);

	_curve_temp = new QwtPlotCurve("Temp");
	c = TEMP_COLOUR;
	_curve_temp->setPen(c);
	_curve_temp->setYAxis(QwtPlot::yLeft);

	_curve_alt = new QwtPlotCurve("Elevation");
	_curve_alt->setRenderHint(QwtPlotItem::RenderAntialiased);
	c = ALT_COLOUR;
	_curve_alt->setPen(c);
    _curve_alt->setBrush(c);
	_curve_alt->setYAxis(QwtPlot::yRight);
	_curve_alt->setBaseline(-300.0); // ensure display is correct even when -ve altitude

	_curve_alt->attach(_plot);
	_curve_cadence->attach(_plot);
	_curve_speed->attach(_plot);
	_curve_hr->attach(_plot);
	_curve_power->attach(_plot);
	_curve_temp->attach(_plot);

	// Checkboxes for graph plots
	_hr_cb.reset(new QCheckBox("Heart Rate"));
	_speed_cb.reset(new QCheckBox("Speed"));
	_alt_cb.reset(new QCheckBox("Elevation"));
	_cadence_cb.reset(new QCheckBox("Cadence"));
	_power_cb.reset(new QCheckBox("Power"));
	_temp_cb.reset(new QCheckBox("Temp"));
	_laps_cb = new QCheckBox("Laps");
	_hr_zones_cb = new QCheckBox("HR Zones");
	_hr_cb->setChecked(true);
	_speed_cb->setChecked(true);
	_alt_cb->setChecked(true);
	_cadence_cb->setChecked(true);
	_power_cb->setChecked(false);
	_temp_cb->setChecked(false);
	_laps_cb->setChecked(true);
	_hr_zones_cb->setChecked(false);

	QPalette plt;
	plt.setColor(QPalette::WindowText, HR_COLOUR);
	_hr_cb->setPalette(plt);
	plt.setColor(QPalette::WindowText, SPEED_COLOUR);
	_speed_cb->setPalette(plt);
	plt.setColor(QPalette::WindowText, ALT_COLOUR);
	_alt_cb->setPalette(plt);
	plt.setColor(QPalette::WindowText, CADENCE_COLOUR);
	_cadence_cb->setPalette(plt);
	plt.setColor(QPalette::WindowText, POWER_COLOUR);
	_power_cb->setPalette(plt);
	plt.setColor(QPalette::WindowText, TEMP_COLOUR);
	_temp_cb->setPalette(plt);

	connect(_hr_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_speed_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_alt_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_cadence_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_power_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_temp_cb.get(), SIGNAL(stateChanged(int)),this,SLOT(curveSelectionChanged()));
	connect(_laps_cb, SIGNAL(stateChanged(int)),this,SLOT(lapSelectionChanged()));
	connect(_hr_zones_cb, SIGNAL(stateChanged(int)),this,SLOT(hrZoneSelectionChanged()));

	// Plot picker for numerical display
	_plot_picker1 = 
		new QwtCustomPlotPicker(
		QwtPlot::xBottom, QwtPlot::yLeft, 
		_data_log, 
		_plot->canvas(), 
		_hr_cb, _speed_cb, _alt_cb, _cadence_cb, _power_cb, _temp_cb);
		
	_plot_picker1->setRubberBandPen(QColor(Qt::white));
    _plot_picker1->setTrackerPen(QColor(Qt::black));
	_plot_picker1->setStateMachine(new QwtPickerTrackerMachine());
	connect(_plot_picker1, SIGNAL(moved(const QPointF&)), this, SLOT(setMarkerPosition(const QPointF&)));
	
	// Plot picker for drawing user selection
	_plot_picker2 = 
		new QwtPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft, QwtPlotPicker::NoRubberBand, QwtPicker::AlwaysOff, _plot->canvas());
	_plot_picker2->setStateMachine(new QwtPickerDragPointMachine());
	connect(_plot_picker2, SIGNAL(appended(const QPointF&)), this, SLOT(beginSelection(const QPointF&)));
	connect(_plot_picker2, SIGNAL(moved(const QPointF&)), this, SLOT(endSelection(const QPointF&)));

	// Plot zoomer
	_plot_zoomer = new QwtCustomPlotZoomer(QwtPlot::xBottom, QwtPlot::yLeft, _plot->canvas());
	_plot_zoomer->setRubberBand(QwtPicker::UserRubberBand);
    _plot_zoomer->setRubberBandPen(QColor(Qt::white));
    _plot_zoomer->setTrackerMode(QwtPicker::AlwaysOff);
    _plot_zoomer->setMousePattern(QwtEventPattern::MouseSelect2,Qt::RightButton, Qt::ControlModifier);
    _plot_zoomer->setMousePattern(QwtEventPattern::MouseSelect3,Qt::RightButton);
	connect(_plot_zoomer, SIGNAL(zoomed(const QRectF&)), this, SLOT(zoomSelection(const QRectF&)));

	// Plot panner
	_plot_panner = new QwtPlotPanner(_plot->canvas());
	_plot_panner->setMouseButton(Qt::MidButton);
	connect(_plot_panner, SIGNAL(moved(int, int)), this, SLOT(panSelection(int, int)));
	connect(_plot_panner, SIGNAL(panned(int, int)), this, SLOT(panAndHoldSelection(int, int)));

	// Selection for x-axis measurement
	_x_axis_measurement = new QComboBox;
	_x_axis_measurement->insertItem(0, "x-axis = time");
	_x_axis_measurement->insertItem(1, "x-axis = distance");
	_x_axis_measurement->setCurrentIndex(1);
	_plot->setAxisScaleDraw(QwtPlot::xBottom, new XAxisScaleDraw(tr("dist")));
	connect(_x_axis_measurement,SIGNAL(currentIndexChanged(int)), this, SLOT(xAxisUnitsChanged(int)));
	connect(_x_axis_measurement,SIGNAL(currentIndexChanged(int)), _plot_picker1, SLOT(xAxisUnitsChanged(int)));

	// Selection for signal smoothing
	_smoothing_selection = new QSpinBox;
	_smoothing_selection->setRange(1,50);
	_smoothing_selection->setPrefix("Smoothing: ");
	_smoothing_selection->setValue(5); // default value
	connect(_smoothing_selection, SIGNAL(valueChanged(int)),this,SLOT(signalSmoothingChanged()));

	// Layout the GUI
	QWidget* plot_options_widget = new QWidget;
	QVBoxLayout* vlayout1 = new QVBoxLayout(plot_options_widget);
	vlayout1->addWidget(_hr_cb.get());
	vlayout1->addWidget(_speed_cb.get());
	vlayout1->addWidget(_alt_cb.get());
	vlayout1->addWidget(_cadence_cb.get());
	vlayout1->addWidget(_power_cb.get());
	vlayout1->addWidget(_temp_cb.get());
	vlayout1->addWidget(_x_axis_measurement);
	vlayout1->addWidget(_smoothing_selection);
	vlayout1->addWidget(_laps_cb);
	vlayout1->addWidget(_hr_zones_cb);
	vlayout1->addStretch();

	QHBoxLayout* hlayout2 = new QHBoxLayout(this);
	hlayout2->addWidget(_plot);
	hlayout2->addWidget(plot_options_widget);
	
	resize(700,270);

	// Disable all controls until ride data is loaded
	setEnabled(false);
}

/******************************************************/
PlotWindow::~PlotWindow()
{}

/******************************************************/
void PlotWindow::setEnabled(bool enabled)
{
	_plot_picker1->setEnabled(enabled);
	_plot_picker2->setEnabled(enabled);
	_plot_zoomer->setEnabled(enabled);
	_plot_panner->setEnabled(enabled);
	_x_axis_measurement->setEnabled(enabled);
	_smoothing_selection->setEnabled(enabled);
	_hr_cb->setEnabled(enabled);
	_speed_cb->setEnabled(enabled);
	_alt_cb->setEnabled(enabled);
	_cadence_cb->setEnabled(enabled);
	_power_cb->setEnabled(enabled);
	_temp_cb->setEnabled(enabled);
	_laps_cb->setEnabled(enabled);
	_hr_zones_cb->setEnabled(enabled);
}

/******************************************************/
void PlotWindow::displayRide(
	boost::shared_ptr<DataLog> data_log, 
	boost::shared_ptr<User> user)
{
	_user = user;

	if (data_log.get() != _data_log.get() || data_log->isModified())
	{
		// Set the data
		_data_log = data_log;
		_plot_picker1->setDataLog(_data_log);

		// Show the data
		drawGraphs();

		// Display lap markers
		clearLapMarkers();
		if (_data_log->numLaps() > 1)
		{
			_laps_cb->setChecked(true);
			drawLapMarkers();
		}
		else
		{
			_laps_cb->setChecked(false);
		}

		// Draw HR zones
		if (_hr_zones_cb->isChecked())
			drawHRZoneMarkers();

		// Decide which graphs to display based on data content
		_hr_cb->setChecked(_data_log->heartRateValid());
		_speed_cb->setChecked(_data_log->speedValid());
		_alt_cb->setChecked(_data_log->altValid());
		_cadence_cb->setChecked(_data_log->cadenceValid());
		_power_cb->setChecked(_data_log->powerValid());
		_temp_cb->setChecked(_data_log->tempValid());

		// Enabled user interface
		setEnabled(true);
		curveSelectionChanged();
		show();
	}
	else
	{
		// Return to base zoom
		_plot_zoomer->zoom(_plot_zoomer->zoomBase());
	}
}

/******************************************************/
void PlotWindow::drawGraphs()
{
	filterCurveData();
	setCurveData();
	if (_x_axis_measurement->currentIndex() == 0) // time
	{
		_plot->setAxisScale(QwtPlot::xBottom, 0, _data_log->totalTime());
	}
	else // distance
	{
		_plot->setAxisScale(QwtPlot::xBottom, 0, _data_log->totalDist());
	}
	_plot->setAxisScale(QwtPlot::yLeft,-15,230,0);
	_plot_zoomer->setZoomBase(true);
}

/******************************************************/
void PlotWindow::setCurveData()
{
	if (_x_axis_measurement->currentIndex() == 0) // time
	{
		_curve_hr->setRawSamples(&_data_log->time(0), &_data_log->heartRateFltd(0), _data_log->numPoints());
		_curve_speed->setRawSamples(&_data_log->time(0), &_data_log->speedFltd(0), _data_log->numPoints());
		_curve_cadence->setRawSamples(&_data_log->time(0), &_data_log->cadenceFltd(0), _data_log->numPoints());
		_curve_alt->setRawSamples(&_data_log->time(0), &_data_log->altFltd(0), _data_log->numPoints());
		_curve_power->setRawSamples(&_data_log->time(0), &_data_log->powerFltd(0), _data_log->numPoints());
		_curve_temp->setRawSamples(&_data_log->time(0), &_data_log->temp(0), _data_log->numPoints());
	}
	else // distance
	{
		_curve_hr->setRawSamples(&_data_log->dist(0), &_data_log->heartRateFltd(0), _data_log->numPoints());
		_curve_speed->setRawSamples(&_data_log->dist(0), &_data_log->speedFltd(0), _data_log->numPoints());
		_curve_cadence->setRawSamples(&_data_log->dist(0), &_data_log->cadenceFltd(0), _data_log->numPoints());
		_curve_alt->setRawSamples(&_data_log->dist(0), &_data_log->altFltd(0), _data_log->numPoints());
		_curve_power->setRawSamples(&_data_log->dist(0), &_data_log->powerFltd(0), _data_log->numPoints());
		_curve_temp->setRawSamples(&_data_log->dist(0), &_data_log->temp(0), _data_log->numPoints());
	}
}

/******************************************************/
void PlotWindow::drawLapMarkers()
{
	if (_data_log->numLaps() > 1)
	{
		for (int i=0; i < _data_log->numLaps(); ++i)
		{
			QwtPlotMarker* marker = new QwtPlotMarker;
			marker->setLineStyle(QwtPlotMarker::VLine);
			marker->setLinePen(QPen(Qt::DotLine));
			if (_x_axis_measurement->currentIndex() == 0) // time
			{
				const double time = _data_log->time(_data_log->lap(i).second);
				marker->setXValue(time);
			}
			else // distance
			{
				const double dist = _data_log->dist(_data_log->lap(i).second);
				marker->setXValue(dist);
			}
			marker->attach(_plot);
			marker->show();
			_lap_markers.push_back(marker);
		}
	}
}

/******************************************************/
void PlotWindow::clearLapMarkers()
{
	for (unsigned int i=0; i < _lap_markers.size(); ++i)
	{
		_lap_markers[i]->detach();
		delete _lap_markers[i];
	}
	_lap_markers.resize(0);
}

/******************************************************/
void PlotWindow::drawHRZoneMarkers()
{
	assert(_user);
	
	int width;
	if (_x_axis_measurement->currentIndex() == 0) // time
		width = _data_log->totalTime();
	else // dist
		width = _data_log->totalDist();

	_hr_zone_markers[0]->setWidth(width);
	_hr_zone_markers[0]->setMinMax(std::make_pair<int, int>(_user->zone1(), _user->zone2()));

	_hr_zone_markers[1]->setWidth(width);
	_hr_zone_markers[1]->setMinMax(std::make_pair<int, int>(_user->zone2(), _user->zone3()));

	_hr_zone_markers[2]->setWidth(width);
	_hr_zone_markers[2]->setMinMax(std::make_pair<int, int>(_user->zone3(), _user->zone4()));

	_hr_zone_markers[3]->setWidth(width);
	_hr_zone_markers[3]->setMinMax(std::make_pair<int, int>(_user->zone4(), _user->zone5()));

	_hr_zone_markers[4]->setWidth(width);
	_hr_zone_markers[4]->setMinMax(std::make_pair<int, int>(_user->zone5(), 300));


	for (unsigned int i=0; i < _hr_zone_markers.size(); ++i)
		_hr_zone_markers[i]->show();
}

/******************************************************/
void PlotWindow::clearHRZoneMarkers()
{
	for (unsigned int i=0; i < _hr_zone_markers.size(); ++i)
	{
		_hr_zone_markers[i]->hide();
	}
}

/******************************************************/
void PlotWindow::displayLap(int lap_index)
{
	// Get laps details
	const std::pair<int, int> lap = _data_log->lap(lap_index);

	// Define zoom rect
	QRect zoom_rect;
	zoom_rect.setBottom(200);
	zoom_rect.setTop(0);
	if (_x_axis_measurement->currentIndex() == 0) // time
	{
		zoom_rect.setLeft(_data_log->time(lap.first));
		zoom_rect.setRight(_data_log->time(lap.second));
	}
	else // dist
	{
		zoom_rect.setLeft(_data_log->dist(lap.first));
		zoom_rect.setRight(_data_log->dist(lap.second));
	}

	// Tell the plot zoomer to zoom on this rect
	_plot_zoomer->zoom(zoom_rect);
}

/******************************************************/
void PlotWindow::setMarkerPosition(const QPointF& point)
{
	if (_x_axis_measurement->currentIndex() == 0) // time
		emit setMarkerPosition(_data_log->indexFromTime(point.x()));
	else // distance
		emit setMarkerPosition(_data_log->indexFromDist(point.x()));
}		

/******************************************************/
void PlotWindow::beginSelection(const QPointF& point)
{
	_plot_picker1->setEnabled(false);
	if (_x_axis_measurement->currentIndex() == 0) // time
		emit beginSelection(_data_log->indexFromTime(point.x()));
	else // distance
		emit beginSelection(_data_log->indexFromDist(point.x()));
}

/******************************************************/
void PlotWindow::endSelection(const QPointF& point)
{
	if (_x_axis_measurement->currentIndex() == 0) // time
		emit endSelection(_data_log->indexFromTime(point.x()));
	else // distance
		emit endSelection(_data_log->indexFromDist(point.x()));
}

/******************************************************/
void PlotWindow::zoomSelection(const QRectF& rect)
{
	_plot_picker1->setEnabled(true);

	if (_plot_zoomer->zoomRectIndex() == 0) // if fully zoomed out
	{
		emit deleteSelection();
	}
	else // regular zoom
	{
		if (_x_axis_measurement->currentIndex() == 0) // time
			emit zoomSelection(_data_log->indexFromTime(rect.left()), _data_log->indexFromTime(rect.right()));
		else // distance
			emit zoomSelection(_data_log->indexFromDist(rect.left()), _data_log->indexFromDist(rect.right()));
	}
}

/******************************************************/
void PlotWindow::panSelection(int x, int y)
{
	if (_x_axis_measurement->currentIndex() == 0) // time
	{
		emit panSelection(
		_data_log->indexFromTime(_plot->invTransform(QwtPlot::xBottom,x))-
		_data_log->indexFromTime(_plot->invTransform(QwtPlot::xBottom,0)));
	}
	else // distance
	{
		emit panSelection(
		_data_log->indexFromDist(_plot->invTransform(QwtPlot::xBottom,x))-
		_data_log->indexFromDist(_plot->invTransform(QwtPlot::xBottom,0)));
	}
}

/******************************************************/
void PlotWindow::panAndHoldSelection(int x, int y)
{
	if (_x_axis_measurement->currentIndex() == 0) // time
	{
		emit panAndHoldSelection(
		_data_log->indexFromTime(_plot->invTransform(QwtPlot::xBottom,x))-
		_data_log->indexFromTime(_plot->invTransform(QwtPlot::xBottom,0)));
	}
	else // distance
	{
		emit panAndHoldSelection(
		_data_log->indexFromDist(_plot->invTransform(QwtPlot::xBottom,x))-
		_data_log->indexFromDist(_plot->invTransform(QwtPlot::xBottom,0)));
	}
}

/******************************************************/
void PlotWindow::xAxisUnitsChanged(int idx)
{
	if (idx == 0) // time
	{
		_plot->setAxisScaleDraw(QwtPlot::xBottom, new XAxisScaleDraw(tr("time")));
		_plot->setAxisTitle(QwtPlot::xBottom,"Time (min)");
	}
	else // dist
	{
		_plot->setAxisScaleDraw(QwtPlot::xBottom, new XAxisScaleDraw(tr("dist")));
		_plot->setAxisTitle(QwtPlot::xBottom,"Distance (km)");
	}
	
	drawGraphs();
	if (_laps_cb->isChecked()) // we need to resent the lap markers
	{
		clearLapMarkers();
		drawLapMarkers();
	}
	emit deleteSelection();
}

/******************************************************/
void PlotWindow::curveSelectionChanged()
{
	if (_hr_cb->isChecked()) _curve_hr->show(); else _curve_hr->hide();
	if (_alt_cb->isChecked()) _curve_alt->show(); else _curve_alt->hide();
	if (_cadence_cb->isChecked()) _curve_cadence->show(); else _curve_cadence->hide();
	if (_speed_cb->isChecked()) _curve_speed->show(); else _curve_speed->hide();
	if (_power_cb->isChecked()) _curve_power->show(); else _curve_power->hide();
	if (_temp_cb->isChecked()) _curve_temp->show(); else _curve_temp->hide();

	_plot->replot();
}

/******************************************************/
void PlotWindow::lapSelectionChanged()
{
	if (_laps_cb->isChecked())
		drawLapMarkers();
	else
		clearLapMarkers();
	_plot->replot();
}

/******************************************************/
void PlotWindow::hrZoneSelectionChanged()
{
	if (_hr_zones_cb->isChecked())
		drawHRZoneMarkers();
	else
		clearHRZoneMarkers();
	_plot->replot();
}

/******************************************************/
void PlotWindow::signalSmoothingChanged()
{
	// Filter the data
	filterCurveData();

	// Update the plots
	setCurveData();
	_plot->replot();

	// Update other windows viewing this data
	emit updateDataView();
}

/******************************************************/
void PlotWindow::filterCurveData()
{
	DataProcessing::lowPassFilterSignal(_data_log->heartRate(),_data_log->heartRateFltd(),_smoothing_selection->value());
	DataProcessing::lowPassFilterSignal(_data_log->speed(),_data_log->speedFltd(),_smoothing_selection->value());
	DataProcessing::lowPassFilterSignal(_data_log->cadence(),_data_log->cadenceFltd(),_smoothing_selection->value());
	DataProcessing::lowPassFilterSignal(_data_log->alt(),_data_log->altFltd(),_smoothing_selection->value());
	DataProcessing::lowPassFilterSignal(_data_log->gradient(),_data_log->gradientFltd(),_smoothing_selection->value());
	DataProcessing::lowPassFilterSignal(_data_log->power(),_data_log->powerFltd(),_smoothing_selection->value());

	_data_log->heartRateFltdValid() = true;
	_data_log->speedFltdValid() = true;
	_data_log->cadenceFltdValid() = true;
	_data_log->altFltdValid() = true;
	_data_log->gradientFltdValid() = true;
	_data_log->powerFltdValid() = true;
}
