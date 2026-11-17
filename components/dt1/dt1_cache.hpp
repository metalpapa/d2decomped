// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dt1.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace d2d::dt1 {

class ArchiveCache {
public:
    template <typename Loader>
    std::shared_ptr<Archive> get(std::string_view path, Pixels pixels, Loader&& loader) {
        const std::string source(path);
        std::string key(source);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        const auto cache_key = std::make_pair(std::move(key), pixels);
        {
            std::lock_guard lock(mutex_);
            if (const auto found = cache_.find(cache_key); found != cache_.end())
                if (auto shared = found->second.lock()) return shared;
        }
        auto bytes = std::invoke(std::forward<Loader>(loader), source);
        if (!bytes) return nullptr;
        auto made = std::make_shared<Archive>(*bytes, pixels);
        std::lock_guard lock(mutex_);
        if (const auto found = cache_.find(cache_key); found != cache_.end())
            if (auto raced = found->second.lock()) return raced;
        cache_[cache_key] = made;
        return made;
    }

private:
    std::mutex mutex_;
    std::map<std::pair<std::string, Pixels>, std::weak_ptr<Archive>> cache_;
};

}  // namespace d2d::dt1