/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/TransformPlan.hxx>
#include <vcl/MappingPolicy.hxx>

#include <array>
#include <atomic>
#include <optional>
#include <cstddef>

namespace vcl
{
/**
 * @brief The Register File for Compiled Transforms
 *
 * Manages the O(1) storage and lock-free invalidation of execution artifacts via an 8-slot lookaside cache.
 * CoordinateMapper mutates its own state concurrently under the SolarMutex, but other threads may read.
 * This cache uses an atomic version stamp to ensure threads never read a transform compiled against stale state.
 */
class VCL_DLLPUBLIC TransformCache
{
public:
    struct CacheEntry
    {
        size_t nStateHash = 0;
        vcl::MappingPolicy ePolicy = vcl::MappingPolicy::IgnoreMapMode;
        std::optional<TransformPlan> moPlan;
    };

private:
    static constexpr size_t CACHE_SIZE = 8;

    mutable std::atomic<uint64_t> mnStateVersion{ 0 };
    mutable uint64_t mnCacheVersion{ 0 };

    // Fixed-size lookaside slots replacing the key-indexed array
    mutable std::array<CacheEntry, CACHE_SIZE> maSlots;

    /**
     * @brief Performs an internal cache flush if the state version has changed.
     */
    void checkAndFlush() const
    {
        uint64_t nCurrentVersion = mnStateVersion.load(std::memory_order_acquire);

        if (mnCacheVersion != nCurrentVersion)
        {
            for (auto& rSlot : maSlots)
            {
                rSlot.nStateHash = 0;
                rSlot.ePolicy = vcl::MappingPolicy::IgnoreMapMode;
                rSlot.moPlan.reset();
            }
            mnCacheVersion = nCurrentVersion;
        }
    }

public:
    TransformCache() = default;

    /**
     * @brief Bumps the atomic state version.
     * Any subsequent Get() or Store() calls will instantly see a version mismatch and flush.
     */
    void Invalidate() const { mnStateVersion.fetch_add(1, std::memory_order_release); }

    /**
     * @brief Retrieves a compiled transform if valid.
     * Automatically flushes all slots if the state version has mutated since the last call.
     */
    const TransformPlan* Get(size_t nHash, vcl::MappingPolicy ePolicy) const
    {
        checkAndFlush();

        for (const auto& rSlot : maSlots)
        {
            if (rSlot.moPlan && rSlot.nStateHash == nHash && rSlot.ePolicy == ePolicy)
            {
                return &(*rSlot.moPlan);
            }
        }
        return nullptr;
    }

    /**
     * @brief Stores a compiled transform artifact into the register file.
     * Uses a shifting eviction ring (evicts the oldest/last slot).
     */
    void Store(size_t nHash, vcl::MappingPolicy ePolicy, const TransformPlan& rTransform) const
    {
        checkAndFlush();

        // Shift slots right to make space at index 0 (Most Recently Used slot entry)
        for (size_t i = CACHE_SIZE - 1; i > 0; --i)
        {
            maSlots[i] = std::move(maSlots[i - 1]);
        }

        // Populate MRU slot
        maSlots[0].nStateHash = nHash;
        maSlots[0].ePolicy = ePolicy;
        maSlots[0].moPlan = rTransform;

        // Sync cache tracking back to the verified atomic state line
        mnCacheVersion = mnStateVersion.load(std::memory_order_acquire);
    }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
