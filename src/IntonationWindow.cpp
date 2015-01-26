/***************************************************************************
 *  Copyright 2015 Marcelo Y. Matuda                                       *
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

#include "IntonationWindow.h"

#include <QMessageBox>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "Controller.h"
#include "Synthesis.h"
#include "ui_IntonationWindow.h"

#define TRM_PARAM_FILE_NAME "generated__intonation_trm_param.txt"
#define SPEECH_FILE_NAME "generated__intonation_speech.wav"



namespace GS {

IntonationWindow::IntonationWindow(QWidget* parent)
		: QWidget(parent)
		, ui_(new Ui::IntonationWindow)
		, synthesis_(nullptr)
{
	ui_->setupUi(this);

	connect(ui_->intonationWidget, SIGNAL(pointSelected(double, double, double, double, double)),
		this, SLOT(setPointData(double, double, double, double, double)));
}

IntonationWindow::~IntonationWindow()
{
}

void
IntonationWindow::clear()
{
	synthesis_ = nullptr;
}

void
IntonationWindow::setup(Synthesis* synthesis)
{
	if (synthesis == nullptr) {
		clear();
		return;
	}

	synthesis_ = synthesis;

	ui_->intonationWidget->updateData(&synthesis_->trmController->eventList());
}

void
IntonationWindow::on_synthesizeButton_clicked()
{
	qDebug("IntonationWindow::on_synthesizeButton_clicked");

	if (synthesis_ == nullptr) return;

	if (!ui_->intonationWidget->saveIntonationToEventList()) {
		return;
	}

	ui_->synthesizeButton->setEnabled(false);

	try {
		auto& eventList = synthesis_->trmController->eventList();
		if (eventList.list().empty()) {
			return;
		}
		eventList.clearMacroIntonation();
		eventList.applyIntonationSmooth();

		QString trmParamFilePath = synthesis_->projectDir + TRM_PARAM_FILE_NAME;
		QString speechFilePath = synthesis_->projectDir + SPEECH_FILE_NAME;

		synthesis_->trmController->synthesizeFromEventList(
					trmParamFilePath.toStdString().c_str(),
					speechFilePath.toStdString().c_str());

		emit playAudioFileRequested(speechFilePath);
	} catch (const Exception& exc) {
		QMessageBox::critical(this, tr("Error"), exc.what());
		ui_->synthesizeButton->setEnabled(true);
	}
}

// Slot.
void
IntonationWindow::loadIntonationFromEventList()
{
	ui_->intonationWidget->loadIntonationFromEventList();
}

// Slot.
void
IntonationWindow::handleAudioStarted()
{
	ui_->synthesizeButton->setEnabled(false);
}

// Slot.
void
IntonationWindow::handleAudioFinished()
{
	ui_->synthesizeButton->setEnabled(true);
}

void
IntonationWindow::on_valueLineEdit_editingFinished()
{
	bool ok;
	double value = ui_->valueLineEdit->text().toDouble(&ok);
	if (!ok) {
		ui_->valueLineEdit->clear();
		return;
	}
	ui_->intonationWidget->setSelectedPointValue(value);
}

void
IntonationWindow::on_slopeLineEdit_editingFinished()
{
	bool ok;
	double slope = ui_->slopeLineEdit->text().toDouble(&ok);
	if (!ok) {
		ui_->slopeLineEdit->clear();
		return;
	}
	ui_->intonationWidget->setSelectedPointSlope(slope);
}

void
IntonationWindow::on_beatOffsetLineEdit_editingFinished()
{
	bool ok;
	double beatOffset = ui_->beatOffsetLineEdit->text().toDouble(&ok);
	if (!ok) {
		ui_->beatOffsetLineEdit->clear();
		return;
	}
	ui_->intonationWidget->setSelectedPointBeatOffset(beatOffset);
}

// Slot.
void
IntonationWindow::setPointData(double value, double slope, double beat, double beatOffset, double absoluteTime)
{
	ui_->valueLineEdit->setText(QString::number(value));
	ui_->slopeLineEdit->setText(QString::number(slope));
	ui_->beatLineEdit->setText(QString::number(beat));
	ui_->beatOffsetLineEdit->setText(QString::number(beatOffset));
	ui_->absoluteTimeLineEdit->setText(QString::number(absoluteTime));
}

} // namespace GS