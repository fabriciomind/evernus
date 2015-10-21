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
#include <unordered_map>
#include <memory>

#include <QApplication>
#include <QRadioButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QClipboard>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QAction>
#include <QDebug>
#include <QHash>
#include <QFont>

#include "CorpMarketOrderValueSnapshotRepository.h"
#include "MarketOrderValueSnapshotRepository.h"
#include "WalletJournalEntryRepository.h"
#include "AssetValueSnapshotRepository.h"
#include "CorpWalletSnapshotRepository.h"
#include "WalletTransactionRepository.h"
#include "QtScriptSyntaxHighlighter.h"
#include "WalletSnapshotRepository.h"
#include "DateFilteredPlotWidget.h"
#include "OrderScriptRepository.h"
#include "NumberFormatDelegate.h"
#include "CharacterRepository.h"
#include "StatisticsSettings.h"
#include "UISettings.h"
#include "TextUtils.h"

#include "qcustomplot.h"

#include "StatisticsWidget.h"

namespace Evernus
{
    StatisticsWidget::StatisticsWidget(const AssetValueSnapshotRepository &assetSnapshotRepo,
                                       const WalletSnapshotRepository &walletSnapshotRepo,
                                       const CorpWalletSnapshotRepository &corpWalletSnapshotRepo,
                                       const MarketOrderValueSnapshotRepository &marketOrderSnapshotRepo,
                                       const CorpMarketOrderValueSnapshotRepository &corpMarketOrderSnapshotRepo,
                                       const WalletJournalEntryRepository &journalRepo,
                                       const WalletTransactionRepository &transactionRepo,
                                       const WalletJournalEntryRepository &corpJournalRepo,
                                       const WalletTransactionRepository &corpTransactionRepo,
                                       const MarketOrderRepository &orderRepo,
                                       const OrderScriptRepository &orderScriptRepo,
                                       const CharacterRepository &characterRepo,
                                       const EveDataProvider &dataProvider,
                                       QWidget *parent)
        : QWidget(parent)
        , mAssetSnapshotRepository(assetSnapshotRepo)
        , mWalletSnapshotRepository(walletSnapshotRepo)
        , mCorpWalletSnapshotRepository(corpWalletSnapshotRepo)
        , mMarketOrderSnapshotRepository(marketOrderSnapshotRepo)
        , mCorpMarketOrderSnapshotRepository(corpMarketOrderSnapshotRepo)
        , mJournalRepository(journalRepo)
        , mCorpJournalRepository(corpJournalRepo)
        , mTransactionRepository(transactionRepo)
        , mCorpTransactionRepository(corpTransactionRepo)
        , mMarketOrderRepository(orderRepo)
        , mOrderScriptRepository(orderScriptRepo)
        , mCharacterRepository(characterRepo)
        , mAggrModel(mMarketOrderRepository, dataProvider)
        , mScriptModel(dataProvider)
    {
        auto mainLayout = new QVBoxLayout{this};

        auto tabs = new QTabWidget{this};
        mainLayout->addWidget(tabs);

        tabs->addTab(createBasicStatisticsWidget(), tr("Basic"));
        tabs->addTab(createAdvancedStatisticsWidget(), tr("Advanced"));

        updateGraphAndLegend();

        connect(&mScriptModel, &ScriptOrderProcessingModel::error, this, &StatisticsWidget::showScriptError);
    }

    void StatisticsWidget::setCharacter(Character::IdType id)
    {
        qDebug() << "Switching statistics to" << id;

        mCharacterId = id;

        if (mCharacterId == Character::invalidId)
        {
            mAggrApplyBtn->setDisabled(true);
            mScriptApplyBtn->setDisabled(true);
            mBalancePlot->getPlot().clearPlottables();
            mJournalPlot->getPlot().clearPlottables();
            updateGraphAndLegend();
        }
        else
        {
            mBalancePlot->blockSignals(true);
            mJournalPlot->blockSignals(true);
            mTransactionPlot->blockSignals(true);

            mAggrApplyBtn->setDisabled(false);
            mScriptApplyBtn->setDisabled(false);

            const auto date = QDate::currentDate();

            mBalancePlot->setFrom(date.addDays(-7));
            mBalancePlot->setTo(date);
            mJournalPlot->setFrom(date.addDays(-7));
            mJournalPlot->setTo(date);
            mTransactionPlot->setFrom(date.addDays(-7));
            mTransactionPlot->setTo(date);

            updateData();

            mTransactionPlot->blockSignals(false);
            mJournalPlot->blockSignals(false);
            mBalancePlot->blockSignals(false);
        }

        mAggrModel.clear();
        mScriptModel.clear();
    }

