#pragma once
#include <opencv2/core.hpp>
#include <qpair.h>

struct BBox
{
	BBox()
		:frameidx(-1), classid(-1), instanceid(-1), 
		clipoffsetf(-1), clipoffsetb(-1),
		rect(INT_MIN, INT_MIN, INT_MIN, INT_MIN),
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}
	BBox(int frameidx, int classid, int instanceid, cv::Rect rect)
		:frameidx(frameidx), classid(classid), instanceid(instanceid), 
		clipoffsetf(-1), clipoffsetb(-1),
		rect(rect), 
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}
	BBox(int frameidx, int classid, int instanceid, int top, int left, int wid, int hei)
		:frameidx(frameidx), classid(classid), instanceid(instanceid), 
		clipoffsetf(-1), clipoffsetb(-1),
		rect(left, top, wid, hei), 
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}

	int frameidx;
	int classid;
	int instanceid;
	cv::Rect rect;
	cv::Rect rect_global;
	int clipoffsetf;  // distance to the former cut position
	int clipoffsetb;  // distance to the next cut position

	bool empty() const { return frameidx < 0 || rect.width < 0 || rect.height < 0; }
	bool rectglobalupdated() const { return rect_global.width > 0 && rect_global.height > 0; }
	bool clipoffsetupdated() const { return clipoffsetf >= 0 || clipoffsetb >= 0; }
};

using ObjID = QPair<int, int>;


class MLStepDataBase
{
public:
	MLStepDataBase()
		: expired(false)
	{}
	virtual ~MLStepDataBase() {}

private:
	bool expired;
};

