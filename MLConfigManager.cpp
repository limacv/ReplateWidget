#include "MLConfigManager.h"
#include <fstream>
#include <qdebug.h>
#include <qvalidator.h>
#include "ui_MLConfigWidget.h"

void MLConfigManager::restore_to_default()
{
	stitcher_cfg.restore_default();
	detectrack_cfg.restore_default();
	update_ui();
}

void MLConfigManager::setup_ui(QWidget* parent)
{
	config_widget = new QDockWidget("Configuration", parent);
	config_widget->setWidget(new QWidget(parent));
	config_widget->setFloating(true);
	config_widget->resize(461, 581);
	config_widget->hide();

	config_widget_ui = new Ui::MLConfigWidget();
	config_widget_ui->setupUi(config_widget->widget());
	
	QDoubleValidator* validatorf01 = new QDoubleValidator(0, 1, 1000, parent);
	QIntValidator* validatorint = new QIntValidator(-1, 10000, parent);
	QDoubleValidator* validatorf0 = new QDoubleValidator(0, 100, 1000, parent);
	config_widget_ui->workSizeLineEdit->setValidator(validatorint);
	config_widget_ui->confThresLineEdit->setValidator(validatorf01);
	config_widget_ui->ioUThresLineEdit->setValidator(validatorf01);
	config_widget_ui->matchConfLineEdit->setValidator(validatorf01);
	config_widget_ui->matchRangeLineEdit->setValidator(validatorint);
	config_widget_ui->blendStrengthLineEdit->setValidator(validatorint);
	config_widget_ui->workScaleLineEdit->setValidator(validatorf0);
	config_widget_ui->seamScaleLineEdit->setValidator(validatorf0);
	config_widget_ui->composeScaleLineEdit->setValidator(validatorf0);
	config_widget_ui->expoCompNrfeedsLineEdit->setValidator(validatorint);
	config_widget_ui->expoCompNrfilteringLineEdit->setValidator(validatorint);
	config_widget_ui->expoCompBlockszLineEdit->setValidator(validatorint);

	QObject::connect(config_widget_ui->yolov5PathLineEdit, &QLineEdit::textEdited, [this](const QString& str) {detectrack_cfg.detect_yolov5_path = str; });
	QObject::connect(config_widget_ui->pythonPathLineEdit, &QLineEdit::textEdited, [this](const QString& str) {detectrack_cfg.detect_python_path = str; });
	QObject::connect(config_widget_ui->yolov5WeightCombo, &QComboBox::currentTextChanged, [this](const QString& str) {detectrack_cfg.detect_yolov5_weight = str; });
	QObject::connect(config_widget_ui->workSizeLineEdit, &QLineEdit::textEdited, [this](const QString& str) {detectrack_cfg.detect_image_size = str.toInt(); });
	QObject::connect(config_widget_ui->confThresLineEdit, &QLineEdit::textEdited, [this](const QString& str) {detectrack_cfg.detect_conf_threshold = str.toDouble(); });
	QObject::connect(config_widget_ui->ioUThresLineEdit, &QLineEdit::textEdited, [this](const QString& str) {detectrack_cfg.detect_iou_threshold = str.toDouble(); });
	QObject::connect(config_widget_ui->deviceComboBox, &QComboBox::currentTextChanged, [this](const QString& str) {detectrack_cfg.detect_device = str; });
	QObject::connect(config_widget_ui->saveVisualCheckBox, &QCheckBox::toggled, [this](bool s) {detectrack_cfg.detect_save_visualization = s; });
	
	QObject::connect(config_widget_ui->skipFrameSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
		[this](int v) {stitcher_cfg.stitch_skip_frame_ = v; });
	QObject::connect(config_widget_ui->featureTypeComboBox, &QComboBox::currentTextChanged, [this](const QString& str) {stitcher_cfg.features_type_ = str.toStdString(); });
	QObject::connect(config_widget_ui->matchConfLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.match_conf_ = str.toDouble(); });
	QObject::connect(config_widget_ui->matchRangeLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.match_range_width = str.toInt(); });
	QObject::connect(config_widget_ui->warpTypeComboBox, &QComboBox::currentTextChanged, [this](const QString& str) {stitcher_cfg.warp_type_ = str.toStdString(); });
	QObject::connect(config_widget_ui->blendTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[this](int i) {stitcher_cfg.blend_type_ = i; });
	QObject::connect(config_widget_ui->blendStrengthLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.blend_strength_ = str.toDouble(); });
	QObject::connect(config_widget_ui->waveCorrectComboBox, &QComboBox::currentTextChanged, [this](const QString& str) {stitcher_cfg.wave_correct_ = str.toStdString(); });
	QObject::connect(config_widget_ui->workScaleLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.work_megapix_ = str.toDouble(); });
	QObject::connect(config_widget_ui->seamScaleLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.seam_megapix_ = str.toDouble(); });
	QObject::connect(config_widget_ui->composeScaleLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.compose_megapix_ = str.toDouble(); });
	QObject::connect(config_widget_ui->expoCompTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
		[this](int i) {stitcher_cfg.expos_comp_type_ = i; });
	QObject::connect(config_widget_ui->expoCompNrfeedsLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.expos_comp_nr_feeds = str.toInt(); });
	QObject::connect(config_widget_ui->expoCompNrfilteringLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.expos_comp_nr_filtering = str.toInt(); });
	QObject::connect(config_widget_ui->expoCompBlockszLineEdit, &QLineEdit::textEdited, [this](const QString& str) {stitcher_cfg.expos_comp_block_size = str.toInt(); });
	QObject::connect(config_widget_ui->stitcherTypeComboBox, &QComboBox::currentTextChanged, [this](const QString& str) {stitcher_cfg.stitcher_type_ = str.toStdString(); });
	QObject::connect(config_widget_ui->tryGPUCheckBox, &QCheckBox::toggled, [this](bool s) {stitcher_cfg.try_gpu_ = s; });

	QObject::connect(config_widget_ui->setdefaultButton, &QPushButton::clicked, [this]() {restore_to_default(); });
	QObject::connect(config_widget_ui->saveButton, &QPushButton::clicked, [this]() {writeToFile(get_localconfig_path()); });
}

