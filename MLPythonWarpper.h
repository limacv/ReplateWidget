#pragma once
#include "MLConfigManager.h"
#pragma push_macro("slots")
#undef slots
#include "Python.h"
#pragma pop_macro("slots")

inline
int callDetectPy()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();
	
	QString cmd = QString("%1 %2/detect.pyw --weights %3 --source %4 --save_path %5 --img-size %6").arg(
		pathcfg.get_python_path(),
		pathcfg.get_yolov5_path(),
		pathcfg.get_yolov5_weight(),
		pathcfg.get_raw_video_path(),
		pathcfg.get_cache_path(),
		QString::number(globaldata.raw_frame_size.width)
	);
	qDebug("Step1Widget::executing command: %s", cmd.toLocal8Bit().constData());
	return system(cmd.toLocal8Bit().constData());
	//const int argc = 10;
	//const wchar_t* argv[argc];
	//auto arg0 = pathcfg.get_python_path().toStdWString();
	//argv[0] = arg0.c_str();
	//auto arg1 = pathcfg.get_yolov5_path().append("/detect.py").toStdWString();
	//argv[1] = arg1.c_str();
	//argv[2] = L"--weights";
	//auto arg3 = pathcfg.get_yolov5_weight().toStdWString();
	//argv[3] = arg3.c_str();
	//argv[4] = L"--source";
	//auto arg5 = pathcfg.get_raw_video_path().toStdWString();
	//argv[5] = arg5.c_str();
	//argv[6] = L"--save_path";
	//auto arg7 = pathcfg.get_cache_path().toStdWString();
	//argv[7] = arg7.c_str();
	//argv[8] = L"--img-size";
	//auto arg9 = QString::number(globaldata.raw_frame_size.width).toStdWString();
	//argv[9] = arg9.c_str();
	//
	//wchar_t* program = Py_DecodeLocale("detector", NULL);
	//if (program == NULL)
	//{
	//	qCritical("MLPythonWarpper::callDetectPy cannot decode string, check installtion of python");
	//	return -1;
	//}
	//Py_SetProgramName(program);
	//Py_Initialize();
	//PySys_SetArgv(argc, const_cast<wchar_t**>(argv));
	//
	//FILE* file = _Py_wfopen(arg1.c_str(), L"r");
	//if (file == NULL)
	//{
	//	qCritical("MLPythonWarpper::callDetectPy cannot open python script");
	//	return -1;
	//}
	//PyRun_SimpleString("print('fejiageogoiaegoiagnoia')");
	//PyRun_SimpleFile(file, "detect.py");
	//PyErr_Print();
	//int ret = Py_FinalizeEx();
	//PyMem_RawFree(program);
	//return ret;
}

inline
int callTrackPy()
{
	const auto& pathcfg = MLConfigManager::get();
	const auto& globaldata = MLDataManager::get();

	QString cmd = QString("%1 %2/track.pyw --source %3 --save_path %4 --img-size %5").arg(
		pathcfg.get_python_path(),
		pathcfg.get_yolov5_path(),
		pathcfg.get_raw_video_path(),
		pathcfg.get_cache_path(),
		QString::number(globaldata.raw_frame_size.width)
	);
	qDebug("Step1Widget::executing command: %s", cmd.toLocal8Bit().constData());
	return system(cmd.toLocal8Bit().constData());
}
