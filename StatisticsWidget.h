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

#include "Character.h"

class QCPBars;

namespace Evernus
{
    class MarketOrderValueSnapshotRepository;
    class WalletJournalEntryRepository;
    class AssetValueSnapshotRepository;
    class WalletTransactionRepository;
    class WalletSnapshotRepository;
    class DateFilteredPlotWidget;

    class StatisticsWidget
        : public QWidget
    {
        Q_OBJECT

    public:
        StatisticsWidget(const AssetValueSnapshotRepository &assetSnapshotRepo,
                         const WalletSnapshotRepository &walletSnapshotRepo,
                         const MarketOrderValueSnapshotRepository &marketOrderSnapshotRepo,
                         const WalletJournalEntryRepository &journalRepo,
                         const WalletTransactionRepository &transactionRepo,
                         QWidget *parent = nullptr);
        virtual ~StatisticsWidget() = default;

    public slots:
        void setCharacter(Character::IdType id);
        void updateBalanceData();
        void updateJournalData();
        void updateTransactionData();

    private:
        static constexpr auto assetValueGraph = 0;
        static constexpr auto walletBalanceGraph = 1;
        static constexpr auto buyOrdersGraph = 2;
        static constexpr auto sellOrdersGraph = 3;
        static constexpr auto totalValueGraph = 4;

        const AssetValueSnapshotRepository &mAssetSnapshotRepository;
        const WalletSnapshotRepository &mWalletSnapshotRepository;
        const MarketOrderValueSnapshotRepository &mMarketOrderSnapshotRepository;
        const WalletJournalEntryRepository &mJournalRepo;
        const WalletTransactionRepository &mTransactionRepo;

        DateFilteredPlotWidget *mBalancePlot = nullptr;
        DateFilteredPlotWidget *mJournalPlot = nullptr;
        DateFilteredPlotWidget *mTransactionPlot = nullptr;

        QCPBars *mIncomingPlot = nullptr;
        QCPBars *mOutgoingPlot = nullptr;

        QCPBars *mBuyPlot = nullptr;
        QCPBars *mSellPlot = nullptr;

        Character::IdType mCharacterId = Character::invalidId;

        void updateGraphAndLegend();

        DateFilteredPlotWidget *createPlot();

        static QCPBars *createBarPlot(DateFilteredPlotWidget *plot, const QString &name, Qt::GlobalColor color);
    };
}
