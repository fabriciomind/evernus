#pragma once

#include <vector>
#include <memory>

#include "ItemData.h"

namespace Evernus
{
    class Item
    {
    public:
        typedef ItemData::IdType IdType;
        typedef std::vector<std::unique_ptr<Item>> ItemList;
        typedef ItemList::iterator ItemIterator;
        typedef ItemList::const_iterator ConstItemIterator;

        Item() = default;
        explicit Item(IdType id);
        Item(const Item &) = delete;
        Item(Item &&) = default;
        virtual ~Item() = default;

        IdType getId() const noexcept;
        void setId(IdType id) noexcept;

        ItemData::TypeIdType getTypeId() const;
        void setTypeId(const ItemData::TypeIdType &id);

        ItemData::LocationIdType getLocationId() const;
        void setLocationId(const ItemData::LocationIdType &id);

        uint getQuantity() const noexcept;
        void setQuantity(uint value) noexcept;

        ItemData getItemData() const &;
        ItemData &&getItemData() && noexcept;
        void setItemData(const ItemData &data);
        void setItemData(ItemData &&data);

        ItemIterator begin() noexcept;
        ConstItemIterator begin() const noexcept;
        ItemIterator end() noexcept;
        ConstItemIterator end() const noexcept;

        void addItem(std::unique_ptr<Item> &&item);

        Item &operator =(const Item &) = delete;
        Item &operator =(Item &&) = default;

    private:
        IdType mId = 0;
        ItemData mData;
        ItemList mContents;
    };
}
