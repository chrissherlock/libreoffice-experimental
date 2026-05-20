/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/CoordinateState.hxx>
#include <vcl/TransformPlan.hxx>
#include <vcl/TransformTypes.hxx>
#include <vcl/TransformCache.hxx>

namespace vcl
{
/**
 * Deterministic compiler that synthesizes a TransformPlan from a CoordinateState.
 * Orchestrates the temporal shift from configuration state to execution plan.
 */
class TransformRouter
{
private:
    // The O(1) artifact storage
    mutable vcl::TransformCache maCache;

    TransformKey ResolveKey(const TransformRequest& rReq) const;

    // The core compositional math logic
    basegfx::B2DHomMatrix BuildMatrix(const CoordinateState& rState,
                                      const TransformRequest& rReq) const;

public:
    // Entry point: Resolves the request to a Plan, utilizing the cache if possible.
    const TransformPlan& Compile(const CoordinateState& rState, const TransformRequest& rReq) const;

    void Invalidate() { maCache.Invalidate(); }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
