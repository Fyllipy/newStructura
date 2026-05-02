#pragma once

#include <QHash>

namespace Structura::App {

template<typename Key, typename Value, typename Container, typename KeyFn>
QHash<Key, Value> buildLookup(const Container &items, KeyFn keyFn)
{
    QHash<Key, Value> map;
    map.reserve(static_cast<int>(items.size()));
    for (const auto &item : items) {
        map.insert(keyFn(item), item);
    }
    return map;
}

template<typename Key, typename Container, typename KeyFn>
QHash<Key, int> buildCounts(const Container &items, KeyFn keyFn)
{
    QHash<Key, int> counts;
    counts.reserve(static_cast<int>(items.size()));
    for (const auto &item : items) {
        const Key key = keyFn(item);
        counts.insert(key, counts.value(key, 0) + 1);
    }
    return counts;
}

template<typename Value, typename Container, typename KeyFn, typename FilterFn>
QHash<int, Value> buildPositiveExternalLookup(const Container &items, KeyFn keyFn, FilterFn filterFn)
{
    QHash<int, Value> map;
    map.reserve(static_cast<int>(items.size()));
    for (const auto &item : items) {
        if (filterFn(item)) {
            map.insert(keyFn(item), item);
        }
    }
    return map;
}

} // namespace Structura::App