    void StatisticsWidget::updateBalanceData()
    {
        const QDateTime from{mBalancePlot->getFrom()};
        const QDateTime to{mBalancePlot->getTo().addDays(1)};

        const auto combineStats = mCombineStatsBtn->isChecked();
        const auto assetShots = (combineStats) ?
                                (mAssetSnapshotRepository.fetchRange(from.toUTC(), to.toUTC())) :
                                (mAssetSnapshotRepository.fetchRange(mCharacterId, from.toUTC(), to.toUTC()));
        const auto walletShots = (combineStats) ?
                                 (mWalletSnapshotRepository.fetchRange(from.toUTC(), to.toUTC())) :
                                 (mWalletSnapshotRepository.fetchRange(mCharacterId, from.toUTC(), to.toUTC()));
        const auto orderShots = (combineStats) ?
                                (mMarketOrderSnapshotRepository.fetchRange(from.toUTC(), to.toUTC())) :
                                (mMarketOrderSnapshotRepository.fetchRange(mCharacterId, from.toUTC(), to.toUTC()));

        auto assetGraph = mBalancePlot->getPlot().graph(assetValueGraph);
        auto walletGraph = mBalancePlot->getPlot().graph(walletBalanceGraph);
        auto corpWalletGraph = mBalancePlot->getPlot().graph(corpWalletBalanceGraph);
        auto buyGraph = mBalancePlot->getPlot().graph(buyOrdersGraph);
        auto sellGraph = mBalancePlot->getPlot().graph(sellOrdersGraph);
        auto sumGraph = mBalancePlot->getPlot().graph(totalValueGraph);

        auto assetValues = std::make_unique<QCPDataMap>();
        auto walletValues = std::make_unique<QCPDataMap>();
        auto corpWalletValues = std::make_unique<QCPDataMap>();
        auto buyValues = std::make_unique<QCPDataMap>();
        auto sellValues = std::make_unique<QCPDataMap>();;

        QCPDataMap buyAndSellValues;

        auto yMax = -1.;

        const auto convertTimestamp = [](const auto &shot) {
            return shot->getTimestamp().toMSecsSinceEpoch() / 1000.;
        };

        const auto dataInserter = [&convertTimestamp](auto &values, const auto &range) {
            for (const auto &shot : range)
            {
                const auto secs = convertTimestamp(shot);
                values.insert(secs, QCPData{secs, shot->getBalance()});
            }
        };

        auto sumData = std::make_unique<QCPDataMap>();
        const auto merger = [&yMax](const auto &values1, const auto &values2) -> QCPDataMap {
            QCPDataMap result;

            auto value1It = std::begin(values1);
            auto value2It = std::begin(values2);
            auto prevValue1 = 0.;
            auto prevValue2 = 0.;

            const auto lerp = [](double a, double b, double t) {
                return a + (b - a) * t;
            };

            while (value1It != std::end(values1) || value2It != std::end(values2))
            {
                if (value1It == std::end(values1))
                {
                    const auto value = prevValue1 + value2It->value;
                    if (value > yMax)
                        yMax = value;

                    result.insert(value2It->key, QCPData{value2It->key, value});

                    ++value2It;
                }
                else if (value2It == std::end(values2))
                {
                    const auto value = prevValue2 + value1It->value;
                    if (value > yMax)
                        yMax = value;

                    result.insert(value1It->key, QCPData{value1It->key, value});

                    ++value1It;
                }
                else
                {
                    if (value1It->key < value2It->key)
                    {
                        const auto prevTick = (value2It == std::begin(values2)) ? (values1.firstKey()) : (std::prev(value2It)->key);
                        const auto div = value2It->key - prevTick;
                        while (value1It->key < value2It->key && value1It != std::end(values1))
                        {
                            prevValue1 = value1It->value;

                            const auto value = lerp(prevValue2, value2It->value, (value1It->key - prevTick) / div) + prevValue1;
                            if (value > yMax)
                                yMax = value;

                            result.insert(value1It->key, QCPData{value1It->key, value});

                            ++value1It;
                        }
                    }
                    else if (value1It->key > value2It->key)
                    {
                        const auto prevTick = (value1It == std::begin(values1)) ? (values2.firstKey()) : (std::prev(value1It)->key);
                        const auto div = value1It->key - prevTick;
                        while (value1It->key > value2It->key && value2It != std::end(values2))
                        {
                            prevValue2 = value2It->value;

                            const auto value = prevValue2 + lerp(prevValue1, value1It->value, (value2It->key - prevTick) / div);
                            if (value > yMax)
                                yMax = value;

                            result.insert(value2It->key, QCPData{value2It->key, value});

                            ++value2It;
                        }
                    }
                    else
                    {
                        prevValue2 = value2It->value;
                        prevValue1 = value1It->value;

                        const auto value = prevValue2 + prevValue1;
                        if (value > yMax)
                            yMax = value;

                        result.insert(value1It->key, QCPData{value1It->key, value});

                        ++value1It;
                        ++value2It;
                    }
                }
            }

            return result;
        };

        const auto combineMaps = [&merger](auto &target, const auto &characterValuesMap) {
            if (characterValuesMap.empty())
                return;

            target = std::begin(characterValuesMap)->second;

            for (auto it = std::next(std::begin(characterValuesMap)); it != std::end(characterValuesMap); ++it)
                target = merger(target, it->second);
        };

        const auto combineShots = [&convertTimestamp, &combineMaps](auto &target, const auto &snapshots) {
            std::unordered_map<Evernus::Character::IdType, QCPDataMap> map;
            for (const auto &snapshot : snapshots)
            {
                const auto secs = convertTimestamp(snapshot);
                map[snapshot->getCharacterId()].insert(secs, QCPData{secs, snapshot->getBalance()});
            }

            combineMaps(target, map);
        };

        if (combineStats)
        {
            combineShots(*assetValues, assetShots);
            combineShots(*walletValues, walletShots);

            std::unordered_map<Character::IdType, QCPDataMap> buyMap, sellMap;
            for (const auto &order : orderShots)
            {
                const auto secs = convertTimestamp(order);
                buyMap[order->getCharacterId()].insert(secs, QCPData{secs, order->getBuyValue()});
                sellMap[order->getCharacterId()].insert(secs, QCPData{secs, order->getSellValue()});
            }

            combineMaps(*buyValues, buyMap);
            combineMaps(*sellValues, sellMap);

            for (auto bIt = std::begin(*buyValues), sIt = std::begin(*sellValues); bIt != std::end(*buyValues); ++bIt, ++sIt)
            {
                Q_ASSERT(bIt.key() == sIt.key());
                buyAndSellValues.insert(bIt.key(),
                                        QCPData{bIt.key(), bIt.value().value + sIt.value().value});
            }
        }
        else
        {
            dataInserter(*assetValues, assetShots);
            dataInserter(*walletValues, walletShots);

            for (const auto &order : orderShots)
            {
                const auto secs = convertTimestamp(order);

                buyValues->insert(secs, QCPData{secs, order->getBuyValue()});
                sellValues->insert(secs, QCPData{secs, order->getSellValue()});

                buyAndSellValues.insert(secs, QCPData{secs, order->getBuyValue() + order->getSellValue()});
            }
        }

        *sumData = merger(*assetValues, *walletValues);

        try
        {
            const auto corpId = mCharacterRepository.getCorporationId(mCharacterId);
            const auto walletShots = (combineStats) ?
                                     (mCorpWalletSnapshotRepository.fetchRange(from.toUTC(), to.toUTC())) :
                                     (mCorpWalletSnapshotRepository.fetchRange(corpId, from.toUTC(), to.toUTC()));

            if (!walletShots.empty())
            {
                dataInserter(*corpWalletValues, walletShots);
                *sumData = merger(*sumData, *corpWalletValues);
            }

            const auto corpOrderShots = mCorpMarketOrderSnapshotRepository.fetchRange(corpId, from.toUTC(), to.toUTC());
            if (!corpOrderShots.empty())
            {
                QCPDataMap corpBuyValues, corpSellValues, corpBuyAndSellValues;
                for (const auto &order : corpOrderShots)
                {
                    const auto secs = order->getTimestamp().toMSecsSinceEpoch() / 1000.;

                    corpBuyValues.insert(secs, QCPData{secs, order->getBuyValue()});
                    corpSellValues.insert(secs, QCPData{secs, order->getSellValue()});

                    corpBuyAndSellValues.insert(secs, QCPData{secs, order->getBuyValue() + order->getSellValue()});
                }

                *buyValues = merger(*buyValues, corpBuyValues);
                *sellValues = merger(*sellValues, corpSellValues);
                buyAndSellValues = merger(buyAndSellValues, corpBuyAndSellValues);
            }
        }
        catch (const CharacterRepository::NotFoundException &)
        {
        }

        *sumData = merger(*sumData, buyAndSellValues);

        QVector<double> sumTicks;
        for (const auto &entry : *sumData)
            sumTicks << entry.key;

        mBalancePlot->getPlot().xAxis->setTickVector(sumTicks);

        assetGraph->setData(assetValues.get(), false);
        assetValues.release();

        walletGraph->setData(walletValues.get(), false);
        walletValues.release();

        corpWalletGraph->setData(corpWalletValues.get(), false);
        corpWalletValues.release();

        buyGraph->setData(buyValues.get(), false);
        buyValues.release();

        sellGraph->setData(sellValues.get(), false);
        sellValues.release();

        sumGraph->setData(sumData.get(), false);
        sumData.release();

        mBalancePlot->getPlot().xAxis->rescale();
        mBalancePlot->getPlot().yAxis->setRange(0., yMax);
        mBalancePlot->getPlot().replot();
    }

