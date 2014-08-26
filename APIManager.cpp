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
#include <QAbstractMessageHandler>
#include <QDomDocument>
#include <QXmlQuery>
#include <QDateTime>

#include "ConquerableStationListXmlReceiver.h"
#include "WalletTransactionsXmlReceiver.h"
#include "WalletJournalXmlReceiver.h"
#include "CharacterListXmlReceiver.h"
#include "MarketOrdersXmlReceiver.h"
#include "AssetListXmlReceiver.h"
#include "RefTypeXmlReceiver.h"
#include "CharacterDomParser.h"
#include "CacheTimerProvider.h"
#include "APIUtils.h"
#include "CorpKey.h"
#include "Item.h"

#include "APIManager.h"

namespace Evernus
{
    namespace
    {
        class APIXmlMessageHandler
            : public QAbstractMessageHandler
        {
        public:
            explicit APIXmlMessageHandler(QString &error)
                : QAbstractMessageHandler{}
                , mError{error}
            {
            }

            virtual ~APIXmlMessageHandler() = default;

        protected:
            virtual void handleMessage(QtMsgType type, const QString &description, const QUrl &identifier, const QSourceLocation &sourceLocation) override
            {
                if (type == QtFatalMsg && mError.isEmpty())
                    mError = QString{"%1 (%2:%3)"}.arg(description).arg(sourceLocation.line()).arg(sourceLocation.column());
            }

        private:
            QString &mError;
        };
    }

    APIManager::APIManager(CacheTimerProvider &cacheTimerProvider)
        : QObject{}
        , mCacheTimerProvider{cacheTimerProvider}
    {
        connect(&mInterface, &APIInterface::generalError, this, &APIManager::generalError);
    }

    void APIManager::fetchCharacterList(const Key &key, const Callback<CharacterList> &callback) const
    {
        if (mPendingCharacterListRequests.find(key.getId()) != std::end(mPendingCharacterListRequests))
            return;

        mPendingCharacterListRequests.emplace(key.getId());

        mInterface.fetchCharacterList(key, [key, callback, this](const QString &response, const QString &error) {
            mPendingCharacterListRequests.erase(key.getId());

            try
            {
                handlePotentialError(response, error);

                CharacterList list{parseResults<Character::IdType, APIXmlReceiver<Character::IdType>::CurElemType>(response, "characters")};
                callback(std::move(list), QString{});
            }
            catch (const std::exception &e)
            {
                callback(CharacterList{}, e.what());
            }
        });
    }

    void APIManager::fetchCharacter(const Key &key, Character::IdType characterId, const Callback<Character> &callback) const
    {
        if (mPendingCharacterRequests.find(characterId) != std::end(mPendingCharacterRequests))
            return;

        mPendingCharacterRequests.emplace(characterId);

        mInterface.fetchCharacter(key, characterId, [key, callback, characterId, this](const QString &response, const QString &error) {
            mPendingCharacterRequests.erase(characterId);

            try
            {
                handlePotentialError(response, error);

                Character character{parseResult<Character>(response)};
                character.setKeyId(key.getId());

                mCacheTimerProvider.setUtcCacheTimer(characterId,
                                                     TimerType::Character,
                                                     APIUtils::getCachedUntil(response));

                callback(std::move(character), QString{});
            }
            catch (const std::exception &e)
            {
                callback(Character{}, e.what());
            }
        });
    }

    void APIManager::fetchAssets(const Key &key, Character::IdType characterId, const Callback<AssetList> &callback) const
    {
        if (mPendingAssetsRequests.find(characterId) != std::end(mPendingAssetsRequests))
            return;

        mPendingAssetsRequests.emplace(characterId);

        mInterface.fetchAssets(key, characterId, [key, characterId, callback, this](const QString &response, const QString &error) {
            mPendingAssetsRequests.erase(characterId);

            try
            {
                handlePotentialError(response, error);

                AssetList assets{parseResults<AssetList::ItemType, std::unique_ptr<AssetList::ItemType::element_type>>(response, "assets")};
                assets.setCharacterId(characterId);

                mCacheTimerProvider.setUtcCacheTimer(characterId,
                                                     TimerType::AssetList,
                                                     APIUtils::getCachedUntil(response));

                callback(std::move(assets), QString{});
            }
            catch (const std::exception &e)
            {
                callback(AssetList{}, e.what());
            }
        });
    }

