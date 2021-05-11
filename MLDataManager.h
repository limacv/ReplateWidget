#pragma once
#include <qimage.h>
#include <qvector.h>
#include <qstring.h>
#include <opencv2/core.hpp>
#include <array>

class MLStep1Data;
class MLStep2Data;

class MLDataManager
{
public:
	MLDataManager()
		:step1datap(nullptr), step2datap(nullptr)
	{}
	~MLDataManager() {};
	static MLDataManager& get()
	{
		static MLDataManager global_manager;
		return global_manager;
	}

	bool load_raw_video(const QString& path);
	int get_framecount() const { return raw_frames.size(); }
	bool is_prepared(int step) const;

public:
	cv::Size raw_frame_size;
	QString video_filename;
	QVector<cv::Mat> raw_frames;
	QVector<cv::Mat> stitched_frames;

	MLStep1Data* step1datap;
	MLStep2Data* step2datap;
};

