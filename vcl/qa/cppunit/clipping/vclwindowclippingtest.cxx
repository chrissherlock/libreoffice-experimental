/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <vcl/outdev.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/syschild.hxx>
#include <vcl/wintypes.hxx>
#include <vcl/region.hxx>
#include <vcl/svapp.hxx>

#include <window.h>
#include <clipping_window.hxx>

namespace
{
class TestClipping : public test::BootstrapFixture
{
public:
    VclPtr<WorkWindow> CreateRootWindow()
    {
        // The root window needs to be a top-level window
        return VclPtr<WorkWindow>::Create(nullptr, WB_STDWORK);
    }

    VclPtr<SystemChildWindow> CreateChildWindow(vcl::Window* pParent)
    {
        // Children should be SystemChildWindow
        return VclPtr<SystemChildWindow>::Create(pParent, WB_STDWORK);
    }

    // Helper to create a window that ensures cleanup in test teardown
    VclPtr<SystemChildWindow> CreateTestWindow(vcl::Window* pParent = nullptr)
    {
        return VclPtr<SystemChildWindow>::Create(pParent, WB_STDWORK);
    }

    void RealizeWindow(vcl::Window* pWin, Size aSize = Size(100, 100))
    {
        pWin->SetPosSizePixel(Point(0, 0), aSize);
        pWin->Show();

        // Instead of forcing state creation, force a layout update.
        // This makes VCL believe the window needs a fresh state,
        // which triggers the proper initialization of maClipState.
        pWin->Invalidate();
        Application::Reschedule();
    }
};

CPPUNIT_TEST_FIXTURE(TestClipping, testExcludeWindowRegion_Comprehensive)
{
    // Use '0' instead of 'WB_STDWORK' to create a pure, borderless root window.
    // This prevents title bars from offsetting geometric coordinates!
    ScopedVclPtr<WorkWindow> pRoot(VclPtr<WorkWindow>::Create(nullptr, 0));
    RealizeWindow(pRoot.get());

    ScopedVclPtr<vcl::Window> pChild(VclPtr<vcl::Window>::Create(pRoot.get()));
    RealizeWindow(pChild.get());

    pChild->Show();

    // Force visibility flags for the headless environment
    pRoot->ImplGetWindowImpl()->mbReallyVisible = true;
    pChild->ImplGetWindowImpl()->mbReallyVisible = true;

    // Standard Rectangular Exclusion
    {
        pChild->SetPosSizePixel(Point(10, 10), Size(20, 20));

        vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

        vcl::clipping::excludeWindowRegion(*pChild, aRegion);

        CPPUNIT_ASSERT_MESSAGE("S1: Point inside the child should be excluded",
                               !aRegion.Contains(Point(15, 15)));
        CPPUNIT_ASSERT_MESSAGE("S1: Point outside the child should remain",
                               aRegion.Contains(Point(5, 5)));
    }

    // Window completely outside the target region
    {
        // Move child far outside the 100x100 test region
        pChild->SetPosSizePixel(Point(200, 200), Size(20, 20));

        vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

        vcl::clipping::excludeWindowRegion(*pChild, aRegion);

        CPPUNIT_ASSERT_MESSAGE("S2: Region should remain completely intact",
                               aRegion.Contains(Point(50, 50)));
    }

    // Custom Window Region (Non-Rectangular)
    {
        // Set child bounds to 50x50, located at (10,10)
        pChild->SetPosSizePixel(Point(10, 10), Size(50, 50));

        // Create a custom L-shaped region for the window
        // Note: WindowRegion coordinates are RELATIVE to the child window's top-left!
        vcl::Region aCustomShape;
        aCustomShape.Union(tools::Rectangle(Point(0, 0), Size(20, 20))); // Top-left square
        aCustomShape.Union(tools::Rectangle(Point(0, 20), Size(20, 20))); // Bottom-left square

        // Apply the custom shape to the window
        pChild->SetWindowRegionPixel(aCustomShape);

        vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

        vcl::clipping::excludeWindowRegion(*pChild, aRegion);

        // Test math:
        // Parent coord (15, 15) is Child coord (5, 5)   -> INSIDE custom shape -> Excluded (False)
        // Parent coord (40, 40) is Child coord (30, 30) -> OUTSIDE custom shape -> Remains (True)
        // EVEN THOUGH (40, 40) is inside the child's 50x50 bounding box!

        CPPUNIT_ASSERT_MESSAGE("S3: Point inside custom shape should be excluded",
                               !aRegion.Contains(Point(15, 15)));

        CPPUNIT_ASSERT_MESSAGE(
            "S3: Point inside bounding box but OUTSIDE custom shape should remain",
            aRegion.Contains(Point(40, 40)));
    }
}

} // end anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
