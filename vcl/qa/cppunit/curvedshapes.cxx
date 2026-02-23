/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <test/outputdevice.hxx>

#include <tools/color.hxx>

#include <vcl/virdev.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/BitmapReadAccess.hxx>

class VclCurvedShapesTest : public test::BootstrapFixture
{
public:
    VclCurvedShapesTest()
        : BootstrapFixture(true, false)
    {
    }

protected:
    // Helper to setup a standardized 100x100 white virtual device
    VclPtr<VirtualDevice> createVDev()
    {
        VclPtr<VirtualDevice> pVDev = VclPtr<VirtualDevice>::Create();
        pVDev->SetOutputSizePixel(Size(100, 100));
        pVDev->SetBackground(Wallpaper(COL_WHITE));
        pVDev->Erase();
        return pVDev;
    }

    // Helper to push a "trap" color to the hardware backend to test state leakage
    void pushTrapState(VirtualDevice* pVDev)
    {
        pVDev->SetFillColor(COL_LIGHTRED);
        pVDev->SetLineColor(COL_LIGHTRED);
        pVDev->DrawRect(tools::Rectangle(0, 0, 10, 10)); // Force push to SalGraphics
    }
};

CPPUNIT_TEST_FIXTURE(VclCurvedShapesTest, testEllipseFillColorState)
{
    auto pVDev = createVDev();
    pushTrapState(pVDev.get());

    // Set the intended color. Prior to the PrepareGraphicsOutput() refactor,
    // DrawEllipse forgot to flush mbFillColorDirty, leaving the trap color active.
    pVDev->SetFillColor(COL_LIGHTGREEN);
    pVDev->SetLineColor(COL_TRANSPARENT);

    // Draw an 80x80 ellipse in the center
    pVDev->DrawEllipse(tools::Rectangle(10, 10, 90, 90));

    Bitmap aBmp = pVDev->GetBitmap(Point(0, 0), Size(100, 100));
    BitmapScopedReadAccess pAccess(aBmp);
    CPPUNIT_ASSERT(pAccess);

    // The dead center of the ellipse should be filled with green
    Color aCenterColor = pAccess->GetColor(50, 50);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Ellipse fill color state leaked! Backend used stale state.",
                                 COL_LIGHTGREEN, aCenterColor);
}

CPPUNIT_TEST_FIXTURE(VclCurvedShapesTest, testArcLineColorState)
{
    auto pVDev = createVDev();
    pushTrapState(pVDev.get());

    // Arcs are lines only. We test that PrepareGraphicsOutput(false) correctly flushes line state.
    pVDev->SetLineColor(COL_LIGHTBLUE);
    pVDev->SetFillColor(COL_TRANSPARENT);

    // Draw an arc in the top-right quadrant.
    // Bounding box: 0,0 to 100,100. Start: Top-Center (50,0). End: Right-Center (100,50).
    pVDev->DrawArc(tools::Rectangle(0, 0, 100, 100), Point(50, 0), Point(100, 50));

    Bitmap aBmp = pVDev->GetBitmap(Point(0, 0), Size(100, 100));
    BitmapScopedReadAccess pAccess(aBmp);
    CPPUNIT_ASSERT(pAccess);

    // Check the exact start point of the arc (top center)
    Color aStartPointColor = pAccess->GetColor(
        0,
        50); // Note: GetColor is (y, x) in some VCL pixel layouts, but assuming (x, y) wrapper here. Safe coordinate is (50, 0).
    aStartPointColor = pAccess->GetColor(50, 0);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Arc line color state leaked! Backend used stale state.",
                                 COL_LIGHTBLUE, aStartPointColor);
}

CPPUNIT_TEST_FIXTURE(VclCurvedShapesTest, testPieFillColorState)
{
    auto pVDev = createVDev();
    pushTrapState(pVDev.get());

    pVDev->SetFillColor(COL_LIGHTGREEN);
    pVDev->SetLineColor(COL_TRANSPARENT);

    // Draw a pie wedge in the top-right quadrant.
    pVDev->DrawPie(tools::Rectangle(0, 0, 100, 100), Point(50, 0), Point(100, 50));

    Bitmap aBmp = pVDev->GetBitmap(Point(0, 0), Size(100, 100));
    BitmapScopedReadAccess pAccess(aBmp);
    CPPUNIT_ASSERT(pAccess);

    // For a pie wedge from center (50,50) to top-right arc, the point (75, 25) is well inside.
    Color aWedgeColor = pAccess->GetColor(75, 25);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Pie fill color state leaked! Backend used stale state.",
                                 COL_LIGHTGREEN, aWedgeColor);
}

CPPUNIT_TEST_FIXTURE(VclCurvedShapesTest, testChordFillColorState)
{
    auto pVDev = createVDev();
    pushTrapState(pVDev.get());

    pVDev->SetFillColor(COL_LIGHTGREEN);
    pVDev->SetLineColor(COL_TRANSPARENT);

    // Draw a chord in the top-right quadrant.
    pVDev->DrawChord(tools::Rectangle(0, 0, 100, 100), Point(50, 0), Point(100, 50));

    Bitmap aBmp = pVDev->GetBitmap(Point(0, 0), Size(100, 100));
    BitmapScopedReadAccess pAccess(aBmp);
    CPPUNIT_ASSERT(pAccess);

    // A chord connects the two arc endpoints directly.
    // The straight line is from (50,0) to (100,50).
    // The curved arc bulges out towards (100,0).
    // Point (80, 20) is inside the chord area.
    Color aChordColor = pAccess->GetColor(80, 20);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Chord fill color state leaked! Backend used stale state.",
                                 COL_LIGHTGREEN, aChordColor);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
