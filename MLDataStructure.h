#pragma once
#include <opencv2/core.hpp>
#include <qpair.h>
#include <qstring.h>
#include <qpainter.h>
#include <qvector>

struct VideoConfig
{	
	VideoConfig()
		:framecount(-1),
		fourcc(-1.),
		fps(-1.),
		size(-1, -1),
		translation(0, 0),
		scaling(1, 1),
		rotation(0),
		repeat(1)
	{ }

	bool isempty() const { return framecount <= 0; }
	void clear()
	{
		framecount = -1;
		fourcc = -1.;
		fps = -1.;
		size = cv::Size(-1, -1);
		translation = cv::Point(-1, -1);
		scaling = cv::Vec2f(1, 1);
		rotation = 0;
		repeat = 1;
	}
	int framecount;
	int fourcc;
	double fps;
	cv::Size size;
	QString filepath;

	cv::Point translation;
	cv::Vec2f scaling;
	float rotation;
	int repeat;
};


struct BBox
{
	BBox()
		:frameidx(-1), classid(-1), instanceid(-1), 
		rect(INT_MIN, INT_MIN, INT_MIN, INT_MIN),
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}
	BBox(int frameidx, int classid, int instanceid, cv::Rect rect)
		:frameidx(frameidx), classid(classid), instanceid(instanceid), 
		rect(rect), 
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}
	BBox(int frameidx, int classid, int instanceid, int top, int left, int wid, int hei)
		:frameidx(frameidx), classid(classid), instanceid(instanceid), 
		rect(left, top, wid, hei), 
		rect_global(INT_MIN, INT_MIN, INT_MIN, INT_MIN) {}

	int frameidx;
	int classid;
	int instanceid;
	cv::Rect rect;
	cv::Rect rect_global;

	bool empty() const { return frameidx < 0 || rect.width < 0 || rect.height < 0; }
	bool rectglobalupdated() const { return rect_global.width > 0 && rect_global.height > 0; }
};

using ObjID = QPair<int, int>;

struct Traject
{
	Traject()
		:beginidx(100000000), endidx(0)
	{}
	Traject(int totalframecount)
		:beginidx(100000000), endidx(0), boxes(totalframecount, nullptr)
	{}

	int beginidx;
	int endidx;
	QVector<BBox*> boxes;  // always has same size as totalframecount, when one frame is invalid, the BBox* is set to nullptr
	
	BBox* operator[](const int idx) const
	{
		return boxes[idx];
	}
	int length() const
	{
		return endidx > beginidx ? endidx - beginidx + 1 : 0;
	}
	void insert(const int idx, BBox* boxp)
	{
		if (boxes[idx] == nullptr && boxp != nullptr)
		{
			beginidx = MIN(idx, beginidx);
			endidx = MAX(idx, endidx);
			boxes[idx] = boxp;
		}
		else if (boxes[idx] != nullptr && boxp == nullptr)
		{
			boxes[idx] = boxp;
			updatebeginend();
		}
		else
			boxes[idx] = boxp;
	}
	void updatebeginend()
	{
		for (int i = 0; i < boxes.size(); ++i)
			if (boxes[i] != nullptr)
			{
				 beginidx = i;
				 break;
			}
		for (int i = boxes.size() - 1; i >= 0; --i)
			if (boxes[i] != nullptr)
			{
				endidx = i;
				break;
			}
	}
};


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
