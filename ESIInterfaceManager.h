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

#include <QDateTime>
#include <QObject>
#include <QString>

#include "QObjectDeleteLaterDeleter.h"
#include "ESIInterfaceErrorLimiter.h"
#include "CitadelAccessCache.h"
#include "Character.h"
#include "ESIOAuth.h"

class QUrl;

namespace Evernus
{
    class ESIInterface;

    class ESIInterfaceManager final
        : public QObject
    {
        Q_OBJECT

    public:
        ESIInterfaceManager(QString clientId, QString clientSecret, QObject *parent = nullptr);
        ESIInterfaceManager(const ESIInterfaceManager &) = delete;
        ESIInterfaceManager(ESIInterfaceManager &&) = default;
        virtual ~ESIInterfaceManager();

        void handleNewPreferences();
        void clearRefreshTokens();
        void cancelSsoAuth(Character::IdType charId);

        const ESIInterface &selectNextInterface();

        const CitadelAccessCache &getCitadelAccessCache() const noexcept;
        CitadelAccessCache &getCitadelAccessCache() noexcept;

        ESIInterfaceManager &operator =(const ESIInterfaceManager &) = delete;
        ESIInterfaceManager &operator =(ESIInterfaceManager &&) = default;

    signals:
        void ssoAuthRequested(Character::IdType charId, const QUrl &url);

    private:
        std::vector<std::unique_ptr<ESIInterface, QObjectDeleteLaterDeleter>> mInterfaces;
        std::size_t mCurrentInterface{0};

        CitadelAccessCache mCitadelAccessCache;
        ESIInterfaceErrorLimiter mErrorLimiter;
        ESIOAuth mOAuth;

        void createInterfaces();

        void readCitadelAccessCache();
        void writeCitadelAccessCache();

        static QString getCachePath();
    };
}
