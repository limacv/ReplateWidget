#include "MLCacheTrajectories.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <qmessagebox.h>
#include <qtextstream.h>
#include <qrandom.h>
#include <fstream>

QVector<QString> MLCacheTrajectories::classid2name = {
	"person", "bicycle", "car", "motorcycle", "airplane", 
	"bus", "train", "truck", "boat", "traffic light", 
	"fire hydrant", "stop sign", "parking meter", "bench", "bird", 
	"cat", "dog", "horse", "sheep", "cow", 
	"elephant", "bear", "zebra", "giraffe", "backpack", 
	"umbrella", "handbag", "tie", "suitcase", "frisbee", 
	"skis", "snowboard", "sports ball", "kite", "baseball bat", 
	"baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", 
	"wine glass", "cup", "fork", "knife", "spoon", 
	"bowl", "banana", "apple", "sandwich", "orange", 
	"broccoli", "carrot", "hot dog", "pizza", "donut", 
	"cake", "chair", "couch", "potted plant", "bed", 
	"dining table", "toilet", "tv", "laptop", "mouse", 
	"remote", "keyboard", "cell phone", "microwave", "oven", 
	"toaster", "sink", "refrigerator", "book", "clock", 
	"vase", "scissors", "teddy bear", "hair drier", "toothbrush"
};

MLCacheTrajectories::~MLCacheTrajectories()
{
	for (auto& pbox : track_boxes_list)
		delete pbox;
}

bool MLCacheTrajectories::tryLoadDetectionFromFile()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();

	for (int i = 0; i < globaldata.get_framecount(); ++i)
	{
		QFile file(pathcfg.get_detect_result_cache(i));
		if (!file.open(QIODevice::ReadOnly))
		{
			QMessageBox::warning(nullptr, "Warning", file.errorString());
			return false;
		}
		
		QTextStream fs(&file);
		fs.readLine();  // pass the comment line
		while (!fs.atEnd())
		{
			QString line = fs.readLine();
			auto lines = line.split(' ');
			int centerx, centery, width, height;
			float confidence;
			int classid;
			centerx = (int)(lines.at(0).toFloat() * globaldata.raw_video_cfg.size.width);
			centery = (int)(lines.at(1).toFloat() * globaldata.raw_video_cfg.size.height);
			width = (int)(lines.at(2).toFloat() * globaldata.raw_video_cfg.size.width);
			height = (int)(lines.at(3).toFloat() * globaldata.raw_video_cfg.size.height);
			confidence = lines.at(4).toFloat();
			classid = lines.at(5).toInt();
			detect_boxes_list.push_back(
				BBox(i, classid, -1, centery - height / 2, centerx - width / 2, width, height)
			);
		}
	}
	return true;
}

bool MLCacheTrajectories::tryLoadTrackFromFile()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	
	//QVector<BBox> track_boxes_list;
	//QVector<QVector<BBox*>> frameidx2boxes;
	//QMap<int, QVector<BBox*>> objid2trajectories;
	const int framecount = globaldata.get_framecount();
	frameidx2boxes.resize(framecount);
	{
		QFile file(pathcfg.get_track_result_cache());
		if (!file.open(QIODevice::ReadOnly))
		{
			QMessageBox::warning(nullptr, "Warning", file.errorString());
			return false;
		}
		QTextStream fs(&file);
		fs.readLine();  // skip the comment line
		while (!fs.atEnd())
		{
			QString line = fs.readLine();
			auto lines = line.split(' ');
			int left, top, width, height;
			int classid, instanceid, frameidx;
			frameidx = lines.at(0).toInt();
			classid = lines.at(1).toInt();
			instanceid = lines.at(2).toInt();
			left = lines.at(3).toInt();
			top = lines.at(4).toInt();
			width = lines.at(5).toInt();
			height = lines.at(6).toInt();
			
			BBox* pbox = new BBox(frameidx, classid, instanceid, top, left, width, height);
			track_boxes_list.push_back(pbox);
			frameidx2boxes[frameidx].insert(ObjID(classid, instanceid), pbox);
			if (objid2trajectories.find(ObjID(classid, instanceid)) == objid2trajectories.end())
				objid2trajectories[ObjID(classid, instanceid)] = Traject(framecount);
			objid2trajectories[ObjID(classid, instanceid)].insert(frameidx, pbox);
		}
	}
	return true;
}

bool MLCacheTrajectories::tryLoadGlobalBoxes()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();
	// read track file
	std::ifstream rectfile(pathcfg.get_stitch_bboxes_path().toStdString());
	if (!rectfile.is_open())
	{
		qWarning("MLCacheStitching::failed to open stitch track file");
		return false;
	}
	rectfile.ignore(1024, '\n');
	int rectcount = 0;
	while (rectfile)
	{
		int frameidx, classid, instanceid, x, y, wid, hei;
		rectfile >> frameidx >> classid >> instanceid >> x >> y >> wid >> hei;
		if (frameidx < 0 || frameidx >= framecount)
			return false;
		auto& boxes = frameidx2boxes[frameidx];
		auto& it = boxes.find(ObjID(classid, instanceid));
		if (it == boxes.end())
			return false;
		it.value()->rect_global = cv::Rect(x, y, wid, hei);
		++rectcount;
	}
	if (rectcount != track_boxes_list.size())
		return false;
	return true;
}


bool MLCacheTrajectories::saveGlobalBoxes() const
{
	const auto& pathcfg = MLConfigManager::get();
	std::ofstream file(pathcfg.get_stitch_bboxes_path().toStdString());
	if (!file.is_open()) return false;

	file << "# frameidx, classid, instanceid, rect.x, rect.y, rect.wid, rect.hei" << std::endl;
	const auto& boxes = track_boxes_list;
	for (const auto& box : boxes)
		file << box->frameidx << " "
		<< box->classid << " "
		<< box->instanceid << " "
		<< box->rect_global.x << " "
		<< box->rect_global.y << " "
		<< box->rect_global.width << " "
		<< box->rect_global.height << std::endl;
	return true;
}

QColor MLCacheTrajectories::getColor(const ObjID& id)
{
	if (colormap.contains(id))
		return colormap.value(id);
	else
	{
		double h = QRandomGenerator::global()->generateDouble();
		QColor color; color.setHsvF(h, 0.8, 0.8);
		colormap.insert(id, color);
		return color;
	}
}