    void StatisticsWidget::updateJournalData()
    {
        const auto combineStats = mCombineStatsBtn->isChecked();
        const auto entries = (combineStats) ?
                             (mJournalRepository.fetchInRange(QDateTime{mJournalPlot->getFrom()},
                                                              QDateTime{mJournalPlot->getTo().addDays(1)},
                                                              WalletJournalEntryRepository::EntryType::All)) :
                             (mJournalRepository.fetchForCharacterInRange(mCharacterId,
                                                                          QDateTime{mJournalPlot->getFrom()},
                                                                          QDateTime{mJournalPlot->getTo().addDays(1)},
                                                                          WalletJournalEntryRepository::EntryType::All));

        auto totalIncome = 0., totalOutcome = 0.;

        QHash<QDate, std::pair<double, double>> values;
        const auto valueAdder = [&values, &totalIncome, &totalOutcome](const auto &entries) {
            for (const auto &entry : entries)
            {
                if (entry->isIgnored())
                    continue;

                auto &value = values[entry->getTimestamp().toLocalTime().date()];

                const auto amount = entry->getAmount();
                if (amount < 0.)
                {
                    totalOutcome -= amount;
                    value.first -= amount;
                }
                else
                {
                    totalIncome += amount;
                    value.second += amount;
                }
            }
        };

        valueAdder(entries);

        QSettings settings;
        if (settings.value(StatisticsSettings::combineCorpAndCharPlotsKey, StatisticsSettings::combineCorpAndCharPlotsDefault).toBool())
        {
            try
            {
                const auto corpId = mCharacterRepository.getCorporationId(mCharacterId);
                const auto entries = (combineStats) ?
                                     (mCorpJournalRepository.fetchInRange(QDateTime{mJournalPlot->getFrom()},
                                                                          QDateTime{mJournalPlot->getTo().addDays(1)},
                                                                          WalletJournalEntryRepository::EntryType::All)) :
                                     (mCorpJournalRepository.fetchForCorporationInRange(corpId,
                                                                                        QDateTime{mJournalPlot->getFrom()},
                                                                                        QDateTime{mJournalPlot->getTo().addDays(1)},
                                                                                        WalletJournalEntryRepository::EntryType::All));

                valueAdder(entries);
            }
            catch (const CharacterRepository::NotFoundException &)
            {
            }
        }

        QVector<double> ticks, incomingTicks, outgoingTicks, incomingValues, outgoingValues;
        createBarTicks(ticks, incomingTicks, outgoingTicks, incomingValues, outgoingValues, values);

        mIncomingPlot->setData(incomingTicks, incomingValues);
        mOutgoingPlot->setData(outgoingTicks, outgoingValues);

        mJournalPlot->getPlot().xAxis->setTickVector(ticks);

        mJournalPlot->getPlot().rescaleAxes();
        mJournalPlot->getPlot().replot();

        const auto loc = locale();

        mJournalIncomeLabel->setText(TextUtils::currencyToString(totalIncome, loc));
        mJournalOutcomeLabel->setText(TextUtils::currencyToString(totalOutcome, loc));
        mJournalBalanceLabel->setText(TextUtils::currencyToString(totalIncome - totalOutcome, loc));
    }

