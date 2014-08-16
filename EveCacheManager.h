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

#include <vector>

#include <QStringList>
#include <QSet>

#include "EveCacheNodes.h"

namespace Evernus
{
    class EveCacheManager
    {
    public:
        explicit EveCacheManager(const QStringList &machoNetPaths);
        explicit EveCacheManager(QStringList &&machoNetPaths);
        ~EveCacheManager() = default;

        void addCacheFolderFilter(const QString &name);
        void addCacheFolderFilter(QString &&name);

        void addMethodFilter(const QString &name);
        void addMethodFilter(QString &&name);

        void parseMachoNet();

        const std::vector<EveCacheNode::NodePtr> &getStreams() const noexcept;

    private:
        const QStringList mMachoNetPaths;
        QSet<QString> mCacheFolderFilters, mMethodFilters;

        std::vector<EveCacheNode::NodePtr> mStreams;
    };
}