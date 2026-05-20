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
#include <vcl/TransformTypes.hxx>
#include <vcl/TransformPlan.hxx>

#include <array>
#include <atomic>
#include <optional>

namespace vcl
{
/**
 * @brief The Register File for Compiled Transforms
 * * Manages the O(1) storage and lock-free invalidation of execution artifacts.
 * CoordinateMapper mutates its own state concurrently under the SolarMutex,
 * but other threads may read. This cache uses an atomic version stamp to
 * ensure threads never read a transform compiled against stale state.
 */
class VCL_DLLPUBLIC TransformCache
{
private:
    mutable std::atomic<uint64_t> mnStateVersion{ 0 };
    mutable uint64_t mnCacheVersion{ 0 };

    mutable std::array<std::optional<TransformPlan>, static_cast<size_t>(TransformKey::Count)>
        maSlots;

public:
    TransformCache() = default;

    /**
     * @brief Bumps the atomic state version.
     * Any subsequent Get() calls will instantly see a version mismatch and flush.
     */
    void Invalidate() const { mnStateVersion.fetch_add(1, std::memory_order_release); }

    /**
     * @brief Retrieves a compiled transform if valid.
     * Automatically flushes all slots if the state version has mutated since the last call.
     */
    const TransformPlan* Get(TransformKey eKey) const
    {
        uint64_t nCurrentVersion = mnStateVersion.load(std::memory_order_acquire);

        if (mnCacheVersion != nCurrentVersion)
        {
            for (auto& slot : maSlots)
                slot.reset();
            mnCacheVersion = nCurrentVersion;
        }

        const auto& rSlot = maSlots[static_cast<size_t>(eKey)];
        return rSlot ? &(*rSlot) : nullptr;
    }

    /**
     * @brief Stores a compiled transform artifact into the register file.
     */
    void Store(TransformKey eKey, const TransformPlan& rTransform) const
    {
        maSlots[static_cast<size_t>(eKey)] = rTransform;
        mnCacheVersion = mnStateVersion.load(std::memory_order_acquire);
    }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
