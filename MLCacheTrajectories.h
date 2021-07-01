#pragma once
#include <opencv2/core.hpp>
#include <qvector.h>
#include <qmap.h>
#include <qstring.h>
#include <qcolor.h>
#include <qhash.h>
#include <opencv2/core.hpp>
#include <fstream>
#include "MLDataStructure.h"

// A container that contains the trajp used in Step1Widget. All the trajp are in raw coordinates (before stitching)
class MLCacheTrajectories
{
public:
	MLCacheTrajectories();
	virtual ~MLCacheTrajectories() { clear(); };
	
	bool isDetectPrepared() const;
	bool isTrackPrepared() const;

	// this will update detect_boxes_list
	bool tryLoadDetectionFromFile();
	// this will update track_boxes_list and frameidx/objid2trajectories
	bool tryLoadTrackFromFile();
	// this will update all global_rects
	bool tryLoadGlobalTrackBoxes();
	bool tryLoadGlobalTrackBoxes_deprecated();
	bool tryLoadGlobalDetectBoxes();

	bool tryLoadAll()
	{
		if (!tryLoadDetectionFromFile()) return false;
		if (!tryLoadTrackFromFile()) return false;
		if (!tryLoadGlobalTrackBoxes()) return false;
		if (!tryLoadGlobalDetectBoxes()) return false;
		return true;
	}

	// this will save global_rects to files
	bool saveGlobalTrackBoxes() const;
	bool saveGlobalDetectBoxes() const;

	bool isDetectOk();
	
	bool isTrackOk();
	bool isDetectGlobalBoxOk();
	bool isTrackGlobalBoxOk();

	void clear()
	{
		// free memory
		for (auto& pbox : track_boxes_list)
			delete pbox;
		for (auto& pbox : detect_boxes_list)
			delete pbox;

		detect_boxes_list.clear();
		frameidx2detectboxes.clear();
		track_boxes_list.clear();
		frameidx2trackboxes.clear();
		objid2trajectories.clear();
	}

	QColor getColor(const ObjID& id) const;

private:
	bool saveWorldRects(const QVector<BBox*>& boxes, const std::string& filepath) const;
	bool loadWorldRects(QVector<BBox*>& boxes, const std::string& filepath) const;

public:
	static QVector<QString> classid2name;
	static QHash<QString, int> name2classid;
	static QHash<ObjID, QColor> colormap;

	QVector<BBox*> detect_boxes_list;
	QVector<QVector<BBox*>> frameidx2detectboxes;

	QVector<BBox*> track_boxes_list;
	QVector<QMap<ObjID, BBox*>> frameidx2trackboxes;
	QMap<ObjID, Traject> objid2trajectories;
};

