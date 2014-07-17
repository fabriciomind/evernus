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
#include <QPainter>

#include "StyledTreeViewItemDelegate.h"

namespace Evernus
{
    void StyledTreeViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QStyledItemDelegate::paint(painter, option, index);

        painter->setPen(QPen{Qt::lightGray, 0., Qt::DotLine});
        painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
        painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    }
}
