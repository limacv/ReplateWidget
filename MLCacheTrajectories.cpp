#include "MLCacheTrajectories.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <qmessagebox.h>
#include <qtextstream.h>
#include <qrandom.h>

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

QString MLCacheTrajectories::classidfilter = "0 1 2 3 4 5 6 7 8 15 16 17 18 19 20 21 22 23 30 31 32 33 34 35 36 37 38";
QHash<QString, int> MLCacheTrajectories::name2classid;
QHash<ObjID, QColor> MLCacheTrajectories::colormap;

MLCacheTrajectories::MLCacheTrajectories()
{
	if (name2classid.isEmpty())
	{
		for (int i = 0; i < classid2name.size(); ++i)
			name2classid[classid2name[i]] = i;
	}
}

bool MLCacheTrajectories::isDetectPrepared() const
{
	return frameidx2detectboxes.size() == (MLDataManager::get().get_framecount());
}

bool MLCacheTrajectories::isTrackPrepared() const
{
	return frameidx2trackboxes.size() == (MLDataManager::get().get_framecount());
}

bool MLCacheTrajectories::tryLoadDetectionFromFile()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();
	detect_boxes_list.clear();
	frameidx2detectboxes.clear();
	frameidx2detectboxes.resize(framecount);
	for (int i = 0; i < framecount; ++i)
	{
		QFile file(pathcfg.get_detect_result_cache(i));
		if (!file.open(QIODevice::ReadOnly))
		{
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
			BBox* pbox = new BBox(i, classid, -1, centery - height / 2, centerx - width / 2, width, height);
			detect_boxes_list.push_back(pbox);
			frameidx2detectboxes[i].push_back(pbox);
		}
	}
	return true;
}

bool MLCacheTrajectories::tryLoadTrackFromFile()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	
	//QVector<BBox> track_boxes_list;
	//QVector<QVector<BBox*>> frameidx2trackboxes;
	//QMap<int, QVector<BBox*>> objid2trajectories;
	const int framecount = globaldata.get_framecount();
	track_boxes_list.clear();
	frameidx2trackboxes.clear();
	objid2trajectories.clear();
	frameidx2trackboxes.resize(framecount);
	{
		QFile file(pathcfg.get_track_result_cache());
		if (!file.open(QIODevice::ReadOnly))
		{
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
			frameidx2trackboxes[frameidx].insert(ObjID(classid, instanceid), pbox);
			if (objid2trajectories.find(ObjID(classid, instanceid)) == objid2trajectories.end())
				objid2trajectories[ObjID(classid, instanceid)] = Traject(framecount);
			objid2trajectories[ObjID(classid, instanceid)].insert(frameidx, pbox);
		}
	}
	return true;
}

bool MLCacheTrajectories::tryLoadGlobalTrackBoxes_deprecated()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();
	// read track file
	std::ifstream rectfile(pathcfg.get_stitch_track_bboxes_path().toStdString());
	if (!rectfile.is_open())
	{
		qWarning("MLCacheStitching::failed to open stitch track file");
		return false;
	}
	rectfile.ignore(1024, '\n');
	int rectcount = 0;
	while (rectfile)
	{
		int frameidx = -1, classid, instanceid, x, y, wid, hei;
		rectfile >> frameidx >> classid >> instanceid >> x >> y >> wid >> hei;
		if (frameidx < 0 || frameidx >= framecount)
			continue;
		auto& boxes = frameidx2trackboxes[frameidx];
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

bool MLCacheTrajectories::tryLoadGlobalDetectBoxes()
{
	const auto& pathcfg = MLConfigManager::get();
	return loadWorldRects(detect_boxes_list, pathcfg.get_stitch_detect_bboxes_path().toStdString());
}

bool MLCacheTrajectories::saveGlobalDetectBoxes() const
{
	const auto& pathcfg = MLConfigManager::get();
	return saveWorldRects(detect_boxes_list, pathcfg.get_stitch_detect_bboxes_path().toStdString());
}

bool MLCacheTrajectories::isDetectOk()
{
	const auto& global_data = MLDataManager::get();
	return !detect_boxes_list.empty()
		&& (frameidx2detectboxes.size() == global_data.get_framecount());
}


bool MLCacheTrajectories::tryLoadGlobalTrackBoxes()
{
	const auto& pathcfg = MLConfigManager::get();
	return loadWorldRects(track_boxes_list, pathcfg.get_stitch_track_bboxes_path().toStdString());
}

bool MLCacheTrajectories::saveGlobalTrackBoxes() const
{
	const auto& pathcfg = MLConfigManager::get();
	return saveWorldRects(track_boxes_list, pathcfg.get_stitch_track_bboxes_path().toStdString());
}

bool MLCacheTrajectories::isTrackOk()
{
	const auto& global_data = MLDataManager::get();
	return !track_boxes_list.empty()
		&& (frameidx2trackboxes.size() == global_data.get_framecount());
}

bool MLCacheTrajectories::isDetectGlobalBoxOk()
{
	int count = 0;
	for (const auto& pbox : detect_boxes_list)
		if (!pbox->rectglobalupdated())
			count++;
	return count < detect_boxes_list.size() * 0.1 + 1;
}

bool MLCacheTrajectories::isTrackGlobalBoxOk()
{
	int count = 0;
	for (const auto& pbox : track_boxes_list)
		if (!pbox->rectglobalupdated())
			count++;
	return count < track_boxes_list.size() * 0.1 + 1;
}

QColor MLCacheTrajectories::getColor(const ObjID& id) const
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

bool MLCacheTrajectories::saveWorldRects(const QVector<BBox*>& boxes, const std::string& filepath) const
{
	std::ofstream file(filepath);
	if (!file.is_open()) return false;

	file << "# frameidx, classid, instanceid, rect.x, rect.y, rect.wid, rect.hei" << std::endl;
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

bool MLCacheTrajectories::loadWorldRects(QVector<BBox*>& boxes, const std::string& filepath) const
{
	const auto& globaldata = MLDataManager::get();
	const int framecount = globaldata.get_framecount();
	// read track file
	std::ifstream rectfile(filepath);
	if (!rectfile.is_open())
	{
		qWarning("MLCacheStitching::failed to open stitch track file");
		return false;
	}
	if (boxes.empty())
	{
		qWarning("MLCacheStitching::try to load world rects before initializing the BBoxes");
		return false;
	}

	rectfile.ignore(1024, '\n');
	int rectcount = 0;
	while (rectfile)
	{
		int frameidx = -1, classid, instanceid, x, y, wid, hei;
		rectfile >> frameidx >> classid >> instanceid >> x >> y >> wid >> hei;
		if (frameidx < 0 || frameidx >= framecount)
			continue;
		auto& pbox = boxes[rectcount];
		if (pbox->frameidx != frameidx
			|| pbox->classid != classid
			|| pbox->instanceid != instanceid)
			break;
		
		pbox->rect_global = cv::Rect(x, y, wid, hei);
		++rectcount;
	}
	if (rectcount != boxes.size())
		return false;
	return true;
}