    void StatisticsWidget::updateTransactionData()
    {
        const auto combineStats = mCombineStatsBtn->isChecked();
        const auto entries = (combineStats) ?
                             (mTransactionRepository.fetchInRange(QDateTime{mTransactionPlot->getFrom()},
                                                                  QDateTime{mTransactionPlot->getTo().addDays(1)},
                                                                  WalletTransactionRepository::EntryType::All)) :
                             (mTransactionRepository.fetchForCharacterInRange(mCharacterId,
                                                                              QDateTime{mTransactionPlot->getFrom()},
                                                                              QDateTime{mTransactionPlot->getTo().addDays(1)},
                                                                              WalletTransactionRepository::EntryType::All));

        auto totalIncome = 0., totalOutcome = 0.;

        QHash<QDate, std::pair<double, double>> values;
        const auto valueAdder = [&values, &totalIncome, &totalOutcome](const auto &entries) {
            for (const auto &entry : entries)
            {
                if (entry->isIgnored())
                    continue;

                auto &value = values[entry->getTimestamp().toLocalTime().date()];

                const auto amount = entry->getPrice();
                if (entry->getType() == Evernus::WalletTransaction::Type::Buy)
                {
                    totalOutcome += amount;
                    value.first += amount;
                }
                else
                {
                    totalIncome += amount;
                    value.second += amount;
                }
            }
        };

        valueAdder(entries);

        QSettings settings;
        if (settings.value(StatisticsSettings::combineCorpAndCharPlotsKey, StatisticsSettings::combineCorpAndCharPlotsDefault).toBool())
        {
            try
            {
                const auto corpId = mCharacterRepository.getCorporationId(mCharacterId);
                const auto entries = (combineStats) ?
                                     (mCorpTransactionRepository.fetchInRange(QDateTime{mJournalPlot->getFrom()},
                                                                              QDateTime{mJournalPlot->getTo().addDays(1)},
                                                                              WalletTransactionRepository::EntryType::All)) :
                                     (mCorpTransactionRepository.fetchForCorporationInRange(corpId,
                                                                                            QDateTime{mJournalPlot->getFrom()},
                                                                                            QDateTime{mJournalPlot->getTo().addDays(1)},
                                                                                            WalletTransactionRepository::EntryType::All));

                valueAdder(entries);
            }
            catch (const CharacterRepository::NotFoundException &)
            {
            }
        }

        QVector<double> ticks, incomingTicks, outgoingTicks, incomingValues, outgoingValues;
        createBarTicks(ticks, incomingTicks, outgoingTicks, incomingValues, outgoingValues, values);

        mSellPlot->setData(incomingTicks, incomingValues);
        mBuyPlot->setData(outgoingTicks, outgoingValues);

        mTransactionPlot->getPlot().xAxis->setTickVector(ticks);

        mTransactionPlot->getPlot().rescaleAxes();
        mTransactionPlot->getPlot().replot();

        const auto loc = locale();

        mTransactionsIncomeLabel->setText(TextUtils::currencyToString(totalIncome, loc));
        mTransactionsOutcomeLabel->setText(TextUtils::currencyToString(totalOutcome, loc));
        mTransactionsBalanceLabel->setText(TextUtils::currencyToString(totalIncome - totalOutcome, loc));
    }

    void StatisticsWidget::updateData()
    {
        updateBalanceData();
        updateJournalData();
        updateTransactionData();
    }

    void StatisticsWidget::handleNewPreferences()
    {
        QSettings settings;
        const auto format = settings.value(UISettings::plotNumberFormatKey, UISettings::plotNumberFormatDefault).toString();

        mBalancePlot->getPlot().yAxis->setNumberFormat(format);
        mJournalPlot->getPlot().yAxis->setNumberFormat(format);
        mTransactionPlot->getPlot().yAxis->setNumberFormat(format);

        updateJournalData();
        updateTransactionData();
        updateGraphColors();

        mBalancePlot->getPlot().replot();
        mJournalPlot->getPlot().replot();
        mTransactionPlot->getPlot().replot();
    }

