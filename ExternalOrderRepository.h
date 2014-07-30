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

#include "ExternalOrderImporter.h"
#include "ExternalOrder.h"
#include "Repository.h"

namespace Evernus
{
    class ExternalOrderRepository
        : public Repository<ExternalOrder>
    {
    public:

        using Repository::Repository;
        virtual ~ExternalOrderRepository() = default;

        virtual QString getTableName() const override;
        virtual QString getIdColumn() const override;

        virtual EntityPtr populate(const QSqlRecord &record) const override;

        void create() const;

        EntityPtr findSellByTypeAndLocation(ExternalOrder::TypeIdType typeId, ExternalOrder::LocationIdType locationId) const;
        EntityList findBuyByTypeAndRegion(ExternalOrder::TypeIdType typeId, uint regionId) const;

        void removeObsolete(const ExternalOrderImporter::TypeLocationPairs &set) const;

    private:
        virtual QStringList getColumns() const override;
        virtual void bindValues(const ExternalOrder &entity, QSqlQuery &query) const override;
        virtual void bindPositionalValues(const ExternalOrder &entity, QSqlQuery &query) const override;
    };
}