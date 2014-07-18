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

#include <QSortFilterProxyModel>
#include <QWidget>

class QItemSelectionModel;
class QLabel;

namespace Evernus
{
    class MarketOrderModel;
    class StyledTreeView;

    class MarketOrderView
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit MarketOrderView(QWidget *parent = nullptr);
        virtual ~MarketOrderView() = default;

        QItemSelectionModel *getSelectionModel() const;
        const QAbstractProxyModel &getProxyModel() const;

        void setModel(MarketOrderModel *model);

    signals:
        void closeOrderInfo();

    public slots:
        void updateInfo();

    private slots:
        void showPriceInfo(const QModelIndex &index);

    private:
        StyledTreeView *mView = nullptr;

        QLabel *mTotalOrdersLabel = nullptr;
        QLabel *mVolumeLabel = nullptr;
        QLabel *mTotalISKLabel = nullptr;
        QLabel *mTotalSizeLabel = nullptr;

        MarketOrderModel *mSource = nullptr;
        QSortFilterProxyModel mProxy;
    };
}