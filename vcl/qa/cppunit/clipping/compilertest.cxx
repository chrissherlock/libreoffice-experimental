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

#include <vcl/window.hxx>

#include <window.h>
#include <clipping/ClipStateBuilder.hxx>
#include <clipping/ClipCompiler.hxx>

namespace
{
CPPUNIT_TEST_FIXTURE(test::BootstrapFixture, testBuilderHierarchyCapture)
{
    using namespace vcl::clipping;

    // Create a parent and a child to test hierarchy capture
    VclPtr<vcl::Window> pParent = VclPtr<vcl::Window>::Create(nullptr, WB_STDWORK);
    VclPtr<vcl::Window> pChild = VclPtr<vcl::Window>::Create(pParent, WB_STDWORK);

    pParent->SetOutputSizePixel(Size(100, 100));
    pChild->SetPosSizePixel(Point(10, 10), Size(20, 20));

    // Force visibility flags for headless environment
    pParent->ImplGetWindowImpl()->mbVisible = true;
    pParent->ImplGetWindowImpl()->mbReallyVisible = true;
    pChild->ImplGetWindowImpl()->mbVisible = true;
    pChild->ImplGetWindowImpl()->mbReallyVisible = true;

    // Force the child to clip its parent (disable NoClip)
    pChild->SetParentClipMode(ParentClipMode::NONE);

    // Build the state
    ClipState aState = ClipStateBuilder::Build(*pParent, ClipSpace::AbsoluteDevice);

    // Verify capture
    CPPUNIT_ASSERT_EQUAL(size_t(1), aState.maChildren.size());
    CPPUNIT_ASSERT_EQUAL(tools::Long(10), aState.maChildren[0].maBounds.Left());

    pParent.disposeAndClear();
}

CPPUNIT_TEST_FIXTURE(test::BootstrapFixture, testClipCompiler_SimpleExclusion)
{
    using namespace vcl::clipping;

    // Mocking the state directly to test pure compiler logic
    ClipState aState;
    aState.maBounds = tools::Rectangle(0, 0, 100, 100);
    aState.bClipChildren = true;
    aState.maChildren.push_back({ tools::Rectangle(25, 25, 50, 50) });

    ClipPlan aPlan = ClipCompiler::Compile(aState);

    vcl::Region aExpected(tools::Rectangle(0, 0, 100, 100));
    aExpected.Exclude(tools::Rectangle(25, 25, 50, 50));

    CPPUNIT_ASSERT_EQUAL(aExpected, aPlan.maFinalRegion);
}

} // end anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
