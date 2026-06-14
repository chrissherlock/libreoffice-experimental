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
#include <vcl/region.hxx>

#include "ClipState.hxx"

namespace vcl::clipping
{
/**
 * An immutable plan representing the result of a clipping compilation.
 */
struct ClipPlan
{
    vcl::Region maFinalRegion;
    bool mbEmpty = false;
};

/**
 * The ClipCompiler performs the geometric calculation based on the
 * provided ClipState. It is pure, deterministic, and device-independent.
 */
class VCL_DLLPUBLIC ClipCompiler
{
public:
    static ClipPlan Compile(const ClipState& rState);
};

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
