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
#include <queue>

#include <QStandardPaths>
#include <QSqlDatabase>
#include <QDataStream>
#include <QSqlQuery>
#include <QDebug>
#include <QFile>
#include <QDir>

#include "ConquerableStationRepository.h"
#include "MarketOrderRepository.h"
#include "RefTypeRepository.h"
#include "APIManager.h"

#include "CachingEveDataProvider.h"

namespace Evernus
{
    const QString CachingEveDataProvider::nameCacheFileName = "generic_names";

    CachingEveDataProvider::CachingEveDataProvider(const EveTypeRepository &eveTypeRepository,
                                                   const MetaGroupRepository &metaGroupRepository,
                                                   const ExternalOrderRepository &externalOrderRepository,
                                                   const MarketOrderRepository &marketOrderRepository,
                                                   const MarketOrderRepository &corpMarketOrderRepository,
                                                   const ConquerableStationRepository &conquerableStationRepository,
                                                   const MarketGroupRepository &marketGroupRepository,
                                                   const RefTypeRepository &refTypeRepository,
                                                   const APIManager &apiManager,
                                                   const QSqlDatabase &eveDb,
                                                   QObject *parent)
        : EveDataProvider{parent}
        , mEveTypeRepository{eveTypeRepository}
        , mMetaGroupRepository{metaGroupRepository}
        , mExternalOrderRepository{externalOrderRepository}
        , mMarketOrderRepository{marketOrderRepository}
        , mCorpMarketOrderRepository{corpMarketOrderRepository}
        , mConquerableStationRepository{conquerableStationRepository}
        , mMarketGroupRepository{marketGroupRepository}
        , mRefTypeRepository{refTypeRepository}
        , mAPIManager{apiManager}
        , mEveDb{eveDb}
    {
        const auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir dataCacheDir{cacheDir + "/data"};

        QFile nameCache{dataCacheDir.filePath(nameCacheFileName)};
        if (nameCache.open(QIODevice::ReadOnly))
        {
            QDataStream cacheStream{&nameCache};
            cacheStream >> mGenericNameCache;
        }
    }

    CachingEveDataProvider::~CachingEveDataProvider()
    {
        try
        {
            const auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            QDir dataCacheDir{cacheDir + "/data"};

            if (dataCacheDir.mkpath("."))
            {
                QFile nameCache{dataCacheDir.filePath(nameCacheFileName)};
                if (nameCache.open(QIODevice::WriteOnly))
                {
                    QDataStream cacheStream{&nameCache};
                    cacheStream << mGenericNameCache;
                }
            }
        }
        catch (...)
        {
        }
    }

    QString CachingEveDataProvider::getTypeName(EveType::IdType id) const
    {
        return getEveType(id)->getName();
    }

    QString CachingEveDataProvider::getTypeMarketGroupParentName(EveType::IdType id) const
    {
        const auto type = getEveType(id);
        const auto marketGroupId = type->getMarketGroupId();
        return (marketGroupId) ? (getMarketGroupParent(*marketGroupId)->getName()) : (QString{});
    }

    QString CachingEveDataProvider::getTypeMarketGroupName(EveType::IdType id) const
    {
        const auto type = getEveType(id);
        const auto marketGroupId = type->getMarketGroupId();
        return (marketGroupId) ? (getMarketGroup(*marketGroupId)->getName()) : (QString{});
    }

    MarketGroup::IdType CachingEveDataProvider::getTypeMarketGroupParentId(EveType::IdType id) const
    {
        const auto type = getEveType(id);
        const auto marketGroupId = type->getMarketGroupId();
        return (marketGroupId) ? (getMarketGroupParent(*marketGroupId)->getId()) : (MarketGroup::invalidId);
    }

    const std::unordered_map<EveType::IdType, QString> &CachingEveDataProvider::getAllTypeNames() const
    {
        if (!mTypeNameCache.empty())
            return mTypeNameCache;

        mTypeNameCache = mEveTypeRepository.fetchAllNames();
        return mTypeNameCache;
    }

    QString CachingEveDataProvider::getTypeMetaGroupName(EveType::IdType id) const
    {
        const auto it = mTypeMetaGroupCache.find(id);
        if (it != std::end(mTypeMetaGroupCache))
            return it->second->getName();

        MetaGroupRepository::EntityPtr result;

        try
        {
            result = mMetaGroupRepository.fetchForType(id);
        }
        catch (const MetaGroupRepository::NotFoundException &)
        {
            result = std::make_shared<MetaGroup>();
        }

        mTypeMetaGroupCache.emplace(id, result);
        return result->getName();
    }

