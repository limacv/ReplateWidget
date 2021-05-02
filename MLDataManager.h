#pragma once
#include <qimage.h>
#include <qvector.h>
#include <qstring.h>
#include <opencv2/core.hpp>

class MLStep1Data;
class MLStep2Data;

class MLDataManager
{
public:
	MLDataManager()
		:step1data(nullptr), step2data(nullptr) 
	{}
	~MLDataManager() {};
	static MLDataManager& get()
	{
		static MLDataManager global_manager;
		return global_manager;
	}

	bool load_raw_video(const QString& path);
	int get_framecount() const { return raw_frames.size(); }

public:
	cv::Size raw_frame_size;
	QString video_filename;
	QVector<cv::Mat> raw_frames;
	QVector<cv::Mat> stitched_frames;

	MLStep1Data* step1data;
	MLStep1Data* step2data;
};

