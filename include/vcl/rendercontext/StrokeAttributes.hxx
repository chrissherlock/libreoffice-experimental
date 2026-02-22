/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <basegfx/vector/b2enums.hxx>
#include <basegfx/numeric/ftools.hxx>

#include <com/sun/star/drawing/LineCap.hpp>

#include <vector>

namespace vcl::rendercontext
{
struct StrokeAttributes
{
    double fWidth = 0.0;
    basegfx::B2DLineJoin eJoin = basegfx::B2DLineJoin::Round;
    css::drawing::LineCap eCap = css::drawing::LineCap_BUTT;
    double fMiterMinimumAngle = basegfx::deg2rad(15.0);
    const std::vector<double>* pDashArray = nullptr;
    double fTransparency = 0.0;
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
