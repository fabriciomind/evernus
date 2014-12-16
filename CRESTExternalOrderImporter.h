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

#include <QNetworkAccessManager>
#include <QWebView>

#include "ExternalOrderImporter.h"
#include "ExternalOrder.h"
#include "CRESTManager.h"
#include "SimpleCrypt.h"

namespace Evernus
{
    class EveDataProvider;

    class CRESTExternalOrderImporter
        : public ExternalOrderImporter
    {
        Q_OBJECT

    public:
        CRESTExternalOrderImporter(QByteArray clientId, QByteArray clientSecret, const EveDataProvider &dataProvider, QObject *parent = nullptr);
        virtual ~CRESTExternalOrderImporter() = default;

        virtual bool eventFilter(QObject *watched, QEvent *event) override;

        virtual void fetchExternalOrders(const TypeLocationPairs &target) const override;

    signals:
        void tokenError(const QString &error);
        void acquiredToken(const QString &accessToken, const QDateTime &expiry);

    private slots:
        void fetchToken();

    private:
        static const QString loginUrl;
        static const QString redirectUrl;

        const EveDataProvider &mDataProvider;

        const QByteArray mClientId;
        const QByteArray mClientSecret;

        SimpleCrypt mCrypt;

        CRESTManager mManager;
        mutable uint mRequestCount = 0;
        mutable bool mPreparingRequests = false;

        mutable std::vector<ExternalOrder> mResult;

        QString mRefreshToken;

        QNetworkAccessManager mNetworkManager;

        std::unique_ptr<QWebView> mAuthView;

        bool hasClientCredentials() const;

        void processResult(std::vector<ExternalOrder> &&orders, const QString &errorText) const;
    };
}