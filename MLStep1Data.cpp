#include "MLStep1Data.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <qmessagebox.h>
#include <qtextstream.h>

bool MLStep1Data::tryLoadDetectionFromFile()
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
			centerx = (int)(lines.at(0).toFloat() * globaldata.raw_frame_size.width);
			centery = (int)(lines.at(1).toFloat() * globaldata.raw_frame_size.height);
			width = (int)(lines.at(2).toFloat() * globaldata.raw_frame_size.width);
			height = (int)(lines.at(3).toFloat() * globaldata.raw_frame_size.height);
			confidence = lines.at(4).toFloat();
			classid = lines.at(5).toInt();
			detect_boxes_list.push_back(
				BBox(i, classid, -1, centery - height / 2, centerx - width / 2, width, height, confidence)
			);
		}
	}
	return true;
}

bool MLStep1Data::tryLoadTrackFromFile()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	
	//QVector<BBox> track_boxes_list;
	//QVector<QVector<BBox*>> frameidx2boxes;
	//QMap<int, QVector<BBox*>> objectid2boxes;
	const int framesize = globaldata.get_framecount();
	frameidx2boxes.resize(framesize);
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
			
			BBox box(frameidx, classid, instanceid, top, left, width, height, 1);
			track_boxes_list.push_back(box);
			BBox* pbox = &track_boxes_list[track_boxes_list.size() - 1];
			frameidx2boxes[frameidx].insert(ObjID(classid, instanceid), pbox);
			objectid2boxes[ObjID(classid, instanceid)].push_back(pbox);
		}
	}
	return true;
}