    QString CachingEveDataProvider::getGenericName(quint64 id) const
    {
        if (id == 0)
            return tr("(unknown)");

        if (mGenericNameCache.contains(id))
            return mGenericNameCache[id];

        if (mPendingNameRequests.find(id) == std::end(mPendingNameRequests))
        {
            qDebug() << "Fetching generic name:" << id;

            mPendingNameRequests.emplace(id);

            mAPIManager.fetchGenericName(id, [id, this](auto &&data, const auto &error) {
                qDebug() << "Got generic name:" << id << data << " (" << error << ")";

                if (error.isEmpty())
                    mGenericNameCache[id] = std::move(data);
                else
                    mGenericNameCache[id] = tr("(unknown)");

                mPendingNameRequests.erase(id);
                if (mPendingNameRequests.empty())
                    emit namesChanged();
            });
        }

        return tr("(unknown)");
    }

    double CachingEveDataProvider::getTypeVolume(EveType::IdType id) const
    {
        const auto it = mTypeCache.find(id);
        if (it != std::end(mTypeCache))
            return it->second->getVolume();

        EveTypeRepository::EntityPtr result;

        try
        {
            result = mEveTypeRepository.find(id);
        }
        catch (const EveTypeRepository::NotFoundException &)
        {
            result = std::make_shared<EveType>();
        }

        mTypeCache.emplace(id, result);
        return result->getVolume();
    }

    std::shared_ptr<ExternalOrder> CachingEveDataProvider::getTypeSellPrice(EveType::IdType id, quint64 stationId) const
    {
        return getTypeSellPrice(id, stationId, true);
    }

    std::shared_ptr<ExternalOrder> CachingEveDataProvider::getTypeBuyPrice(EveType::IdType id, quint64 stationId) const
    {
        const auto key = std::make_pair(id, stationId);
        const auto it = mBuyPrices.find(key);
        if (it != std::end(mBuyPrices))
            return it->second;

        auto result = std::make_shared<ExternalOrder>();
        mBuyPrices.emplace(key, result);

        const auto solarSystemId = getStationSolarSystemId(stationId);
        if (solarSystemId == 0)
            return result;

        const auto regionId = getSolarSystemRegionId(solarSystemId);
        if (regionId == 0)
            return result;

        const auto jIt = mSystemJumpMap.find(regionId);
        if (jIt == std::end(mSystemJumpMap))
            return result;

        const auto &jumpMap = jIt->second;
        const auto isReachable = [stationId, solarSystemId, &jumpMap, this](const auto &order) {
            const auto range = order->getRange();
            if (range == -1)
                return stationId == order->getStationId();

            std::unordered_set<uint> visited;
            std::queue<uint> candidates;

            visited.emplace(solarSystemId);
            candidates.emplace(solarSystemId);

            auto depth = 0;

            auto incrementNode = 0u, lastParentNode = solarSystemId;

            while (!candidates.empty())
            {
                const auto current = candidates.front();
                candidates.pop();

                if (current == incrementNode)
                {
                    ++depth;
                    if (depth > range)
                        return false;
                }

                if (order->getSolarSystemId() == current)
                    return true;

                const auto children = jumpMap.equal_range(current);

                if (current == lastParentNode)
                {
                    if (children.first == children.second)
                    {
                        if (!candidates.empty())
                            lastParentNode = candidates.front();
                    }
                    else
                    {
                        lastParentNode = incrementNode = children.first->second;
                    }
                }

                for (auto it = children.first; it != children.second; ++it)
                {
                    if (visited.find(it->second) == std::end(visited))
                    {
                        visited.emplace(it->second);
                        candidates.emplace(it->second);
                    }
                }
            }

            return false;
        };

        const auto &orders = getExternalOrders(id, regionId);
        for (const auto &order : orders)
        {
            if (order->getPrice() <= result->getPrice() || !isReachable(order))
                continue;

            result = order;
            mBuyPrices[key] = result;
        }

        return result;
    }

    void CachingEveDataProvider::updateExternalOrders(const std::vector<ExternalOrder> &orders)
    {
        ExternalOrderImporter::TypeLocationPairs affectedOrders;

        std::vector<std::reference_wrapper<const ExternalOrder>> toStore;
        for (const auto &order : orders)
        {
            toStore.emplace_back(std::cref(order));
            affectedOrders.emplace(std::make_pair(order.getTypeId(), order.getStationId()));
        }

        clearExternalOrderCaches();

        mExternalOrderRepository.removeObsolete(affectedOrders);
        mExternalOrderRepository.batchStore(toStore, true);
    }