    void APIManager::fetchConquerableStationList(const Callback<ConquerableStationList> &callback) const
    {
#ifdef Q_OS_WIN
        mInterface.fetchConquerableStationList([callback, this](const QString &response, const QString &error) {
#else
        mInterface.fetchConquerableStationList([callback = callback, this](const QString &response, const QString &error) {
#endif
            try
            {
                handlePotentialError(response, error);

                ConquerableStationList stations{parseResults<ConquerableStation, APIXmlReceiver<ConquerableStation>::CurElemType>(response, "outposts")};
                callback(std::move(stations), QString{});
            }
            catch (const std::exception &e)
            {
                callback(ConquerableStationList{}, e.what());
            }
        });
    }

    void APIManager::fetchRefTypes(const Callback<RefTypeList> &callback) const
    {
#ifdef Q_OS_WIN
        mInterface.fetchRefTypes([callback, this](const QString &response, const QString &error) {
#else
        mInterface.fetchRefTypes([callback = callback, this](const QString &response, const QString &error) {
#endif
            try
            {
                handlePotentialError(response, error);

                callback(parseResults<RefType, APIXmlReceiver<RefType>::CurElemType>(response, "refTypes"), QString{});
            }
            catch (const std::exception &e)
            {
                callback(RefTypeList{}, e.what());
            }
        });
    }

    void APIManager::fetchWalletJournal(const Key &key,
                                        Character::IdType characterId,
                                        WalletJournalEntry::IdType fromId,
                                        WalletJournalEntry::IdType tillId,
                                        const Callback<WalletJournal> &callback) const
    {
        fetchWalletJournal(key, characterId, 0, fromId, tillId, std::make_shared<WalletJournal>(), callback, "transactions", TimerType::WalletJournal);
    }

    void APIManager::fetchWalletJournal(const CorpKey &key,
                                        Character::IdType characterId,
                                        quint64 corpId,
                                        WalletJournalEntry::IdType fromId,
                                        WalletJournalEntry::IdType tillId,
                                        const Callback<WalletJournal> &callback) const
    {
        fetchWalletJournal(key, characterId, corpId, fromId, tillId, std::make_shared<WalletJournal>(), callback, "entries", TimerType::CorpWalletJournal);
    }

    void APIManager::fetchWalletTransactions(const Key &key,
                                             Character::IdType characterId,
                                             WalletTransaction::IdType fromId,
                                             WalletTransaction::IdType tillId,
                                             const Callback<WalletTransactions> &callback) const
    {
        fetchWalletTransactions(key, characterId, 0, fromId, tillId, std::make_shared<WalletTransactions>(), callback, TimerType::WalletTransactions);
    }

    void APIManager::fetchWalletTransactions(const CorpKey &key,
                                             Character::IdType characterId,
                                             quint64 corpId,
                                             WalletTransaction::IdType fromId,
                                             WalletTransaction::IdType tillId,
                                             const Callback<WalletTransactions> &callback) const
    {
        fetchWalletTransactions(key, characterId, corpId, fromId, tillId, std::make_shared<WalletTransactions>(), callback, TimerType::CorpWalletTransactions);
    }

    void APIManager::fetchMarketOrders(const Key &key, Character::IdType characterId, const Callback<MarketOrders> &callback) const
    {
        doFetchMarketOrders(key, characterId, callback, TimerType::MarketOrders);
    }

    void APIManager::fetchMarketOrders(const CorpKey &key, Character::IdType characterId, const Callback<MarketOrders> &callback) const
    {
        doFetchMarketOrders(key, characterId, callback, TimerType::CorpMarketOrders);
    }

    template<class Key>
    void APIManager::fetchWalletJournal(const Key &key,
                                        Character::IdType characterId,
                                        quint64 corpId,
                                        WalletJournalEntry::IdType fromId,
                                        WalletJournalEntry::IdType tillId,
                                        std::shared_ptr<WalletJournal> &&journal,
                                        const Callback<WalletJournal> &callback,
                                        const QString &rowsetName,
                                        TimerType timerType) const
    {
        mInterface.fetchWalletJournal(key, characterId, fromId,
                                      [=](const QString &response, const QString &error) mutable {
            try
            {
                handlePotentialError(response, error);

                auto parsed
                    = parseResults<WalletJournal::value_type, APIXmlReceiver<WalletJournal::value_type>::CurElemType>(response, rowsetName);

                auto reachedEnd = parsed.empty();
                auto nextFromId = std::numeric_limits<WalletJournalEntry::IdType>::max();

                for (auto &entry : parsed)
                {
                    const auto id = entry.getId();
                    if (id > tillId)
                    {
                        entry.setCharacterId(characterId);
                        entry.setCorporationId(corpId);
                        journal->emplace(std::move(entry));

                        if (nextFromId > id)
                            nextFromId = id;
                    }
                    else
                    {
                        reachedEnd = true;
                    }
                }

                if (reachedEnd)
                {
                    mCacheTimerProvider.setUtcCacheTimer(characterId,
                                                         timerType,
                                                         APIUtils::getCachedUntil(response));

                    callback(std::move(*journal), QString{});
                }
                else
                {
                    fetchWalletJournal(key, characterId, corpId, nextFromId, tillId, std::move(journal), callback, rowsetName, timerType);
                }
            }
            catch (const std::exception &e)
            {
                callback(WalletJournal{}, e.what());
            }
        });
    }

    template<class Key>
    void APIManager::fetchWalletTransactions(const Key &key,
                                             Character::IdType characterId,
                                             quint64 corpId,
                                             WalletTransaction::IdType fromId,
                                             WalletTransaction::IdType tillId,
                                             std::shared_ptr<WalletTransactions> &&transactions,
                                             const Callback<WalletTransactions> &callback,
                                             TimerType timerType) const
    {
        mInterface.fetchWalletTransactions(key, characterId, fromId,
                                           [=](const QString &response, const QString &error) mutable {
            try
            {
                handlePotentialError(response, error);

                auto parsed
                    = parseResults<WalletTransactions::value_type, APIXmlReceiver<WalletTransactions::value_type>::CurElemType>(response, "transactions");

                auto reachedEnd = parsed.empty();
                auto nextFromId = std::numeric_limits<WalletTransaction::IdType>::max();

                for (auto &entry : parsed)
                {
                    const auto id = entry.getId();
                    if (id > tillId)
                    {
                        entry.setCharacterId(characterId);
                        entry.setCorporationId(corpId);
                        transactions->emplace(std::move(entry));

                        if (nextFromId > id)
                            nextFromId = id;
                    }
                    else
                    {
                        reachedEnd = true;
                    }
                }

                if (reachedEnd)
                {
                    mCacheTimerProvider.setUtcCacheTimer(characterId,
                                                         timerType,
                                                         APIUtils::getCachedUntil(response));

                    callback(std::move(*transactions), QString{});
                }
                else
                {
                    fetchWalletTransactions(key, characterId, corpId, nextFromId, tillId, std::move(transactions), callback, timerType);
                }
            }
            catch (const std::exception &e)
            {
                callback(WalletTransactions{}, e.what());
            }
        });
    }

    template<class Key>
    void APIManager::doFetchMarketOrders(const Key &key, Character::IdType characterId, const Callback<MarketOrders> &callback, TimerType timerType) const
    {
#ifdef Q_OS_WIN
        mInterface.fetchMarketOrders(key, characterId, [callback, characterId, timerType, this](const QString &response, const QString &error) {
#else
        mInterface.fetchMarketOrders(key, characterId, [callback = callback, characterId, timerType, this](const QString &response, const QString &error) {
#endif
            try
            {
                handlePotentialError(response, error);

                mCacheTimerProvider.setUtcCacheTimer(characterId,
                                                     timerType,
                                                     APIUtils::getCachedUntil(response));

                callback(parseResults<MarketOrders::value_type, APIXmlReceiver<MarketOrders::value_type>::CurElemType>(response, "orders"), QString{});
            }
            catch (const std::exception &e)
            {
                callback(MarketOrders{}, e.what());
            }
        });
    }

    template<class T, class CurElem>
    std::vector<T> APIManager::parseResults(const QString &xml, const QString &rowsetName)
    {
        std::vector<T> result;
        QString error;

        APIXmlMessageHandler handler{error};

        QXmlQuery query;
        query.setMessageHandler(&handler);
        query.setFocus(xml);
        query.setQuery(QString{"//rowset[@name='%1']/row"}.arg(rowsetName));

        APIXmlReceiver<T, CurElem> receiver{result, query.namePool()};
        query.evaluateTo(&receiver);

        if (!error.isEmpty())
            throw std::runtime_error{error.toStdString()};

        return result;
    }

    template<class T>
    T APIManager::parseResult(const QString &xml)
    {
        QDomDocument document;
        if (!document.setContent(xml))
            throw std::runtime_error{tr("Invalid XML document received!").toStdString()};

        return APIDomParser::parse<T>(document.documentElement().firstChildElement("result"));
    }

    QString APIManager::queryPath(const QString &path, const QString &xml)
    {
        QString out;

        QXmlQuery query;
        query.setFocus(xml);
        query.setQuery(path);
        query.evaluateTo(&out);

        return out.trimmed();
    }

    void APIManager::handlePotentialError(const QString &xml, const QString &error)
    {
        if (!error.isEmpty())
            throw std::runtime_error{error.toStdString()};

        if (xml.isEmpty())
            throw std::runtime_error{tr("No XML document received!").toStdString()};

        const auto errorText = queryPath("/eveapi/error/text()", xml);
        if (!errorText.isEmpty())
            throw std::runtime_error{errorText.toStdString()};
    }
}
