/**
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QComboBox>

namespace Evernus
{
    class FilterTextRepository;

    class TextFilterWidget
        : public QComboBox
    {
        Q_OBJECT

    public:
        explicit TextFilterWidget(const FilterTextRepository &filterRepo, QWidget *parent = nullptr);
        virtual ~TextFilterWidget() = default;

        static QString getDefaultPlaceholderText();

    signals:
        void filterEntered(const QString &text);

    private slots:
        void applyCurrentFilter();
        void refreshHistory();

        void showItemContextMenu(const QPoint &pos);

    private:
        const FilterTextRepository &mFilterRepo;
    };
}
