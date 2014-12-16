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
#include <stdexcept>

#include <QCoreApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QUrlQuery>
#include <QSettings>
#include <QDebug>

#include "CRESTInterface.h"

namespace Evernus
{
#ifdef EVERNUS_CREST_SISI
    const QString CRESTInterface::crestUrl = "https://api-sisi.testeveonline.com";
#else
    const QString CRESTInterface::crestUrl = "https://crest-tq.eveonline.com";
#endif

    const QString CRESTInterface::regionsUrlName = "regions";
    const QString CRESTInterface::itemTypesUrlName = "itemTypes";

    void CRESTInterface::fetchBuyMarketOrders(uint regionId, EveType::IdType typeId, const Callback &callback) const
    {
        qDebug() << "Fetching buy orders for" << regionId << "and" << typeId;

#ifdef Q_OS_WIN
        auto fetcher = [=](const QString &error) {
#else
        auto fetcher = [=, callback = callback](const auto &error) {
#endif
            if (!error.isEmpty())
            {
                callback(QJsonDocument{}, error);
                return;
            }

            QJsonDocument doc;

            try
            {
                doc = getOrders(getRegionBuyOrdersUrl(regionId), typeId);
            }
            catch (const std::exception &e)
            {
                callback(QJsonDocument{}, e.what());
                return;
            }

            callback(std::move(doc), QString{});
        };

        checkAuth(fetcher);
    }

    void CRESTInterface::fetchSellMarketOrders(uint regionId, EveType::IdType typeId, const Callback &callback) const
    {
        qDebug() << "Fetching sell orders for" << regionId << "and" << typeId;

#ifdef Q_OS_WIN
        auto fetcher = [=](const QString &error) {
#else
        auto fetcher = [=, callback = callback](const auto &error) {
#endif
            if (!error.isEmpty())
            {
                callback(QJsonDocument{}, error);
                return;
            }

            QJsonDocument doc;

            try
            {
                doc = getOrders(getRegionSellOrdersUrl(regionId), typeId);
            }
            catch (const std::exception &e)
            {
                callback(QJsonDocument{}, e.what());
                return;
            }

            callback(std::move(doc), QString{});
        };

        checkAuth(fetcher);
    }

    void CRESTInterface::updateTokenAndContinue(QString token, const QDateTime &expiry)
    {
        mAccessToken = std::move(token);
        mExpiry = expiry;

        if (mEndpoints.isEmpty())
        {
            qDebug() << "Fetching CREST endpoints...";

            const auto json = syncGet(crestUrl, "application/vnd.ccp.eve.Api-v3+json");
            std::function<void (const QJsonObject &)> addEndpoints = [this, &addEndpoints](const QJsonObject &object) {
                for (auto it = std::begin(object); it != std::end(object); ++it)
                {
                    const auto value = it.value().toObject();
                    if (value.contains("href"))
                    {
                        qDebug() << "Endpoint:" << it.key() << "->" << it.value();
                        mEndpoints[it.key()] = value.value("href").toString();
                    }
                    else
                    {
                        addEndpoints(value);
                    }
                }
            };

            addEndpoints(json.object());
        }

        for (const auto &request : mPendingRequests)
            request(QString{});

        mPendingRequests.clear();
    }

    void CRESTInterface::handleTokenError(const QString &error)
    {
        for (const auto &request : mPendingRequests)
            request(error);

        mPendingRequests.clear();
    }

    QUrl CRESTInterface::getRegionBuyOrdersUrl(uint regionId) const
    {
        return getRegionOrdersUrl(regionId, "marketBuyOrders", mRegionBuyOrdersUrls);
    }

    QUrl CRESTInterface::getRegionSellOrdersUrl(uint regionId) const
    {
        return getRegionOrdersUrl(regionId, "marketSellOrders", mRegionSellOrdersUrls);
    }

    QUrl CRESTInterface::getRegionOrdersUrl(uint regionId,
                                            const QString &urlName,
                                            RegionOrderUrlMap &map) const
    {
        if (map.contains(regionId))
            return map[regionId];

        if (!mEndpoints.contains(regionsUrlName))
            throw std::runtime_error{tr("Missing CREST regions url!").toStdString()};

        qDebug() << "Fetching region orders url:" << regionId << urlName;

        const auto doc = syncGet(QString{"%1%2/"}.arg(mEndpoints[regionsUrlName]).arg(regionId),
                                 "application/vnd.ccp.eve.Region-v1+json");

        const QUrl href = doc.object().value(urlName).toObject().value("href").toString();
        map[regionId] = href;

        qDebug() << "Region orders url:" << href;

        return href;
    }

    QJsonDocument CRESTInterface::getOrders(QUrl regionUrl, EveType::IdType typeId) const
    {
        if (!mEndpoints.contains(itemTypesUrlName))
            throw std::runtime_error{tr("Missing CREST item types url!").toStdString()};

        QUrlQuery query;
        query.addQueryItem("type", QString{"%1%2/"}.arg(mEndpoints[itemTypesUrlName]).arg(typeId));

        regionUrl.setQuery(query);

        return syncGet(regionUrl, "application/vnd.ccp.eve.MarketOrderCollection-v1+json");
    }

    template<class T>
    void CRESTInterface::checkAuth(const T &continuation) const
    {
        if (mExpiry < QDateTime::currentDateTime())
        {
            mPendingRequests.emplace_back(continuation);
            if (mPendingRequests.size() == 1)
                emit tokenRequested();
        }
        else
        {
            continuation(QString{});
        }
    }

    QJsonDocument CRESTInterface::syncGet(const QUrl &url, const QByteArray &accept) const
    {
        qDebug() << "CREST request:" << url;

        QNetworkRequest request{url};
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QString{"%1 %2"}.arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion()));
        request.setRawHeader("Authorization", "Bearer " + mAccessToken.toLatin1());
        request.setRawHeader("Accept", accept);

        auto reply = mNetworkManager.get(request);

        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        loop.exec(QEventLoop::ExcludeUserInputEvents);

        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
            throw std::runtime_error{reply->errorString().toStdString()};

        return QJsonDocument::fromJson(reply->readAll());
    }
}