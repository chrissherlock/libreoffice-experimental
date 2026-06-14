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

CPPUNIT_TEST_FIXTURE(TestClipping, testClipChildren_Comprehensive)
{
    ScopedVclPtr<WorkWindow> pRoot(VclPtr<WorkWindow>::Create(nullptr, WB_CLIPCHILDREN));
    ScopedVclPtr<vcl::Window> pChild(VclPtr<vcl::Window>::Create(pRoot.get()));

    pChild->SetPosSizePixel(Point(50, 50), Size(20, 20));

    // Force the child to clip its parent (disable NoClip)
    pChild->SetParentClipMode(ParentClipMode::NONE);

    pRoot->Show();
    pChild->Show();
    pRoot->Invalidate(InvalidateFlags::Children);
    Application::Reschedule();

    // Force visibility flags
    pRoot->ImplGetWindowImpl()->mbVisible = true;
    pRoot->ImplGetWindowImpl()->mbReallyVisible = true;
    pChild->ImplGetWindowImpl()->mbVisible = true;
    pChild->ImplGetWindowImpl()->mbReallyVisible = true;

    // Initialize ALL clipping states (Parent AND Child)
    vcl::clipping::initWinClipRegion(*pRoot);
    vcl::clipping::initWinChildClipRegion(*pRoot);
    vcl::clipping::initWinClipRegion(*pChild);

    // ==========================================
    // DIAGNOSTIC ASSERTIONS (The VCL polygraph)
    // ==========================================

    // Does the parent actually have the style?
    CPPUNIT_ASSERT_MESSAGE("Fail 1: Parent missing WB_CLIPCHILDREN",
                           (pRoot->GetStyle() & WB_CLIPCHILDREN) != 0);

    // Did the child successfully attach to the parent?
    CPPUNIT_ASSERT_MESSAGE("Fail 2: Child not in parent's list", pRoot->GetChildCount() == 1);

    // Does VCL believe the child is visible?
    CPPUNIT_ASSERT_MESSAGE("Fail 3: Child IsVisible() is false", pChild->IsVisible());

    // Does VCL believe the child is REALLY visible? (Frame mapped)
    CPPUNIT_ASSERT_MESSAGE("Fail 4: Child IsReallyVisible() is false", pChild->IsReallyVisible());

    // Is the ParentClipMode preventing it? (Should be NONE)
    CPPUNIT_ASSERT_MESSAGE("Fail 5: ParentClipMode is set to NoClip",
                           pChild->GetParentClipMode() != ParentClipMode::NoClip);

    // ==========================================

    vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));
    bool bIsRegionEmpty = vcl::clipping::clipChildren(*pRoot, aRegion);

    // Because we only punched a 20x20 hole in a 100x100 region, the region is NOT empty.
    // VCL correctly returns false to tell the paint engine to keep drawing.
    CPPUNIT_ASSERT_MESSAGE("Fail 6: Region should not be completely empty", !bIsRegionEmpty);

    // The true test of the math: Did it punch the hole?
    CPPUNIT_ASSERT_MESSAGE("Fail 7: Child region (50,50 to 70,70) not excluded",
                           !aRegion.Contains(Point(60, 60)));

    CPPUNIT_ASSERT_MESSAGE("Fail 8: Outside region should remain untouched",
                           aRegion.Contains(Point(10, 10)));
}

CPPUNIT_TEST_FIXTURE(TestClipping, testClipSiblings_Comprehensive)
{
    ScopedVclPtr<WorkWindow> pRoot(VclPtr<WorkWindow>::Create(nullptr, WB_CLIPCHILDREN));

    // Sibling 1 (Created First -> Back of the Z-Order)
    ScopedVclPtr<vcl::Window> pS1(VclPtr<vcl::Window>::Create(pRoot.get()));
    pS1->SetPosSizePixel(Point(10, 10), Size(40, 40));
    pS1->Show();

    // Sibling 2 (Created Second -> Front of the Z-Order)
    ScopedVclPtr<vcl::Window> pS2(VclPtr<vcl::Window>::Create(pRoot.get()));
    pS2->SetPosSizePixel(Point(30, 30), Size(40, 40));
    pS2->Show();

    pRoot->Show();
    pRoot->Invalidate(InvalidateFlags::Children);
    Application::Reschedule();

    // Force headless visibility for the entire tree
    pRoot->ImplGetWindowImpl()->mbVisible = true;
    pRoot->ImplGetWindowImpl()->mbReallyVisible = true;
    pS1->ImplGetWindowImpl()->mbVisible = true;
    pS1->ImplGetWindowImpl()->mbReallyVisible = true;
    pS2->ImplGetWindowImpl()->mbVisible = true;
    pS2->ImplGetWindowImpl()->mbReallyVisible = true;

    // Initialize clipping states
    vcl::clipping::initWinClipRegion(*pRoot);
    vcl::clipping::initWinClipRegion(*pS1);
    vcl::clipping::initWinClipRegion(*pS2);

    vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));
    vcl::clipping::clipSiblings(*pS2, aRegion);

    // Test the Math: S1 (Back) should be punched out
    CPPUNIT_ASSERT_MESSAGE("S1's region (10,10 to 50,50) should be excluded",
                           !aRegion.Contains(Point(15, 15)));

    // Test the Math: Area outside the siblings should remain intact
    CPPUNIT_ASSERT_MESSAGE("Outside area should remain untouched", aRegion.Contains(Point(80, 80)));
}

} // end anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
