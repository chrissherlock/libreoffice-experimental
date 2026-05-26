/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "AccessibleViewForwarder.hxx"
#include <AccessibleChartView.hxx>

#include <tools/mapunit.hxx>
#include <vcl/window.hxx>

using namespace ::com::sun::star;

namespace chart
{

AccessibleViewForwarder::AccessibleViewForwarder( AccessibleChartView* pAccChartView, vcl::Window* pWindow )
    :m_pAccChartView( pAccChartView )
    ,m_pWindow( pWindow )
    ,m_aMapMode( MapUnit::Map100thMM )
{
}

AccessibleViewForwarder::~AccessibleViewForwarder()
{
}

// ________ IAccessibleViewforwarder ________

tools::Rectangle AccessibleViewForwarder::GetVisibleArea() const
{
    if (!m_pWindow)
        return tools::Rectangle();

    return m_pWindow->convertTo<vcl::LogicRect>(
        vcl::WindowRect(tools::Rectangle(Point(0, 0), m_pWindow->GetOutputSizePixel())),
        m_aMapMode);
}

Point AccessibleViewForwarder::LogicToWindow( const Point& rPoint ) const
{
    if (!m_pAccChartView && m_pWindow )
        return Point();

    awt::Point aLocation = m_pAccChartView->getLocationOnScreen();
    Point aTopLeft( aLocation.X, aLocation.Y );
    return m_pWindow->convertTo<vcl::WindowPoint>(vcl::LogicPoint(rPoint), m_aMapMode).get() + aTopLeft;
}

Size AccessibleViewForwarder::LogicToWindow( const Size& rSize ) const
{
    if (!m_pWindow)
        return Size();

    return m_pWindow->convertTo<vcl::WindowSize>(
        vcl::LogicSize(rSize),
        m_aMapMode
    ).get();
}

} // namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
