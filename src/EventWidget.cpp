/***************************************************************************
 *  Copyright 2014, 2015 Marcelo Y. Matuda                                 *
 *  Copyright 1991, 1992, 1993, 1994, 1995, 1996, 2001, 2002               *
 *    David R. Hill, Leonard Manzara, Craig Schock                         *
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
// 2015-01
// This file was created by Marcelo Y. Matuda, and code/information
// from Gnuspeech was added to it later.

#include "EventWidget.h"

#include <cmath>

#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSizePolicy>

#include "EventList.h"
#include "Model.h"

#define MARGIN 10.0
#define TRACK_HEIGHT 20.0
#define GRAPH_HEIGHT 120.0
#define SPECIAL_STRING "(special)"
#define MININUM_WIDTH 1024
#define MININUM_HEIGHT 768
#define TEXT_MARGIN 5.0



namespace GS {

EventWidget::EventWidget(QWidget* parent)
		: QWidget(parent)
		, eventList_(nullptr)
		, model_(nullptr)
		, timeScale_(0.7)
		, modelUpdated_(false)
		, textAscent_(0.0)
		, textYOffset_(0.0)
		, labelWidth_(0.0)
		, totalWidth_(MININUM_WIDTH)
		, totalHeight_(MININUM_HEIGHT)
{
	//setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	setMinimumWidth(totalWidth_);
	setMinimumHeight(totalHeight_);

	QPalette pal;
	pal.setColor(QPalette::Window, Qt::white);
	setPalette(pal);
	setAutoFillBackground(true);
}

// Note: with no antialiasing, the coordinates in QPointF are rounded to the nearest integer.
void
EventWidget::paintEvent(QPaintEvent*)
{
	if (eventList_ == nullptr || eventList_->list().empty()) {
		return;
	}

	QPainter p(this);

	if (modelUpdated_) {
		QFontMetrics fm = p.fontMetrics();
		textAscent_ = fm.ascent();
		textYOffset_ = 0.5 * textAscent_;

		int maxWidth = 0;
		for (unsigned int i = 0; i < model_->parameterList().size(); ++i) {
			int width = fm.width(model_->parameterList()[i].name().c_str());
			if (width > maxWidth) {
				maxWidth = width;
			}
		}
		int width = fm.width(tr(SPECIAL_STRING));
		if (width > maxWidth) {
			maxWidth = width;
		}
		labelWidth_ = maxWidth;

		modelUpdated_ = false;
	}

	double xEnd, yEnd;
	if (selectedParamList_.empty()) {
		xEnd = 1024.0;
		yEnd = 768.0;
	} else {
		xEnd = 2.0 * MARGIN + labelWidth_ + eventList_->list().back()->time * timeScale_;
		yEnd = 2.0 * TRACK_HEIGHT + (MARGIN + GRAPH_HEIGHT) * selectedParamList_.size();
	}
	totalWidth_ = std::ceil(xEnd + 3.0 * MARGIN);
	if (totalWidth_ < MININUM_WIDTH) {
		totalWidth_ = MININUM_WIDTH;
	}
	totalHeight_ = std::ceil(yEnd + MARGIN);
	if (totalHeight_ < MININUM_HEIGHT) {
		totalHeight_ = MININUM_HEIGHT;
	}
	setMinimumWidth(totalWidth_);
	setMinimumHeight(totalHeight_);

	if (!selectedParamList_.empty()) {

		postureTimeList_.clear();

		double y = MARGIN + 2.0 * TRACK_HEIGHT - 0.5 * (TRACK_HEIGHT - 1.0) + textYOffset_;
		unsigned int postureIndex = 0;
		for (const TRMControlModel::Event_ptr& ev : eventList_->list()) {
			double x = 2.0 * MARGIN + labelWidth_ + ev->time * timeScale_;
			if (ev->flag) {
				postureTimeList_.push_back(ev->time);
				p.setPen(Qt::black);
				const TRMControlModel::Posture* posture = eventList_->getPostureAtIndex(postureIndex++);
				if (posture) {
					// Posture name.
					p.drawText(QPointF(x, y), posture->name().c_str());
				}
				// Event vertical line.
				p.drawLine(QPointF(x, MARGIN + 2.0 * TRACK_HEIGHT), QPointF(x, yEnd));
			} else {
				p.setPen(Qt::lightGray);
				// Event vertical line.
				p.drawLine(QPointF(x, MARGIN + 2.0 * TRACK_HEIGHT), QPointF(x, yEnd));
			}
		}
		p.setPen(Qt::black);

		double xRule = 2.0 * MARGIN + labelWidth_;
		double yRuleText = MARGIN + TRACK_HEIGHT - 0.5 * (TRACK_HEIGHT - 1.0) + textYOffset_;
		p.drawText(QPointF(MARGIN, yRuleText), tr("Rule"));
		for (int i = 0; i < eventList_->numberOfRules(); ++i) {
			auto* ruleData = eventList_->getRuleAtIndex(i);
			if (ruleData) {
#if 0
				double ruleDur = ruleData->duration * timeScale_;
				// Rule frame.
				p.drawRect(QRectF(QPointF(xRule, MARGIN), QPointF(xRule + ruleDur, MARGIN + TRACK_HEIGHT)));
				// Rule number.
				p.drawText(QPointF(xRule + TEXT_MARGIN, yRuleText), QString("%1").arg(ruleData->number));

				xRule += ruleDur;
#else
				unsigned int firstPosture = ruleData->firstPosture;
				unsigned int lastPosture = ruleData->lastPosture;

				int postureTime1, postureTime2;
				if (firstPosture < postureTimeList_.size()) {
					postureTime1 = postureTimeList_[firstPosture];
				} else {
					postureTime1 = 0; // invalid
				}
				if (lastPosture < postureTimeList_.size()) {
					postureTime2 = postureTimeList_[lastPosture];
				} else {
					postureTime2 = postureTime1 + ruleData->duration;
				}

				// Rule frame.
				p.drawRect(QRectF(
						QPointF(xRule + postureTime1 * timeScale_, MARGIN),
						QPointF(xRule + postureTime2 * timeScale_, MARGIN + TRACK_HEIGHT)));
				// Rule number.
				p.drawText(QPointF(xRule + postureTime1 * timeScale_ + TEXT_MARGIN, yRuleText), QString("%1").arg(ruleData->number));
#endif
			}
		}
	}

	QPen pen;
	QPen pen2;
	pen2.setWidth(2);

	for (unsigned int i = 0; i < selectedParamList_.size(); ++i) {
		unsigned int paramIndex = selectedParamList_[i];
		unsigned int modelParamIndex;
		if (paramIndex >= NUM_PARAM) { // special
			modelParamIndex = paramIndex - NUM_PARAM;
		} else {
			modelParamIndex = paramIndex;
		}
		double currentMin = model_->parameterList()[modelParamIndex].minimum();
		double currentMax = model_->parameterList()[modelParamIndex].maximum();

		double yBase = 2.0 * TRACK_HEIGHT + (MARGIN + GRAPH_HEIGHT) * (i + 1U);

		// Label.
		p.drawText(QPointF(MARGIN, yBase - 0.5 * GRAPH_HEIGHT + textYOffset_), model_->parameterList()[modelParamIndex].name().c_str());
		if (paramIndex >= NUM_PARAM) {
			p.drawText(QPointF(MARGIN, yBase - 0.5 * GRAPH_HEIGHT + textYOffset_ + TRACK_HEIGHT), tr(SPECIAL_STRING));
		}
		// Limits.
		p.drawText(QPointF(MARGIN, yBase), QString("%1").arg(currentMin));
		p.drawText(QPointF(MARGIN, yBase - GRAPH_HEIGHT + textAscent_), QString("%1").arg(currentMax));

		// Graph frame.
		p.drawLine(QPointF(2.0 * MARGIN + labelWidth_, yBase - GRAPH_HEIGHT), QPointF(xEnd, yBase - GRAPH_HEIGHT));
		p.drawLine(QPointF(2.0 * MARGIN + labelWidth_, yBase)               , QPointF(xEnd, yBase));
		p.drawLine(QPointF(2.0 * MARGIN + labelWidth_, yBase - GRAPH_HEIGHT), QPointF(2.0 * MARGIN + labelWidth_, yBase));
		p.drawLine(QPointF(xEnd, yBase - GRAPH_HEIGHT)                      , QPointF(xEnd, yBase));

		// Graph curve.
		p.setPen(pen2);
		p.setRenderHint(QPainter::Antialiasing);
		QPointF prevPoint;
		const double valueFactor = 1.0 / (currentMax - currentMin);
		for (const TRMControlModel::Event_ptr& ev : eventList_->list()) {
			double x = 0.5 + 2.0 * MARGIN + labelWidth_ + ev->time * timeScale_; // 0.5 added because of antialiasing
			double value = ev->getValue(paramIndex);
			if (value != GS_EVENTLIST_INVALID_EVENT_VALUE) {
				double y = 0.5 + yBase - (value - currentMin) * valueFactor * GRAPH_HEIGHT; // 0.5 added because of antialiasing
				if (!prevPoint.isNull()) {
					p.drawLine(prevPoint, QPointF(x, y));
				}
				prevPoint.setX(x);
				prevPoint.setY(y);
			}
		}
		p.setRenderHint(QPainter::Antialiasing, false);
		p.setPen(pen);
	}
}

QSize
EventWidget::sizeHint() const
{
	return QSize(totalWidth_, totalHeight_);
}

void
EventWidget::updateData(TRMControlModel::EventList* eventList, TRMControlModel::Model* model)
{
	eventList_ = eventList;
	model_ = model;

	if (model_ && model_->parameterList().size() != NUM_PARAM) {
		qWarning("[EventWidget::updateData] Wrong number of parameters: %lu (should be %d).", model_->parameterList().size(), NUM_PARAM);
		model_ = nullptr;
		eventList_ = nullptr;
		return;
	}

	modelUpdated_ = true;

	update();
}

void
EventWidget::clearParameterSelection()
{
	selectedParamList_.clear();
	update();
}

void
EventWidget::changeParameterSelection(unsigned int paramIndex, bool special, bool selected)
{
	if (special) {
		paramIndex += NUM_PARAM;
	}

	if (selected) {
		selectedParamList_.push_back(paramIndex);
	} else {
		for (auto iter = selectedParamList_.begin(); iter != selectedParamList_.end(); ++iter) {
			if (*iter == paramIndex) {
				selectedParamList_.erase(iter);
				break;
			}
		}
	}
	update();
}

} // namespace GS
