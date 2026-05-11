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

#include <vcl/dllapi.h>

#include <TransformPlan.hxx>

namespace vcl
{
/**
 * @brief The stateless execution planner for VCL Transformations.
 * * Analyzes raw affine matrices to determine their geometric invariants
 * (e.g., orientation, orthogonality) and classifies them into performance
 * taxonomies (e.g., pure translation vs. complex affine) to build highly
 * optimized TransformPlan artifacts.
 */
class VCL_DLLPUBLIC TransformCompiler
{
public:
    static TransformPlan Compile(const basegfx::B2DHomMatrix& rMat);
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