    void StatisticsWidget::applyAggrFilter()
    {
        const auto limit = mAggrLimitEdit->value();
        mAggrModel.reset(mCharacterId,
                         static_cast<MarketOrderRepository::AggregateColumn>(mAggrGroupingColumnCombo->currentData().toInt()),
                         static_cast<MarketOrderRepository::AggregateOrderColumn>(mAggrOrderColumnCombo->currentData().toInt()),
                         (limit == 0) ? (-1) : (limit),
                         mAggrIncludeActiveBtn->isChecked(),
                         mAggrIncludeNotFulfilledBtn->isChecked());

        mAggrView->setModel(&mAggrModel);
    }

    void StatisticsWidget::applyScript()
    {
        mScriptModel.reset(mMarketOrderRepository.fetchForCharacter(mCharacterId),
                           mAggrScriptEdit->toPlainText(),
                           (mScriptForEachModeBtn->isChecked()) ?
                           (ScriptOrderProcessingModel::Mode::ForEach) :
                           (ScriptOrderProcessingModel::Mode::Aggregate));
        mAggrView->setModel(&mScriptModel);
    }

    void StatisticsWidget::copyAggrData()
    {
        const auto indexes = mAggrView->selectionModel()->selectedIndexes();
        if (indexes.isEmpty())
            return;

        QString result;

        auto prevRow = indexes.first().row();
        for (const auto &index : indexes)
        {
            if (prevRow != index.row())
            {
                prevRow = index.row();
                result.append('\n');
            }

            result.append(mAggrModel.data(index).toString());
            result.append('\t');
        }

        QApplication::clipboard()->setText(result);
    }

    void StatisticsWidget::showScriptError(const QString &message)
    {
        QMessageBox::warning(this, tr("Script error"), message);
    }

    void StatisticsWidget::saveScript()
    {
        const auto name
            = QInputDialog::getText(this, tr("Save script"), tr("Enter script name:"), QLineEdit::Normal, mLastLoadedScript);
        if (!name.isEmpty())
        {
            mLastLoadedScript = name;

            OrderScript script{name};
            script.setCode(mAggrScriptEdit->toPlainText());

            mOrderScriptRepository.store(script);
        }
    }

    void StatisticsWidget::loadScript()
    {
        const auto name
            = QInputDialog::getItem(this, tr("Load script"), tr("Select script:"), mOrderScriptRepository.getAllNames(), 0, false);
        if (!name.isEmpty())
        {
            try
            {
                mAggrScriptEdit->setPlainText(mOrderScriptRepository.find(name)->getCode());
                mLastLoadedScript = name;
            }
            catch (const OrderScriptRepository::NotFoundException &)
            {
            }
        }
    }

    void StatisticsWidget::deleteScript()
    {
        const auto name
            = QInputDialog::getItem(this, tr("Delete script"), tr("Select script:"), mOrderScriptRepository.getAllNames(), 0, false);
        if (!name.isEmpty())
            mOrderScriptRepository.remove(name);
    }

    void StatisticsWidget::updateBalanceTooltip(QMouseEvent *event)
    {
        auto getValue = [=](const QCPGraph *graph) {
            const auto x = graph->keyAxis()->pixelToCoord(event->pos().x());
            const auto data = graph->data();
            const auto b = data->lowerBound(x);

            if (b == std::end(*data))
                return 0.;

            const auto a = (b.key() == x || b == std::begin(*data)) ? (b) : (std::prev(b));
            const auto coeff = (qFuzzyCompare(a.key(), b.key())) ? (1.) : (x - a.key()) / (b.key() - a.key());
            return b->value * coeff + a->value * (1 - coeff);
        };

        const auto sellOrdersValue = getValue(mBalancePlot->getPlot().graph(sellOrdersGraph));
        const auto buyOrdersValue = getValue(mBalancePlot->getPlot().graph(buyOrdersGraph));
        const auto corpWalletValue = getValue(mBalancePlot->getPlot().graph(corpWalletBalanceGraph));
        const auto walletValue = getValue(mBalancePlot->getPlot().graph(walletBalanceGraph));
        const auto assetValue = getValue(mBalancePlot->getPlot().graph(assetValueGraph));
        const auto totalValue = getValue(mBalancePlot->getPlot().graph(totalValueGraph));

        const auto loc = locale();
        mBalancePlot->setToolTip(
            tr("Assets: %1\nWallet: %2\nCorp. wallet: %3\nBuy orders: %4\nSell orders: %5\nTotal: %6")
                .arg(TextUtils::currencyToString(assetValue, loc))
                .arg(TextUtils::currencyToString(walletValue, loc))
                .arg(TextUtils::currencyToString(corpWalletValue, loc))
                .arg(TextUtils::currencyToString(buyOrdersValue, loc))
                .arg(TextUtils::currencyToString(sellOrdersValue, loc))
                .arg(TextUtils::currencyToString(totalValue, loc))
        );
    }

