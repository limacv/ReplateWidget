#pragma once
#include <opencv2/core.hpp>
#include <qvector.h>
#include <qmap.h>
#include <qstring.h>

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

class MLStep1Data
{
public:
	MLStep1Data() {}
	~MLStep1Data() {}
	
	// this will update detect_boxes_list
	bool tryLoadDetectionFromFile();

	// this will update track_boxes_list and frameidx/objectid2boxes
	bool tryLoadTrackFromFile();

private:
	QVector<BBox> detect_boxes_list;

	QVector<BBox> track_boxes_list;
	QVector<QMap<ObjID, BBox*>> frameidx2boxes;
	QMap<ObjID, QVector<BBox*>> objectid2boxes;
};

