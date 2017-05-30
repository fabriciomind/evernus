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

#include <memory>

#include <QSortFilterProxyModel>

#include "OreReprocessingArbitrageModel.h"
#include "StandardModelProxyWidget.h"
#include "PriceType.h"

class QStackedWidget;
class QDoubleSpinBox;
class QCheckBox;
class QAction;

namespace Evernus
{
    class AdjustableTableView;
    class MarketDataProvider;
    class EveDataProvider;
    class Character;

    class OreReprocessingArbitrageWidget
        : public StandardModelProxyWidget
    {
        Q_OBJECT

    public:
        explicit OreReprocessingArbitrageWidget(const EveDataProvider &dataProvider,
                                                const MarketDataProvider &marketDataProvider,
                                                QWidget *parent = nullptr);
        OreReprocessingArbitrageWidget(const OreReprocessingArbitrageWidget &) = default;
        OreReprocessingArbitrageWidget(OreReprocessingArbitrageWidget &&) = default;
        virtual ~OreReprocessingArbitrageWidget() = default;

        void setCharacter(std::shared_ptr<Character> character);
        void clearData();

        void setPriceTypes(PriceType src, PriceType dst) noexcept;

        OreReprocessingArbitrageWidget &operator =(const OreReprocessingArbitrageWidget &) = default;
        OreReprocessingArbitrageWidget &operator =(OreReprocessingArbitrageWidget &&) = default;

    public slots:
        void recalculateData();

    private:
        static const auto waitingLabelIndex = 0;

        const MarketDataProvider &mMarketDataProvider;

        QDoubleSpinBox *mStationEfficiencyEdit = nullptr;
        QCheckBox *mIncludeStationTaxBtn = nullptr;
        QCheckBox *mIgnoreMinVolumeBtn = nullptr;
        QStackedWidget *mDataStack = nullptr;
        AdjustableTableView *mDataView = nullptr;

        OreReprocessingArbitrageModel mDataModel;
        QSortFilterProxyModel mDataProxy;

        PriceType mSrcPriceType = PriceType::Buy;
        PriceType mDstPriceType = PriceType::Buy;
    };
}
