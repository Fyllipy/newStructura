#pragma once

#include <algorithm>

namespace Structura::App {

template<typename Container, typename IdGetter>
int nextExternalIdFrom(const Container &items, IdGetter idGetter)
{
    int maxId = 0;
    for (const auto &item : items) {
        maxId = std::max(maxId, idGetter(item));
    }
    return maxId + 1;
}

template<typename Container, typename IdGetter, typename IdSetter>
void ensurePositiveExternalIds(Container &items, IdGetter idGetter, IdSetter idSetter)
{
    int nextId = nextExternalIdFrom(items, idGetter);
    for (auto &item : items) {
        if (idGetter(item) <= 0) {
            idSetter(item, nextId++);
        }
    }
}

template<typename Container, typename IdGetter, typename IdApplier>
void applyMissingExternalIds(const Container &items, IdGetter idGetter, IdApplier idApplier)
{
    int nextId = nextExternalIdFrom(items, idGetter);
    for (const auto &item : items) {
        if (idGetter(item) <= 0) {
            idApplier(item, nextId++);
        }
    }
}

} // namespace Structura::App
