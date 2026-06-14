/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>

#include <vcl/region.hxx>

#include <vector>
#include <optional>

namespace vcl::clipping
{
/** * Represents the topological state of a single UI element
 * during a clipping compilation pass.
 */
struct ClipNode
{
    tools::Rectangle maBounds;
    // We store minimal state to decide clipping behavior
};

/**
 * The immutable input for the ClipCompiler.
 * Once built, this struct provides all the context needed to calculate
 * the clipping region, free of any 'Window' or 'OutputDevice' dependencies.
 */
struct ClipState
{
    // --- Topological Context ---
    tools::Rectangle maBounds;
    std::optional<vcl::Region> maCustomRegion;

    // --- Policy Flags ---
    bool bClipChildren;
    bool bClipSiblings;
    bool bIsVisible;

    // --- Hierarchy/State ---
    // Flattened lists to replace WindowHierarchy pointers
    std::vector<ClipNode> maChildren;
    std::vector<ClipNode> maSiblings; // Flattened Z-order list

    // --- Future-proofing ---
    // If your compiler needs more context (e.g., DPI scale), add it here.
};

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
