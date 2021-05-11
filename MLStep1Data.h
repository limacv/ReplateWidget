#pragma once
#include <opencv2/core.hpp>
#include <qvector.h>
#include <qmap.h>
#include <qstring.h>
#include <opencv2/core.hpp>
#include "MLStepDataBase.h"

struct BBox
{
	BBox()
		:frameidx(-1), classid(-1), rect(), confidence(-1), instanceid(-1) {}
	BBox(int frameidx, int classid, int instanceid, cv::Rect rect, float conf)
		:frameidx(frameidx), classid(classid), rect(rect), confidence(conf), instanceid(instanceid) {}
	BBox(int frameidx, int classid, int instanceid, int top, int left, int wid, int hei, float conf)
		:frameidx(frameidx), classid(classid), rect(left, top, wid, hei), confidence(conf), instanceid(instanceid) {}
	int frameidx;
	int classid;
	int instanceid;
	cv::Rect rect;
	float confidence;
};

using ObjID = QPair<int, int>;


// A container that contains the data used in Step1Widget. All the data are in raw coordinates (before stitching)
class MLStep1Data: public MLStepDataBase
{
public:
	MLStep1Data() {}
	virtual ~MLStep1Data();
	
	// this will update detect_boxes_list
	bool tryLoadDetectionFromFile();

	// this will update track_boxes_list and frameidx/objectid2boxes
	bool tryLoadTrackFromFile();

	// this will initialize the mask
	void reinitMasks(const cv::Size& sz, int framecount);

	bool isDetectOk() { return !detect_boxes_list.empty(); }
	bool isTrackOk() { return !track_boxes_list.empty(); }

private:
	static QVector<QString> classid2name;

	friend class Step1Widget;
	friend class Step1RenderArea;
	friend class MLDataManager;

	QVector<BBox> detect_boxes_list;

	QVector<BBox*> track_boxes_list;
	QVector<QMap<ObjID, BBox*>> frameidx2boxes;
	QMap<ObjID, QVector<BBox*>> objectid2boxes;

public:
	QVector<cv::Mat> masks;
};

