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
#include <clipping/ClipStateBuilder.hxx>

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

    // Define geometric constraints to match our test space
    pRoot->SetOutputSizePixel(Size(100, 100));
    pChild->SetPosSizePixel(Point(50, 50), Size(20, 20));
    pChild->SetParentClipMode(ParentClipMode::NONE);

    pRoot->Show();
    pChild->Show();

    // The Compiler is the sole authority.
    // We request the plan for the Root window.
    auto aTopology
        = vcl::clipping::ClipStateBuilder::Build(*pRoot, vcl::clipping::ClipSpace::AbsoluteDevice);
    vcl::clipping::ClipPlan aPlan = vcl::clipping::ClipCompiler::Compile(aTopology);

    // aPlan.maFinalRegion is now the clipping-adjusted canvas.
    // We intersect it with our test region to verify the "hole" is present.
    vcl::Region aTestRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));
    aTestRegion.Intersect(aPlan.maFinalRegion);

    // Verify the child area is missing (punched out)
    CPPUNIT_ASSERT_MESSAGE("Child area at (60,60) should be excluded from the paint region",
                           !aTestRegion.Contains(Point(60, 60)));

    // Verify the parent area outside the child remains intact
    CPPUNIT_ASSERT_MESSAGE("Parent area at (10,10) should remain for painting",
                           aTestRegion.Contains(Point(10, 10)));
}

} // end anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
