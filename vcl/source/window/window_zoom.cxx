/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <vcl/window.hxx>

#include <window.h>

namespace vcl
{
void Window::SetZoom(double fZoom)
{
    if (mpWindowImpl && mpWindowImpl->mfZoom != fZoom)
    {
        mpWindowImpl->mfZoom = fZoom;
        CompatStateChanged(StateChangedType::Zoom);
    }
}

void Window::SetZoomedPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    double fZoom = GetZoom();
    if (fZoom != 1.0)
    {
        vcl::Font aFont(rFont);
        Size aSize = aFont.GetFontSize();
        aSize.setWidth(basegfx::fround<tools::Long>(aSize.Width() * fZoom));
        aSize.setHeight(basegfx::fround<tools::Long>(aSize.Height() * fZoom));
        aFont.SetFontSize(aSize);
        SetPointFont(rRenderContext, aFont);
    }
    else
    {
        SetPointFont(rRenderContext, rFont);
    }
}

tools::Long Window::CalcZoom(tools::Long nCalc) const
{
    double fZoom = GetZoom();
    if (fZoom != 1.0)
    {
        double n = nCalc * fZoom;
        nCalc = basegfx::fround<tools::Long>(n);
    }
    return nCalc;
}

double Window::GetZoom() const { return mpWindowImpl->mfZoom; }

bool Window::IsZoom() const { return mpWindowImpl->mfZoom != 1.0; }

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
