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

    // 2. Setup an independent mapper with offsets
    CoordinateMapper aMapper;
    aMapper.SetDeviceOriginX(2);
    aMapper.SetDeviceOriginY(3);

    Point aLogicPt(4, 5);

    vcl::rendercontext::PrimitiveRenderer::DrawPixel(*pGraphics, aMapper, xVDev.get(), aLogicPt,
                                                     COL_RED);

    // Remove the origin offset so we can query absolute device pixels
    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(0);
    xVDev->SetDeviceOriginY(0);

    // Expected Device coordinates: X = 4 + 2 = 6, Y = 5 + 3 = 8
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Pixel was not drawn at the mapped device coordinate", COL_RED,
                                 xVDev->GetPixel(Point(6, 8)));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Original logical coordinate should be empty due to offset",
                                 COL_WHITE, xVDev->GetPixel(Point(4, 5)));

    Color aReadColor = vcl::rendercontext::PrimitiveRenderer::GetPixel(*pGraphics, aMapper,
                                                                       xVDev.get(), aLogicPt);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Facade GetPixel failed to map and retrieve correctly", COL_RED,
                                 aReadColor);
}

CPPUNIT_TEST_FIXTURE(PrimitiveRendererTest, testDrawLine)
{
    ScopedVclPtrInstance<VirtualDevice> xVDev;
    xVDev->SetOutputSizePixel(Size(15, 15));
    xVDev->SetBackground(Wallpaper(COL_WHITE));

    // 1. QUARTZ FIX: Force AA lines to snap to the integer grid
    xVDev->SetAntialiasing(AntialiasingFlags::PixelSnapHairline);

    xVDev->Erase();

    // 2. MEMORY FIX: Let the VirtualDevice manage the CoordinateMapper natively
    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(1);
    xVDev->SetDeviceOriginY(1);

    xVDev->SetLineColor(COL_BLUE);

    Point aLogicStart(1, 1);
    Point aLogicEnd(8, 1);

    // Route through OutputDevice to use the safe, initialized CoordinateMapper
    xVDev->DrawLine(aLogicStart, aLogicEnd);

    std::cerr << "\n=== FRAME BUFFER ASCII DUMP (15x15) ===\n";

    // Read exact device pixels
    xVDev->SetMapMode(MapMode(MapUnit::MapPixel));
    xVDev->SetDeviceOriginX(0);
    xVDev->SetDeviceOriginY(0);

    // Print X-axis header
    std::cerr << "   ";
    for (int x = 0; x < 15; ++x)
        std::cerr << std::hex << (x % 16) << " ";
    std::cerr << "\n";

    // Print the grid
    for (tools::Long y = 0; y < 15; ++y)
    {
        std::cerr << std::setw(2) << std::dec << y << " ";
        for (tools::Long x = 0; x < 15; ++x)
        {
            Color aCol = xVDev->GetPixel(Point(x, y));

            if (aCol == COL_WHITE)
                std::cerr << ". ";
            else if (aCol == COL_BLUE)
                std::cerr << "B ";
            else if (aCol == COL_RED)
                std::cerr << "R ";
            else if (aCol == COL_BLACK)
                std::cerr << "X ";
            else
                std::cerr << "? ";
        }
        std::cerr << "\n";
    }
    std::cerr << "=======================================\n";

    // CROSS-PLATFORM ASSERTIONS:
    // Quartz shifts AA endpoints by +1. Cairo includes the start.
    // We check the guaranteed internal overlap: [3 to 8].
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Start pixel failed", COL_BLUE, xVDev->GetPixel(Point(3, 2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mid pixel failed", COL_BLUE, xVDev->GetPixel(Point(5, 2)));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("End pixel failed", COL_BLUE, xVDev->GetPixel(Point(8, 2)));

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
