/***************************************************************************
 *  Copyright 2014, 2015, 2017 Marcelo Y. Matuda                           *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "SynthesisWindow.h"

#include <memory>
#include <string>

#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <QString>
#include <QStringList>

#include "AudioWorker.h"
#include "Controller.h"
#include "Model.h"
#include "PhoneticStringParser.h"
#include "Synthesis.h"
#include "TextParser.h"
#include "ui_SynthesisWindow.h"

#define VTM_PARAM_FILE_NAME "generated__vtm_param.txt"



namespace GS {

SynthesisWindow::SynthesisWindow(QWidget* parent)
		: QWidget{parent}
		, ui_{std::make_unique<Ui::SynthesisWindow>()}
		, model_{}
		, synthesis_{}
		, audioWorker_{}
{
	ui_->setupUi(this);

	QFontMetrics fm = fontMetrics();
	int rowHeight = fm.height() + fm.ascent();

	QHeaderView* vHeader = ui_->parameterTableWidget->verticalHeader();
	vHeader->setSectionResizeMode(QHeaderView::Fixed);
	vHeader->setDefaultSectionSize(rowHeight);
	QHeaderView* hHeader = ui_->parameterTableWidget->horizontalHeader();
	hHeader->setStretchLastSection(true);
	hHeader->setSectionResizeMode(QHeaderView::Fixed);
	ui_->parameterTableWidget->setColumnCount(1);
	ui_->parameterTableWidget->setHorizontalHeaderLabels(QStringList() << tr("Parameter"));

	ui_->tempoSpinBox->setRange(0.1, 10.0);
	ui_->tempoSpinBox->setSingleStep(0.1);
	ui_->tempoSpinBox->setDecimals(1);
	ui_->tempoSpinBox->setValue(1.0);

	ui_->xZoomSpinBox->setRange(ui_->parameterWidget->xZoomMin(), ui_->parameterWidget->xZoomMax());
	ui_->xZoomSpinBox->setSingleStep(0.1);
	ui_->xZoomSpinBox->setValue(1.0);

	ui_->yZoomSpinBox->setRange(ui_->parameterWidget->yZoomMin(), ui_->parameterWidget->yZoomMax());
	ui_->yZoomSpinBox->setSingleStep(0.1);
	ui_->yZoomSpinBox->setValue(1.0);

	ui_->parameterScrollArea->setBackgroundRole(QPalette::Base);

	connect(ui_->textLineEdit   , &QLineEdit::returnPressed   , ui_->parseButton, &QPushButton::click);
	connect(ui_->parameterWidget, &ParameterWidget::mouseMoved, this            , &SynthesisWindow::updateMouseTracking);
	connect(ui_->parameterWidget, &ParameterWidget::zoomReset , this            , &SynthesisWindow::resetZoom);
	connect(ui_->parameterScrollArea->verticalScrollBar()  , &QScrollBar::valueChanged, ui_->parameterWidget, &ParameterWidget::getVerticalScrollbarValue);
	connect(ui_->parameterScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, ui_->parameterWidget, &ParameterWidget::getHorizontalScrollbarValue);

	audioWorker_ = new AudioWorker;
	audioWorker_->moveToThread(&audioThread_);
	connect(&audioThread_, &QThread::finished,
			audioWorker_, &AudioWorker::deleteLater);
	connect(this         , &SynthesisWindow::playAudioRequested,
			audioWorker_, &AudioWorker::playAudio);
	connect(audioWorker_ , &AudioWorker::finished,
			this        , &SynthesisWindow::handleAudioFinished);
	connect(audioWorker_ , &AudioWorker::errorOccurred,
			this        , &SynthesisWindow::handleAudioError);
	audioThread_.start();
}

SynthesisWindow::~SynthesisWindow()
{
	audioThread_.quit();
	audioThread_.wait();
}

void
SynthesisWindow::clear()
{
	ui_->parameterTableWidget->setRowCount(0);
	ui_->parameterWidget->updateData(nullptr, nullptr);
	synthesis_ = nullptr;
	model_ = nullptr;
}

void
SynthesisWindow::setup(VTMControlModel::Model* model, Synthesis* synthesis)
{
	if (model == nullptr || synthesis == nullptr) {
		clear();
		return;
	}

	try {
		model_ = model;
		synthesis_ = synthesis;

		ui_->parameterWidget->updateData(&synthesis_->vtmController->eventList(), model_);

		setupParameterTable();
	} catch (...) {
		clear();
		throw;
	}
}

void
SynthesisWindow::on_parseButton_clicked()
{
	if (!synthesis_) {
		return;
	}
	QString text = ui_->textLineEdit->text();
	if (text.trimmed().isEmpty()) {
		return;
	}
	disableProcessingButtons();

	try {
		std::string phoneticString = synthesis_->textParser->parse(text.toUtf8().constData());
		ui_->phoneticStringTextEdit->setPlainText(phoneticString.c_str());
	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
		enableProcessingButtons();
		return;
	}

	on_synthesizeButton_clicked();
}

void
SynthesisWindow::on_synthesizeButton_clicked()
{
	if (!synthesis_) {
		return;
	}
	QString phoneticString = ui_->phoneticStringTextEdit->toPlainText();
	if (phoneticString.trimmed().isEmpty()) {
		return;
	}
	disableProcessingButtons();

	try {
		QString vtmParamFilePath = synthesis_->projectDir + VTM_PARAM_FILE_NAME;

		VTMControlModel::Configuration& config = synthesis_->vtmController->vtmControlModelConfiguration();
		config.tempo = ui_->tempoSpinBox->value();

		const ConfigurationData& vtmConfig = synthesis_->vtmController->vtmConfigurationData();
		const double sampleRate = vtmConfig.value<double>("output_rate");

		audioWorker_->player().fillBuffer([&](std::vector<float>& buffer) {
			synthesis_->vtmController->synthesizePhoneticString(
						phoneticString.toStdString().c_str(),
						vtmParamFilePath.toStdString().c_str(),
						buffer);
		});

		ui_->parameterWidget->update();

		emit audioStarted();
		emit playAudioRequested(sampleRate);

		emit textSynthesized();
	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
		enableProcessingButtons();
	}
}

void
SynthesisWindow::on_synthesizeToFileButton_clicked()
{
	if (!synthesis_) {
		return;
	}
	QString phoneticString = ui_->phoneticStringTextEdit->toPlainText();
	if (phoneticString.trimmed().isEmpty()) {
		return;
	}
	QString filePath = QFileDialog::getSaveFileName(this, tr("Save file"), synthesis_->projectDir, tr("WAV files (*.wav)"));
	if (filePath.isEmpty()) {
		return;
	}
	disableProcessingButtons();

	try {
		QString vtmParamFilePath = synthesis_->projectDir + VTM_PARAM_FILE_NAME;

		VTMControlModel::Configuration& config = synthesis_->vtmController->vtmControlModelConfiguration();
		config.tempo = ui_->tempoSpinBox->value();

		synthesis_->vtmController->synthesizePhoneticString(
					phoneticString.toStdString().c_str(),
					vtmParamFilePath.toStdString().c_str(),
					filePath.toStdString().c_str());

		ui_->parameterWidget->update();

		emit textSynthesized();
	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
	}

	enableProcessingButtons();
}

// Slot.
void
SynthesisWindow::synthesizeWithManualIntonation()
{
	if (!synthesis_) {
		return;
	}
	disableProcessingButtons();

	try {
		auto& eventList = synthesis_->vtmController->eventList();
		if (eventList.list().empty()) {
			return;
		}
		eventList.clearMacroIntonation();
		eventList.prepareMacroIntonationInterpolation();

		const ConfigurationData& vtmConfig = synthesis_->vtmController->vtmConfigurationData();
		const double sampleRate = vtmConfig.value<double>("output_rate");

		QString vtmParamFilePath = synthesis_->projectDir + VTM_PARAM_FILE_NAME;

		audioWorker_->player().fillBuffer([&](std::vector<float>& buffer) {
			synthesis_->vtmController->synthesizeFromEventList(
						vtmParamFilePath.toStdString().c_str(),
						buffer);
		});

		emit audioStarted();
		emit playAudioRequested(sampleRate);
	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
		enableProcessingButtons();
	}
}

// Slot.
void
SynthesisWindow::synthesizeToFileWithManualIntonation(QString filePath)
{
	if (!synthesis_) {
		return;
	}
	disableProcessingButtons();

	try {
		auto& eventList = synthesis_->vtmController->eventList();
		if (eventList.list().empty()) {
			return;
		}
		eventList.clearMacroIntonation();
		eventList.prepareMacroIntonationInterpolation();

		QString vtmParamFilePath = synthesis_->projectDir + VTM_PARAM_FILE_NAME;

		synthesis_->vtmController->synthesizeFromEventList(
					vtmParamFilePath.toStdString().c_str(),
					filePath.toStdString().c_str());

	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
	}

	enableProcessingButtons();
}

void
SynthesisWindow::on_parameterTableWidget_cellChanged(int row, int column)
{
	bool selected = ui_->parameterTableWidget->item(row, column)->checkState() == Qt::Checked;
	ui_->parameterWidget->changeParameterSelection(row, selected);
}

void
SynthesisWindow::on_xZoomSpinBox_valueChanged(double d)
{
	ui_->parameterWidget->changeXZoom(d);
}

void
SynthesisWindow::on_yZoomSpinBox_valueChanged(double d)
{
	ui_->parameterWidget->changeYZoom(d);
}

// Slot.
void
SynthesisWindow::setupParameterTable()
{
	if (model_ == nullptr) return;

	auto* table = ui_->parameterTableWidget;
	table->setRowCount(model_->parameterList().size());
	for (unsigned int i = 0; i < model_->parameterList().size(); ++i) {
		auto item = std::make_unique<QTableWidgetItem>(model_->parameterList()[i].name().c_str());
		item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		item->setCheckState(Qt::Checked);
		table->setItem(i, 0, item.release());
	}
	table->resizeColumnsToContents();
}

// Slot.
void
SynthesisWindow::updateMouseTracking(double time, double value)
{
	if (time < -0.1) {
		ui_->timeLineEdit->clear();
		ui_->valueLineEdit->clear();
	} else {
		ui_->timeLineEdit->setText(QString::number(time, 'f', 0));
		ui_->valueLineEdit->setText(QString::number(value, 'f', 2));
	}
}

// Slot.
void
SynthesisWindow::handleAudioError(QString msg)
{
	QMessageBox::critical(this, tr("Error"), msg);

	enableProcessingButtons();

	emit audioFinished();
}

// Slot.
void
SynthesisWindow::handleAudioFinished()
{
	enableProcessingButtons();

	emit audioFinished();
}

void
SynthesisWindow::resetZoom()
{
	ui_->xZoomSpinBox->setValue(1.0);
	ui_->yZoomSpinBox->setValue(1.0);
}

void
SynthesisWindow::enableProcessingButtons()
{
	ui_->parseButton->setEnabled(true);
	ui_->synthesizeButton->setEnabled(true);
	ui_->synthesizeToFileButton->setEnabled(true);
}

void
SynthesisWindow::disableProcessingButtons()
{
	ui_->parseButton->setEnabled(false);
	ui_->synthesizeButton->setEnabled(false);
	ui_->synthesizeToFileButton->setEnabled(false);
}

} // namespace GS
