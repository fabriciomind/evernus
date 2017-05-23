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

#include <QWidget>

#include "InterRegionMarketDataFilterProxyModel.h"
#include "InterRegionMarketDataModel.h"
#include "ExternalOrder.h"
#include "TaskConstants.h"
#include "PriceType.h"

class QAbstractItemView;
class QStackedWidget;
class QPushButton;
class QTableView;
class QComboBox;
class QCheckBox;
class QLineEdit;

namespace Evernus
{
    class AdjustableTableView;
    class MarketDataProvider;
    class EveDataProvider;

    class InterRegionAnalysisWidget
        : public QWidget
    {
        Q_OBJECT

    public:
        InterRegionAnalysisWidget(const QByteArray &clientId,
                                  const QByteArray &clientSecret,
                                  const EveDataProvider &dataProvider,
                                  const MarketDataProvider &marketDataProvider,
                                  QWidget *parent = nullptr);
        virtual ~InterRegionAnalysisWidget() = default;

        void setPriceTypes(PriceType src, PriceType dst) noexcept;

        void setBogusOrderThreshold(double value) noexcept;
        void discardBogusOrders(bool flag) noexcept;

        void setCharacter(const std::shared_ptr<Character> &character);

        void recalculateAllData();
        void completeImport();
        void clearData();

    signals:
        void showInEve(EveType::IdType id, Character::IdType ownerId);

    private slots:
        void applyInterRegionFilter();

        void selectInterRegionType(const QItemSelection &selected);

        void showInEveForCurrentInterRegion();

        void copyRows(const QAbstractItemView &view, const QAbstractItemModel &model) const;

    private:
        static const auto waitingLabelIndex = 0;
        static const auto allRegionsIndex = 0;

        const EveDataProvider &mDataProvider;
        const MarketDataProvider &mMarketDataProvider;

        QAction *mShowInEveInterRegionAct = nullptr;
        QAction *mCopyInterRegionRowsAct = nullptr;

        QComboBox *mSourceRegionCombo = nullptr;
        QComboBox *mDestRegionCombo = nullptr;
        QLineEdit *mMinInterRegionVolumeEdit = nullptr;
        QLineEdit *mMaxInterRegionVolumeEdit = nullptr;
        QLineEdit *mMinInterRegionMarginEdit = nullptr;
        QLineEdit *mMaxInterRegionMarginEdit = nullptr;
        QStackedWidget *mInterRegionDataStack = nullptr;
        AdjustableTableView *mInterRegionTypeDataView = nullptr;

        InterRegionMarketDataModel mInterRegionDataModel;
        InterRegionMarketDataFilterProxyModel mInterRegionViewProxy;
        bool mRefreshedInterRegionData = false;

        quint64 mSrcStation = 0;
        quint64 mDstStation = 0;

        PriceType mSrcPriceType = PriceType::Buy;
        PriceType mDstPriceType = PriceType::Buy;

        void changeStation(quint64 &destination, QPushButton &btn, const QString &settingName);
        void recalculateInterRegionData();
    };
}