    void StatisticsWidget::updateGraphAndLegend()
    {
        auto assetGraph = mBalancePlot->getPlot().addGraph();
        assetGraph->setName(tr("Asset value"));

        auto walletGraph = mBalancePlot->getPlot().addGraph();
        walletGraph->setName(tr("Wallet balance"));

        auto corpWalletGraph = mBalancePlot->getPlot().addGraph();
        corpWalletGraph->setName(tr("Corp. wallet balance"));

        auto buyGraph = mBalancePlot->getPlot().addGraph();
        buyGraph->setName(tr("Buy order value"));

        auto sellGraph = mBalancePlot->getPlot().addGraph();
        sellGraph->setName(tr("Sell order value"));

        auto totalGraph = mBalancePlot->getPlot().addGraph();
        totalGraph->setName(tr("Total value"));

        mBalancePlot->getPlot().legend->setVisible(true);

        mIncomingPlot = createBarPlot(mJournalPlot, tr("Incoming"), Qt::green);
        mOutgoingPlot = createBarPlot(mJournalPlot, tr("Outgoing"), Qt::red);

        mSellPlot = createBarPlot(mTransactionPlot, tr("Sell"), Qt::green);
        mBuyPlot = createBarPlot(mTransactionPlot, tr("Buy"), Qt::red);

        mJournalPlot->getPlot().legend->setVisible(true);
        mTransactionPlot->getPlot().legend->setVisible(true);

        updateGraphColors();
    }

    void StatisticsWidget::updateGraphColors()
    {
        const auto sellOrdersValue = mBalancePlot->getPlot().graph(sellOrdersGraph);
        const auto buyOrdersValue = mBalancePlot->getPlot().graph(buyOrdersGraph);
        const auto corpWalletValue = mBalancePlot->getPlot().graph(corpWalletBalanceGraph);
        const auto walletValue = mBalancePlot->getPlot().graph(walletBalanceGraph);
        const auto assetValue = mBalancePlot->getPlot().graph(assetValueGraph);
        const auto totalValue = mBalancePlot->getPlot().graph(totalValueGraph);

        QSettings settings;

        sellOrdersValue->setPen(
            settings.value(StatisticsSettings::statisticsSellOrderPlotColorKey, StatisticsSettings::statisticsSellOrderPlotColorDefault).value<QColor>());
        buyOrdersValue->setPen(
            settings.value(StatisticsSettings::statisticsBuyOrderPlotColorKey, StatisticsSettings::statisticsBuyOrderPlotColorDefault).value<QColor>());
        corpWalletValue->setPen(
            settings.value(StatisticsSettings::statisticsCorpWalletPlotColorKey, StatisticsSettings::statisticsCorpWalletPlotColorDefault).value<QColor>());
        walletValue->setPen(
            settings.value(StatisticsSettings::statisticsWalletPlotColorKey, StatisticsSettings::statisticsWalletPlotColorDefault).value<QColor>());
        assetValue->setPen(
            settings.value(StatisticsSettings::statisticsAssetPlotColorKey, StatisticsSettings::statisticsAssetPlotColorDefault).value<QColor>());
        totalValue->setPen(
            settings.value(StatisticsSettings::statisticsTotalPlotColorKey, StatisticsSettings::statisticsTotalPlotColorDefault).value<QColor>());
    }

    QWidget *StatisticsWidget::createBasicStatisticsWidget()
    {
        auto widget = new QWidget{this};
        auto mainLayout = new QVBoxLayout{widget};

        QSettings settings;

        mCombineStatsBtn = new QCheckBox{tr("Combine statistics for all characters"), this};
        mainLayout->addWidget(mCombineStatsBtn);
        mCombineStatsBtn->setChecked(settings.value(StatisticsSettings::combineStatisticsKey, StatisticsSettings::combineStatisticsDefault).toBool());
        connect(mCombineStatsBtn, &QCheckBox::toggled, this, [=](bool checked) {
            QSettings settings;
            settings.setValue(StatisticsSettings::combineStatisticsKey, checked);

            updateData();
        });

        auto balanceGroup = new QGroupBox{tr("Balance"), this};
        mainLayout->addWidget(balanceGroup);

        auto balanceLayout = new QVBoxLayout{balanceGroup};

        mBalancePlot = createPlot();
        balanceLayout->addWidget(mBalancePlot);
        connect(mBalancePlot, &DateFilteredPlotWidget::filterChanged, this, &StatisticsWidget::updateBalanceData);
        connect(mBalancePlot, &DateFilteredPlotWidget::mouseMove, this, &StatisticsWidget::updateBalanceTooltip);

        auto journalGroup = new QGroupBox{tr("Wallet journal"), this};
        mainLayout->addWidget(journalGroup);

        auto journalLayout = new QVBoxLayout{journalGroup};

        mJournalPlot = createPlot();
        journalLayout->addWidget(mJournalPlot);
        connect(mJournalPlot, &DateFilteredPlotWidget::filterChanged, this, &StatisticsWidget::updateJournalData);

        QFont font;
        font.setBold(true);

        auto journalSummaryLayout = new QHBoxLayout{};
        journalLayout->addLayout(journalSummaryLayout);

        journalSummaryLayout->addWidget(new QLabel{tr("Total income:"), this}, 0, Qt::AlignRight);

        mJournalIncomeLabel = new QLabel{"-", this};
        journalSummaryLayout->addWidget(mJournalIncomeLabel, 0, Qt::AlignLeft);
        mJournalIncomeLabel->setFont(font);

        journalSummaryLayout->addWidget(new QLabel{tr("Total cost:"), this}, 0, Qt::AlignRight);

        mJournalOutcomeLabel = new QLabel{"-", this};
        journalSummaryLayout->addWidget(mJournalOutcomeLabel, 0, Qt::AlignLeft);
        mJournalOutcomeLabel->setFont(font);

        journalSummaryLayout->addWidget(new QLabel{tr("Balance:"), this}, 0, Qt::AlignRight);

        mJournalBalanceLabel = new QLabel{"-", this};
        journalSummaryLayout->addWidget(mJournalBalanceLabel, 0, Qt::AlignLeft);
        mJournalBalanceLabel->setFont(font);

        journalSummaryLayout->addStretch();

        auto transactionGroup = new QGroupBox{tr("Wallet transactions"), this};
        mainLayout->addWidget(transactionGroup);

        auto transactionLayout = new QVBoxLayout{transactionGroup};

        mTransactionPlot = createPlot();
        transactionLayout->addWidget(mTransactionPlot);
        connect(mTransactionPlot, &DateFilteredPlotWidget::filterChanged, this, &StatisticsWidget::updateTransactionData);

        auto transactionSummaryLayout = new QHBoxLayout{};
        transactionLayout->addLayout(transactionSummaryLayout);

        transactionSummaryLayout->addWidget(new QLabel{tr("Total income:"), this}, 0, Qt::AlignRight);

        mTransactionsIncomeLabel = new QLabel{"-", this};
        transactionSummaryLayout->addWidget(mTransactionsIncomeLabel, 0, Qt::AlignLeft);
        mTransactionsIncomeLabel->setFont(font);

        transactionSummaryLayout->addWidget(new QLabel{tr("Total cost:"), this}, 0, Qt::AlignRight);

        mTransactionsOutcomeLabel = new QLabel{"-", this};
        transactionSummaryLayout->addWidget(mTransactionsOutcomeLabel, 0, Qt::AlignLeft);
        mTransactionsOutcomeLabel->setFont(font);

        transactionSummaryLayout->addWidget(new QLabel{tr("Balance:"), this}, 0, Qt::AlignRight);

        mTransactionsBalanceLabel = new QLabel{"-", this};
        transactionSummaryLayout->addWidget(mTransactionsBalanceLabel, 0, Qt::AlignLeft);
        mTransactionsBalanceLabel->setFont(font);

        transactionSummaryLayout->addStretch();

        mainLayout->addStretch();

        return widget;
    }

