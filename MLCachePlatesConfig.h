#pragma once
#include "MLDataStructure.h"
#include <qvector.h>
#include <qmap.h>

class MLCacheStitching;
class MLCacheTrajectories;

class MLCachePlatesConfig
{
public:
	MLCachePlatesConfig(MLCacheStitching* sp, MLCacheTrajectories* tp)
		:replate_duration(-1), replate_offset(0), stitch_datap(sp), trajs_datap(tp)
	{ }
	
	void initialize_cut(int outframecount);

	void collectPlate(const float framepct, const ObjID& objid, QVector<BBox*>& boxes);
	void collectPlate(const float framepct, QVector<BBox*>& boxes);

private:
	MLCacheStitching* stitch_datap;
	MLCacheTrajectories* trajs_datap;

public:
	int replate_duration;
	int replate_offset;
	QMap<ObjID, QVector<int>> cut;  // the cut frameidx is not necessarily lies inside totalframes
};

