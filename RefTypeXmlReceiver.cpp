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
#include "RefTypeXmlReceiver.h"

namespace Evernus
{
    template<>
    void APIXmlReceiver<RefType>::attribute(const QXmlName &name, const QStringRef &value)
    {
        if (name.localName(mNamePool) == "refTypeID")
            mCurrentElement->setId(convert<RefType::IdType>(value.toString()));
        else if (name.localName(mNamePool) == "refTypeName")
            mCurrentElement->setName(value.toString());
    }

    template<>
    void APIXmlReceiver<RefType>::endElement()
    {
        if (mCurrentElement->getId() != RefType::invalidId)
            mContainer.emplace_back(*mCurrentElement);
    }
}