    QWidget *StatisticsWidget::createAdvancedStatisticsWidget()
    {
        auto widget = new QWidget{this};
        auto mainLayout = new QVBoxLayout{widget};

        mainLayout->addWidget(new QLabel{tr(
            "This tab allows you to create custom reports aggregating historic market order data."), this});

        auto configGroup = new QGroupBox{this};
        mainLayout->addWidget(configGroup);

        auto configLayout = new QVBoxLayout{configGroup};

        auto simpleConfigBtn = new QRadioButton{tr("Simple aggregation"), this};
        configLayout->addWidget(simpleConfigBtn);
        simpleConfigBtn->setChecked(true);

        auto simpleConfigWidget = new QWidget{this};
        configLayout->addWidget(simpleConfigWidget);
        connect(simpleConfigBtn, &QRadioButton::toggled, simpleConfigWidget, &QWidget::setVisible);

        auto simgpleConfigLayout = new QHBoxLayout{simpleConfigWidget};

        simgpleConfigLayout->addWidget(new QLabel{tr("Group by:"), this});

        mAggrGroupingColumnCombo = new QComboBox{this};
        simgpleConfigLayout->addWidget(mAggrGroupingColumnCombo);
        mAggrGroupingColumnCombo->addItem(tr("Type"), static_cast<int>(MarketOrderRepository::AggregateColumn::TypeId));
        mAggrGroupingColumnCombo->addItem(tr("Location"), static_cast<int>(MarketOrderRepository::AggregateColumn::LocationId));

        simgpleConfigLayout->addWidget(new QLabel{tr("Order by:"), this});

        mAggrOrderColumnCombo = new QComboBox{this};
        simgpleConfigLayout->addWidget(mAggrOrderColumnCombo);
        mAggrOrderColumnCombo->addItem(tr("Id"), static_cast<int>(MarketOrderRepository::AggregateOrderColumn::Id));
        mAggrOrderColumnCombo->addItem(tr("Count"), static_cast<int>(MarketOrderRepository::AggregateOrderColumn::Count));
        mAggrOrderColumnCombo->addItem(tr("Price"), static_cast<int>(MarketOrderRepository::AggregateOrderColumn::Price));
        mAggrOrderColumnCombo->addItem(tr("Volume"), static_cast<int>(MarketOrderRepository::AggregateOrderColumn::Volume));

        simgpleConfigLayout->addWidget(new QLabel{tr("Limit:"), this});

        mAggrLimitEdit = new QSpinBox{this};
        simgpleConfigLayout->addWidget(mAggrLimitEdit);
        mAggrLimitEdit->setValue(10);
        mAggrLimitEdit->setMaximum(10000);
        mAggrLimitEdit->setSpecialValueText(tr("none"));

        mAggrIncludeActiveBtn = new QCheckBox{tr("Include active"), this};
        simgpleConfigLayout->addWidget(mAggrIncludeActiveBtn);

        mAggrIncludeNotFulfilledBtn = new QCheckBox{tr("Include expired/cancelled"), this};
        simgpleConfigLayout->addWidget(mAggrIncludeNotFulfilledBtn);

        mAggrApplyBtn = new QPushButton{tr("Apply"), this};
        simgpleConfigLayout->addWidget(mAggrApplyBtn);
        mAggrApplyBtn->setDisabled(true);
        connect(mAggrApplyBtn, &QPushButton::clicked, this, &StatisticsWidget::applyAggrFilter);

        simgpleConfigLayout->addStretch();

        auto scriptConfigBtn = new QRadioButton{tr("Script processing"), this};
        configLayout->addWidget(scriptConfigBtn);

        auto scriptConfigWidget = new QWidget{this};
        configLayout->addWidget(scriptConfigWidget);
        scriptConfigWidget->setVisible(false);
        connect(scriptConfigBtn, &QRadioButton::toggled, scriptConfigWidget, &QWidget::setVisible);

        auto scriptConfigLayout = new QHBoxLayout{scriptConfigWidget};

        QFont scriptFont{"Monospace"};
        scriptFont.setFixedPitch(true);
        scriptFont.setStyleHint(QFont::TypeWriter);

        mAggrScriptEdit = new QTextEdit{this};
        scriptConfigLayout->addWidget(mAggrScriptEdit, 1);
        mAggrScriptEdit->setPlaceholderText(tr("see the online help to learn how to use script processing"));
        mAggrScriptEdit->document()->setDefaultFont(scriptFont);

        new QtScriptSyntaxHighlighter{mAggrScriptEdit};

        auto scriptControlsLayout = new QVBoxLayout{};
        scriptConfigLayout->addLayout(scriptControlsLayout);

        mScriptApplyBtn = new QPushButton{tr("Apply"), this};
        scriptControlsLayout->addWidget(mScriptApplyBtn);
        mScriptApplyBtn->setDisabled(true);
        connect(mScriptApplyBtn, &QPushButton::clicked, this, &StatisticsWidget::applyScript);

        auto saveScriptBtn = new QPushButton{tr("Save script..."), this};
        scriptControlsLayout->addWidget(saveScriptBtn);
        connect(saveScriptBtn, &QPushButton::clicked, this, &StatisticsWidget::saveScript);

        auto loadScriptBtn = new QPushButton{tr("Load script..."), this};
        scriptControlsLayout->addWidget(loadScriptBtn);
        connect(loadScriptBtn, &QPushButton::clicked, this, &StatisticsWidget::loadScript);

        auto deleteScriptBtn = new QPushButton{tr("Delete script..."), this};
        scriptControlsLayout->addWidget(deleteScriptBtn);
        connect(deleteScriptBtn, &QPushButton::clicked, this, &StatisticsWidget::deleteScript);

        auto scriptModeGroup = new QGroupBox{tr("Mode"), this};
        scriptControlsLayout->addWidget(scriptModeGroup);

        auto scriptModeGroupLayout = new QVBoxLayout{scriptModeGroup};

        mScriptForEachModeBtn = new QRadioButton{tr("For each"), this};
        scriptModeGroupLayout->addWidget(mScriptForEachModeBtn);
        mScriptForEachModeBtn->setChecked(true);

        auto scriptAggrgateModeBtn = new QRadioButton{tr("Aggregate"), this};
        scriptModeGroupLayout->addWidget(scriptAggrgateModeBtn);

        scriptControlsLayout->addStretch();

        auto copyAct = new QAction{this};
        copyAct->setShortcuts(QKeySequence::Copy);
        connect(copyAct, &QAction::triggered, this, &StatisticsWidget::copyAggrData);

        mAggrView = new QTableView{this};
        mainLayout->addWidget(mAggrView, 1);
        mAggrView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        mAggrView->addAction(copyAct);
        mAggrView->setItemDelegate(new NumberFormatDelegate{this});

        return widget;
    }

