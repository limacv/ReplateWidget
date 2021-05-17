#pragma once
#include <opencv2/core.hpp>
#include <qvector.h>
#include <qmap.h>
#include <qstring.h>
#include <qcolor.h>
#include <qhash.h>
#include <opencv2/core.hpp>
#include "MLDataStructure.h"

// A container that contains the trajp used in Step1Widget. All the trajp are in raw coordinates (before stitching)
class MLCacheTrajectories
{
public:
	MLCacheTrajectories() {}
	virtual ~MLCacheTrajectories();
	
	// this will update detect_boxes_list
	bool tryLoadDetectionFromFile();
	// this will update track_boxes_list and frameidx/objid2trajectories
	bool tryLoadTrackFromFile();
	// this will update all global_rects
	bool tryLoadGlobalBoxes() const;

	// this will save global_rects to files
	bool saveGlobalBoxes() const;

	bool isDetectOk() { return !detect_boxes_list.empty(); }
	bool isTrackOk() { return !track_boxes_list.empty(); }

	QColor getColor(const ObjID& id);

public:
	static QVector<QString> classid2name;

	QVector<BBox> detect_boxes_list;

	QVector<BBox*> track_boxes_list;
	QVector<QMap<ObjID, BBox*>> frameidx2boxes;
	QMap<ObjID, Traject> objid2trajectories;

private:
	QHash<ObjID, QColor> colormap;
};

