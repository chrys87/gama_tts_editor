/***************************************************************************
 *  Copyright 2014, 2015 Marcelo Y. Matuda                                 *
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

#ifndef SYNTHESIS_WINDOW_H
#define SYNTHESIS_WINDOW_H

#include <memory>

#include <QString>
#include <QWidget>



namespace Ui {
class SynthesisWindow;
}

namespace GS {

class Synthesis;
namespace TRMControlModel {
class Model;
}

class SynthesisWindow : public QWidget {
	Q_OBJECT
public:
	explicit SynthesisWindow(QWidget* parent=0);
	~SynthesisWindow();

	void clear();
	void setup(TRMControlModel::Model* model, Synthesis* synthesis);
signals:
	void textSynthesized();
private slots:
	void on_parseButton_clicked();
	void on_synthesizeButton_clicked();
	void on_parameterTableWidget_cellChanged(int row, int column);
	void setupParameterTable();
	void updateMouseTracking(double time, double value);
private:
	enum {
		NUM_PARAM = 16
	};

	std::unique_ptr<Ui::SynthesisWindow> ui_;
	TRMControlModel::Model* model_;
	Synthesis* synthesis_;
};

} // namespace GS

#endif // SYNTHESIS_WINDOW_H