    QString CachingEveDataProvider::getLocationName(quint64 id) const
    {
        const auto it = mLocationNameCache.find(id);
        if (it != std::end(mLocationNameCache))
            return it->second;

        QString result;
        if (id >= 66000000 && id <= 66014933)
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT stationName FROM staStations WHERE stationID = ?");
            query.bindValue(0, id - 6000001);

            DatabaseUtils::execQuery(query);
            query.next();

            result = query.value(0).toString();
        }
        else if (id >= 66014934 && id <= 67999999)
        {
            try
            {
                auto station = mConquerableStationRepository.find(id - 6000000);
                result = station->getName();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else if (id >= 60014861 && id <= 60014928)
        {
            try
            {
                auto station = mConquerableStationRepository.find(id);
                result = station->getName();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else if (id > 60000000 && id <= 61000000)
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT stationName FROM staStations WHERE stationID = ?");
            query.bindValue(0, id);

            DatabaseUtils::execQuery(query);
            query.next();

            result = query.value(0).toString();
        }
        else if (id > 61000000)
        {
            try
            {
                auto station = mConquerableStationRepository.find(id);
                result = station->getName();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT itemName FROM mapDenormalize WHERE itemID = ?");
            query.bindValue(0, id);

            DatabaseUtils::execQuery(query);
            query.next();

            result = query.value(0).toString();
        }

        mLocationNameCache.emplace(id, result);
        return result;
    }

    QString CachingEveDataProvider::getRegionName(uint id) const
    {
        const auto it = mRegionNameCache.find(id);
        if (it != std::end(mRegionNameCache))
            return it->second;

        QSqlQuery query{mEveDb};
        query.prepare("SELECT regionName FROM mapRegions WHERE regionID = ?");
        query.bindValue(0, id);

        DatabaseUtils::execQuery(query);
        query.next();

        return mRegionNameCache.emplace(id, query.value(0).toString()).first->second;
    }

    QString CachingEveDataProvider::getSolarSystemName(uint id) const
    {
        const auto it = mSolarSystemNameCache.find(id);
        if (it != std::end(mSolarSystemNameCache))
            return it->second;

        QSqlQuery query{mEveDb};
        query.prepare("SELECT solarSystemName FROM mapSolarSystems WHERE solarSystemID = ?");
        query.bindValue(0, id);

        DatabaseUtils::execQuery(query);
        query.next();

        return mSolarSystemNameCache.emplace(id, query.value(0).toString()).first->second;
    }

    QString CachingEveDataProvider::getRefTypeName(uint id) const
    {
        const auto it = mRefTypeNames.find(id);
        return (it != std::end(mRefTypeNames)) ? (it->second) : (QString{});
    }

    const std::vector<EveDataProvider::MapLocation> &CachingEveDataProvider::getRegions() const
    {
        if (mRegionCache.empty())
        {
            auto query = mEveDb.exec("SELECT regionID, regionName FROM mapRegions ORDER BY regionName");

            const auto size = query.size();
            if (size > 0)
                mRegionCache.reserve(size);

            while (query.next())
                mRegionCache.emplace_back(std::make_pair(query.value(0).toUInt(), query.value(1).toString()));
        }

        return mRegionCache;
    }

    const std::vector<EveDataProvider::MapLocation> &CachingEveDataProvider::getConstellations(uint regionId) const
    {
        if (mConstellationCache.find(regionId) == std::end(mConstellationCache))
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT constellationID, constellationName FROM mapConstellations WHERE regionID = ? ORDER BY constellationName");
            query.bindValue(0, regionId);

            DatabaseUtils::execQuery(query);

            auto &constellations = mConstellationCache[regionId];

            const auto size = query.size();
            if (size > 0)
                constellations.reserve(size);

            while (query.next())
                constellations.emplace_back(std::make_pair(query.value(0).toUInt(), query.value(1).toString()));
        }

        return mConstellationCache[regionId];
    }

    const std::vector<EveDataProvider::MapLocation> &CachingEveDataProvider::getSolarSystems(uint constellationId) const
    {
        if (mSolarSystemCache.find(constellationId) == std::end(mSolarSystemCache))
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT solarSystemID, solarSystemName FROM mapSolarSystems WHERE constellationID = ? ORDER BY solarSystemName");
            query.bindValue(0, constellationId);

            DatabaseUtils::execQuery(query);

            auto &systems = mSolarSystemCache[constellationId];

            const auto size = query.size();
            if (size > 0)
                systems.reserve(size);

            while (query.next())
                systems.emplace_back(std::make_pair(query.value(0).toUInt(), query.value(1).toString()));
        }

        return mSolarSystemCache[constellationId];
    }

    const std::vector<EveDataProvider::Station> &CachingEveDataProvider::getStations(uint solarSystemId) const
    {
        if (mStationCache.find(solarSystemId) == std::end(mStationCache))
        {
            QSqlQuery query{mEveDb};
            query.prepare(
                "SELECT stationID id, stationName name FROM staStations WHERE solarSystemID = ? "
                "UNION "
                "SELECT itemID id, itemName name FROM mapDenormalize WHERE solarSystemID = ?");
            query.bindValue(0, solarSystemId);
            query.bindValue(1, solarSystemId);

            DatabaseUtils::execQuery(query);

            auto &stations = mStationCache[solarSystemId];

            const auto size = query.size();
            if (size > 0)
                stations.reserve(size);

            while (query.next())
                stations.emplace_back(std::make_pair(query.value(0).toUInt(), query.value(1).toString()));

            const auto conqStations = mConquerableStationRepository.fetchForSolarSystem(solarSystemId);
            for (const auto &station : conqStations)
                stations.emplace_back(std::make_pair(station->getId(), station->getName()));

            std::sort(std::begin(stations), std::end(stations), [](const auto &a, const auto &b) {
                return a.second < b.second;
            });
        }

        return mStationCache[solarSystemId];
    }

    double CachingEveDataProvider::getSolarSystemSecurityStatus(uint solarSystemId) const
    {
        const auto it = mSecurityStatuses.find(solarSystemId);
        if (it != std::end(mSecurityStatuses))
            return it->second;

        QSqlQuery query{mEveDb};
        query.prepare("SELECT security FROM mapSolarSystems WHERE solarSystemID = ?");
        query.bindValue(0, solarSystemId);

        DatabaseUtils::execQuery(query);

        query.next();

        return mSecurityStatuses.emplace(solarSystemId, query.value(0).toDouble()).first->second;
    }

    void CachingEveDataProvider::precacheJumpMap()
    {
        auto query = mEveDb.exec("SELECT fromRegionID, fromSolarSystemID, toSolarSystemID FROM mapSolarSystemJumps WHERE fromRegionID = toRegionID");
        while (query.next())
            mSystemJumpMap[query.value(0).toUInt()].emplace(query.value(1).toUInt(), query.value(2).toUInt());
    }

    void CachingEveDataProvider::precacheRefTypes()
    {
        const auto refs = mRefTypeRepository.fetchAll();
        if (refs.empty())
        {
            qDebug() << "Fetching ref types...";
            mAPIManager.fetchRefTypes([this](const auto &refs, const auto &error) {
                if (error.isEmpty())
                {
                    mRefTypeRepository.batchStore(refs, true);
                    for (const auto &ref : refs)
                        mRefTypeNames.emplace(ref.getId(), std::move(ref).getName());
                }
                else
                {
                    qDebug() << error;
                }
            });
        }
        else
        {
            for (const auto &ref : refs)
                mRefTypeNames.emplace(ref->getId(), std::move(*ref).getName());
        }
    }

    void CachingEveDataProvider::clearExternalOrderCaches()
    {
        mSellPrices.clear();
        mBuyPrices.clear();
        mTypeRegionOrderCache.clear();
    }

    void CachingEveDataProvider::clearStationCache()
    {
        mStationCache.clear();
    }

    std::shared_ptr<ExternalOrder> CachingEveDataProvider::getTypeSellPrice(EveType::IdType id, quint64 stationId, bool dontThrow) const
    {
        const auto key = std::make_pair(id, stationId);
        const auto it = mSellPrices.find(key);
        if (it != std::end(mSellPrices))
            return it->second;

        std::shared_ptr<ExternalOrder> result;

        try
        {
            result = mExternalOrderRepository.findSellByTypeAndStation(id, stationId, mMarketOrderRepository, mCorpMarketOrderRepository);
        }
        catch (const ExternalOrderRepository::NotFoundException &)
        {
            if (!dontThrow)
                throw;

            result = std::make_shared<ExternalOrder>();
        }

        mSellPrices.emplace(key, result);
        return result;
    }

    EveTypeRepository::EntityPtr CachingEveDataProvider::getEveType(EveType::IdType id) const
    {
        const auto it = mTypeCache.find(id);
        if (it != std::end(mTypeCache))
            return it->second;

        EveTypeRepository::EntityPtr type;

        try
        {
            type = mEveTypeRepository.find(id);
        }
        catch (const EveTypeRepository::NotFoundException &)
        {
            type = std::make_shared<EveType>();
        }

        return mTypeCache.emplace(id, type).first->second;
    }

    MarketGroupRepository::EntityPtr CachingEveDataProvider::getMarketGroupParent(MarketGroup::IdType id) const
    {
        const auto it = mTypeMarketGroupParentCache.find(id);
        if (it != std::end(mTypeMarketGroupParentCache))
            return it->second;

        MarketGroupRepository::EntityPtr result;

        try
        {
            result = mMarketGroupRepository.findParent(id);
        }
        catch (const MarketGroupRepository::NotFoundException &)
        {
            result = std::make_shared<MarketGroup>();
        }

        return mTypeMarketGroupParentCache.emplace(id, result).first->second;
    }

    MarketGroupRepository::EntityPtr CachingEveDataProvider::getMarketGroup(MarketGroup::IdType id) const
    {
        const auto it = mTypeMarketGroupCache.find(id);
        if (it != std::end(mTypeMarketGroupCache))
            return it->second;

        MarketGroupRepository::EntityPtr result;

        try
        {
            result = mMarketGroupRepository.find(id);
        }
        catch (const MarketGroupRepository::NotFoundException &)
        {
            result = std::make_shared<MarketGroup>();
        }

        return mTypeMarketGroupCache.emplace(id, result).first->second;
    }

    uint CachingEveDataProvider::getSolarSystemRegionId(uint systemId) const
    {
        const auto it = mSolarSystemRegionCache.find(systemId);
        if (it != std::end(mSolarSystemRegionCache))
            return it->second;

        QSqlQuery query{mEveDb};
        query.prepare("SELECT regionID FROM mapSolarSystems WHERE solarSystemID = ?");
        query.bindValue(0, systemId);

        DatabaseUtils::execQuery(query);
        query.next();

        const auto regionId = query.value(0).toUInt();

        mSolarSystemRegionCache.emplace(systemId, regionId);
        return regionId;
    }

    uint CachingEveDataProvider::getStationSolarSystemId(quint64 stationId) const
    {
        const auto it = mLocationSolarSystemCache.find(stationId);
        if (it != std::end(mLocationSolarSystemCache))
            return it->second;

        uint systemId = 0;
        if (stationId >= 66000000 && stationId <= 66014933)
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT solarSystemID FROM staStations WHERE stationID = ?");
            query.bindValue(0, stationId - 6000001);

            DatabaseUtils::execQuery(query);
            query.next();

            systemId = query.value(0).toUInt();
        }
        else if (stationId >= 66014934 && stationId <= 67999999)
        {
            try
            {
                auto station = mConquerableStationRepository.find(stationId - 6000000);
                systemId = station->getSolarSystemId();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else if (stationId >= 60014861 && stationId <= 60014928)
        {
            try
            {
                auto station = mConquerableStationRepository.find(stationId);
                systemId = station->getSolarSystemId();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else if (stationId > 60000000 && stationId <= 61000000)
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT solarSystemID FROM staStations WHERE stationID = ?");
            query.bindValue(0, stationId);

            DatabaseUtils::execQuery(query);
            query.next();

            systemId = query.value(0).toUInt();
        }
        else if (stationId > 61000000)
        {
            try
            {
                auto station = mConquerableStationRepository.find(stationId);
                systemId = station->getSolarSystemId();
            }
            catch (const ConquerableStationRepository::NotFoundException &)
            {
            }
        }
        else
        {
            QSqlQuery query{mEveDb};
            query.prepare("SELECT solarSystemID FROM mapDenormalize WHERE itemID = ?");
            query.bindValue(0, stationId);

            DatabaseUtils::execQuery(query);
            query.next();

            systemId = query.value(0).toUInt();
        }

        mLocationSolarSystemCache.emplace(stationId, systemId);
        return systemId;
    }

    const ExternalOrderRepository::EntityList &CachingEveDataProvider::getExternalOrders(EveType::IdType typeId, uint regionId) const
    {
        const auto key = std::make_pair(typeId, regionId);
        const auto it = mTypeRegionOrderCache.find(key);
        if (it != std::end(mTypeRegionOrderCache))
            return it->second;

        return mTypeRegionOrderCache.emplace(
            key, mExternalOrderRepository.findBuyByTypeAndRegion(typeId, regionId, mMarketOrderRepository, mCorpMarketOrderRepository))
                .first->second;
    }
}