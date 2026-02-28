/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <iomanip>
#include <iostream>
#include <cassert>

#include <cppunit/TestAssert.h>
#include <iostream>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <test/bootstrapfixture.hxx>

#include <tools/mapunit.hxx>

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>
#include <CoordinateMapper.hxx>

namespace
{
// Inheriting from test::BootstrapFixture ensures the UNO component context
// and VCL configuration managers are fully booted before VirtualDevice is used.
class PrimitiveRendererTest : public test::BootstrapFixture
{
};

CPPUNIT_TEST_FIXTURE(PrimitiveRendererTest, testDrawPixel)
{
    ScopedVclPtrInstance<VirtualDevice> xVDev;
    xVDev->SetOutputSizePixel(Size(10, 10));
    xVDev->SetBackground(Wallpaper(COL_WHITE));
    xVDev->Erase();

    SalGraphics* pGraphics = xVDev->GetGraphics();
    CPPUNIT_ASSERT_MESSAGE("Failed to acquire SalGraphics", pGraphics != nullptr);

    // MEMORY FIX: Give the mapper a MapMode so scaling fractions are initialized!
    CoordinateMapper aMapper;
    aMapper.ResetMapMode(MapMode(MapUnit::MapPixel));
    aMapper.SetDeviceOriginX(2);
    aMapper.SetDeviceOriginY(3);

    Point aLogicPt(4, 5);

    // NEW SIGNATURE: No OutputDevice* passed to the facade
    vcl::rendercontext::PrimitiveRenderer::DrawPixel(*pGraphics, aMapper, aLogicPt, COL_RED);

    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(0);
    xVDev->SetDeviceOriginY(0);

    // Expected Device coordinates: X = 4 + 2 = 6, Y = 5 + 3 = 8
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Pixel was not drawn at the mapped device coordinate", COL_RED,
                                 xVDev->GetPixel(Point(6, 8)));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Original logical coordinate should be empty due to offset",
                                 COL_WHITE, xVDev->GetPixel(Point(4, 5)));
}

CPPUNIT_TEST_FIXTURE(PrimitiveRendererTest, testDrawLine)
{
    ScopedVclPtrInstance<VirtualDevice> xVDev;
    xVDev->SetOutputSizePixel(Size(15, 15));
    xVDev->SetBackground(Wallpaper(COL_WHITE));
    xVDev->Erase();

    SalGraphics* pGraphics = xVDev->GetGraphics();
    CPPUNIT_ASSERT_MESSAGE("Failed to acquire SalGraphics", pGraphics != nullptr);

    // MAC QUARTZ FIX: Force VCL to push the blue stroke color down into the hardware CGContext
    xVDev->SetLineColor(COL_BLUE);
    xVDev->DrawPixel(Point(-1, -1));

    // MEMORY FIX: Give the mapper a MapMode so scaling fractions are initialized!
    CoordinateMapper aMapper;
    aMapper.ResetMapMode(MapMode(MapUnit::MapPixel));
    aMapper.SetDeviceOriginX(1);
    aMapper.SetDeviceOriginY(1);

    Point aLogicStart(1, 1);
    Point aLogicEnd(8, 1);

    // NEW SIGNATURE: No OutputDevice* passed to the facade
    vcl::rendercontext::PrimitiveRenderer::DrawLine(*pGraphics, aMapper, aLogicStart, aLogicEnd,
                                                    false, false);

    // Read exact device pixels
    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(0);
    xVDev->SetDeviceOriginY(0);

    // Expected Device coordinates: Start (2, 2) to End (9, 2)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Start pixel failed", COL_BLUE, xVDev->GetPixel(Point(2, 2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mid pixel failed", COL_BLUE, xVDev->GetPixel(Point(5, 2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("End pixel failed", COL_BLUE, xVDev->GetPixel(Point(9, 2)));

    // Ensure the offset worked and the original logical coordinate is empty
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Offset failed", COL_WHITE, xVDev->GetPixel(Point(1, 1)));
}

CPPUNIT_TEST_FIXTURE(PrimitiveRendererTest, testDrawRect)
{
    ScopedVclPtrInstance<VirtualDevice> xVDev;
    xVDev->SetOutputSizePixel(Size(15, 15));
    xVDev->SetBackground(Wallpaper(COL_WHITE));

    xVDev->SetAntialiasing(AntialiasingFlags::PixelSnapHairline);
    xVDev->Erase();

    xVDev->SetLineColor(COL_RED);
    xVDev->SetFillColor(COL_BLUE);

    // Set the offset natively so the hardware backend matrix
    // stays in perfect sync with the CoordinateMapper math.
    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(2);
    xVDev->SetDeviceOriginY(2);

    // A logical 4x4 square starting at (1, 1)
    tools::Rectangle aLogicRect(Point(1, 1), Size(4, 4));

    // This now routes through OutputDevice::DrawRect ->
    // FlushGraphicsState() -> PrimitiveRenderer::DrawRect!
    xVDev->DrawRect(aLogicRect);

    // Read exact device pixels by resetting the origin
    xVDev->SetDeviceOriginX(0);
    xVDev->SetDeviceOriginY(0);

    // Expected Device coordinates: TopLeft (3, 3), Size 4x4 -> BottomRight (6, 6)

    // Check interior fill first (Center pixel)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Fill mapping failed", COL_BLUE, xVDev->GetPixel(Point(4, 4)));

    // Check border (Top Left corner)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Border mapping failed", COL_RED, xVDev->GetPixel(Point(3, 3)));

    // Ensure offsets worked and nothing drew outside the mapped area
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Offset failed", COL_WHITE, xVDev->GetPixel(Point(1, 1)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Boundary leak", COL_WHITE, xVDev->GetPixel(Point(7, 7)));
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