void MLConfigManager::update_videopath(const QString& video_path)
{
	auto ff = QFileInfo(video_path);
	raw_video_path = ff.absoluteFilePath();
	basename = ff.baseName();

	if (!QDir().mkpath(get_cache_path()))
		qWarning("MLConfigManager::failed to create directory %s", qPrintable(get_cache_path()));
}

void MLConfigManager::update_ui()
{
	if (config_widget == nullptr || config_widget_ui == nullptr)
		return;

	config_widget_ui->cachePathLineEdit->setText(cache_root);
	config_widget_ui->videoPathLineEdit->setText(raw_video_path);
	config_widget_ui->videoNameLineEdit->setText(basename);
	
	config_widget_ui->yolov5PathLineEdit->setText(detectrack_cfg.detect_yolov5_path);
	config_widget_ui->pythonPathLineEdit->setText(detectrack_cfg.detect_python_path);
	config_widget_ui->yolov5WeightCombo->setCurrentText(detectrack_cfg.detect_yolov5_weight);
	config_widget_ui->workSizeLineEdit->setText(QString::number(detectrack_cfg.detect_image_size));
	config_widget_ui->confThresLineEdit->setText(QString::number(detectrack_cfg.detect_conf_threshold));
	config_widget_ui->ioUThresLineEdit->setText(QString::number(detectrack_cfg.detect_iou_threshold));
	config_widget_ui->deviceComboBox->setCurrentText(detectrack_cfg.detect_device);
	config_widget_ui->saveVisualCheckBox->setChecked(detectrack_cfg.detect_save_visualization);

	config_widget_ui->skipFrameSpinBox->setValue(stitcher_cfg.stitch_skip_frame_);
	config_widget_ui->featureTypeComboBox->setCurrentText(QString::fromStdString(stitcher_cfg.features_type_));
	config_widget_ui->matchConfLineEdit->setText(QString::number(stitcher_cfg.match_conf_));
	config_widget_ui->matchRangeLineEdit->setText(QString::number(stitcher_cfg.match_range_width));
	config_widget_ui->warpTypeComboBox->setCurrentText(QString::fromStdString(stitcher_cfg.warp_type_));
	config_widget_ui->blendTypeComboBox->setCurrentIndex(stitcher_cfg.blend_type_);
	config_widget_ui->blendStrengthLineEdit->setText(QString::number(stitcher_cfg.blend_strength_));
	config_widget_ui->waveCorrectComboBox->setCurrentText(QString::fromStdString(stitcher_cfg.wave_correct_));
	config_widget_ui->workScaleLineEdit->setText(QString::number(stitcher_cfg.work_megapix_));
	config_widget_ui->seamScaleLineEdit->setText(QString::number(stitcher_cfg.seam_megapix_));
	config_widget_ui->composeScaleLineEdit->setText(QString::number(stitcher_cfg.compose_megapix_));
	config_widget_ui->expoCompTypeComboBox->setCurrentIndex(stitcher_cfg.expos_comp_type_);
	config_widget_ui->expoCompNrfeedsLineEdit->setText(QString::number(stitcher_cfg.expos_comp_nr_feeds));
	config_widget_ui->expoCompNrfilteringLineEdit->setText(QString::number(stitcher_cfg.expos_comp_nr_filtering));
	config_widget_ui->expoCompBlockszLineEdit->setText(QString::number(stitcher_cfg.expos_comp_block_size));
	config_widget_ui->stitcherTypeComboBox->setCurrentText(QString::fromStdString(stitcher_cfg.stitcher_type_));
	config_widget_ui->tryGPUCheckBox->setChecked(stitcher_cfg.try_gpu_);
}

void MLConfigManager::readFromFile(const QString& cfgpath)
{
	try
	{
		YAML::Node config = YAML::LoadFile(cfgpath.toStdString());
	
		if (config["cache_root"])
		{
			cache_root = QString::fromStdString(config["cache_root"].as<string>());
			cache_root = QFileInfo(cache_root).absoluteFilePath();
		}

		stitcher_cfg.read(config);
		detectrack_cfg.read(config);
	}
	catch (const YAML::ParserException& e)
	{
		qWarning() << e.what();
	}
	catch (const YAML::BadFile& e)
	{
		qWarning() << "cannot find file" << cfgpath;
	}
	update_ui();
}

void MLConfigManager::writeToFile(const QString& cfgpath) const
{
	YAML::Emitter emiter;
	ofstream file(cfgpath.toStdString());
	if (file.is_open())
		qWarning() << "Failed to write to config file: " << cfgpath;

	emiter << YAML::BeginMap;
	emiter << YAML::Key << "cache_root" << YAML::Value << cache_root.toStdString();
	emiter << YAML::Key << "video_path" << YAML::Value << raw_video_path.toStdString();
	emiter << YAML::Key << "video_name" << YAML::Value << basename.toStdString();

	stitcher_cfg.write(emiter);
	detectrack_cfg.write(emiter);

	emiter << YAML::EndMap;

	file << emiter.c_str();

}