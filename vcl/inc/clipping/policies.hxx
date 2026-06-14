/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/region.hxx>
#include "ClipCompiler.hxx"

namespace vcl::clipping
{
/**
 * Backend Execution Policies define the 'How' of clipping for different devices.
 * Each rendering backend (Skia, GDI, PDF, etc.) implements its own concrete policy.
 */
template <typename T> struct BackendClipPolicy
{
    // The mandatory interface for all backends
    static void Apply(T& rDev, const ClipPlan& rPlan)
    {
        // Default: standard region application
        rDev.SetClipRegion(rPlan.maFinalRegion);
    }
};

/**
 * Hierarchy Policy definitions for the ClipStateBuilder.
 * These are used by the ClipStateBuilder to extract data.
 */
struct HierarchyPolicy
{
    static bool ShouldClipChildren(const vcl::Window& rWin);
    static bool ShouldClipSiblings(const vcl::Window& rWin);
};

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
