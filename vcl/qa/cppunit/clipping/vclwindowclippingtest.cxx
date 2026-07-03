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
#include <clipping/ClippingManager.hxx>

namespace
{
class TestClipping : public test::BootstrapFixture
{
public:
    VclPtr<WorkWindow> CreateRootWindow()
    {
        return VclPtr<WorkWindow>::Create(nullptr, WB_STDWORK);
    }

    void RealizeWindow(vcl::Window* pWin, Size aSize = Size(100, 100))
    {
        pWin->SetPosSizePixel(Point(0, 0), aSize);
        pWin->Show();
        pWin->Invalidate();
        Application::Reschedule();
    }
};

CPPUNIT_TEST_FIXTURE(TestClipping, testClipChildren_Comprehensive)
{
    ScopedVclPtr<WorkWindow> pRoot(VclPtr<WorkWindow>::Create(nullptr, WB_CLIPCHILDREN));
    ScopedVclPtr<vcl::Window> pChild(VclPtr<vcl::Window>::Create(pRoot.get()));

    pChild->SetPosSizePixel(Point(50, 50), Size(20, 20));
    pChild->SetParentClipMode(ParentClipMode::NONE);

    pRoot->Show();
    pChild->Show();

    // Stateless ClippingManager does not require manual initialization calls.
    vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

    pRoot->GetOutDev()->GetClippingManager(*pRoot).ClipChildren(*pRoot, aRegion, false);

    CPPUNIT_ASSERT(!aRegion.IsEmpty());
    CPPUNIT_ASSERT(!aRegion.Contains(Point(60, 60)));
    CPPUNIT_ASSERT(aRegion.Contains(Point(10, 10)));
}

CPPUNIT_TEST_FIXTURE(TestClipping, testClipSiblings_Comprehensive)
{
    ScopedVclPtr<WorkWindow> pRoot(VclPtr<WorkWindow>::Create(nullptr, WB_CLIPCHILDREN));

    // Sibling 1 (Back of the Z-Order)
    ScopedVclPtr<vcl::Window> pS1(VclPtr<vcl::Window>::Create(pRoot.get()));
    pS1->SetPosSizePixel(Point(10, 10), Size(40, 40));

    // Sibling 2 (Front of the Z-Order)
    ScopedVclPtr<vcl::Window> pS2(VclPtr<vcl::Window>::Create(pRoot.get()));
    pS2->SetPosSizePixel(Point(30, 30), Size(40, 40));

    // Force visibility flags for the headless environment so IsReallyVisible() evaluates to true
    pRoot->ImplGetWindowImpl()->mbReallyVisible = true;
    pS1->ImplGetWindowImpl()->mbReallyVisible = true;
    pS2->ImplGetWindowImpl()->mbReallyVisible = true;

    vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

    // Use the ClippingManager to clip siblings FOR S1.
    // This tells the manager to look for anything "on top" of S1 (which is S2) and exclude it.
    pS1->GetOutDev()->GetClippingManager(*pS1).ClipSiblings(*pS1, aRegion);

    // Test the Math: S2 (Front) should be punched out of the region
    CPPUNIT_ASSERT_MESSAGE("S2's region should be excluded", !aRegion.Contains(Point(35, 35)));

    // Test the Math: S1 (Back) should NOT be punched out, because we are clipping FOR it
    CPPUNIT_ASSERT_MESSAGE("S1's region should remain", aRegion.Contains(Point(15, 15)));

    // Test the Math: Area outside both should remain intact
    CPPUNIT_ASSERT_MESSAGE("Outside area should remain untouched", aRegion.Contains(Point(80, 80)));
}

} // end anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