    DateFilteredPlotWidget *StatisticsWidget::createPlot()
    {
        auto plot = new DateFilteredPlotWidget{this};
        plot->getPlot().xAxis->grid()->setVisible(false);
        plot->getPlot().yAxis->setNumberPrecision(2);
        plot->getPlot().yAxis->setLabel("ISK");

        QSettings settings;
        plot->getPlot().yAxis->setNumberFormat(
            settings.value(UISettings::plotNumberFormatKey, UISettings::plotNumberFormatDefault).toString());

        return plot;
    }

    QCPBars *StatisticsWidget::createBarPlot(DateFilteredPlotWidget *plot, const QString &name, Qt::GlobalColor color)
    {
        auto graph = std::make_unique<QCPBars>(plot->getPlot().xAxis, plot->getPlot().yAxis);
        auto graphPtr = graph.get();
        graphPtr->setWidth(3600 * 6);
        graphPtr->setName(name);
        graphPtr->setPen(QPen{color});
        graphPtr->setBrush(color);
        plot->getPlot().addPlottable(graphPtr);
        graph.release();

        return graphPtr;
    }

    void StatisticsWidget::createBarTicks(QVector<double> &ticks,
                                          QVector<double> &incomingTicks,
                                          QVector<double> &outgoingTicks,
                                          QVector<double> &incomingValues,
                                          QVector<double> &outgoingValues,
                                          const QHash<QDate, std::pair<double, double>> &values)
    {
        QHashIterator<QDate, std::pair<double, double>> it{values};
        while (it.hasNext())
        {
            it.next();

            const auto secs = QDateTime{it.key()}.toMSecsSinceEpoch() / 1000.;

            ticks << secs;

            incomingTicks << secs - 3600 * 3;
            outgoingTicks << secs + 3600 * 3;

            outgoingValues << it.value().first;
            incomingValues << it.value().second;
        }
    }
}
