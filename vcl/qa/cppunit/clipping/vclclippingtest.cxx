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
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/wintypes.hxx>

#include <clipping.hxx>

namespace
{
// A mock window to force a real paint event, which is the ONLY time
// clipToPaintRegion actually applies its intersection logic.
class PaintTestWindow : public WorkWindow
{
public:
    tools::Rectangle maOutsideRect;
    tools::Rectangle maPartialRect;

    PaintTestWindow()
        : WorkWindow(nullptr, WB_STDWORK)
    {
        SetOutputSizePixel(Size(100, 100));
    }

    // Hook into the system paint event
    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        // Inside Paint(), mbInPaint is TRUE and mpPaintRegion is valid.

        // 1. Outside Case
        maOutsideRect = tools::Rectangle(200, 200, 300, 300);
        vcl::clipping::clipToPaintRegion(rRenderContext, maOutsideRect);
        maOutsideRect.Normalize();

        // 2. Partial Case
        maPartialRect = tools::Rectangle(50, 50, 150, 150);
        vcl::clipping::clipToPaintRegion(rRenderContext, maPartialRect);
        maPartialRect.Normalize();
    }
};

// Use BootstrapFixture to prevent DeInitVCL leaks
CPPUNIT_TEST_FIXTURE(test::BootstrapFixture, testClippingStateTransitions)
{
    ScopedVclPtr<VirtualDevice> pVDev = VclPtr<VirtualDevice>::Create(DeviceFormat::WITHOUT_ALPHA);

    CPPUNIT_ASSERT_MESSAGE("Device should start in a Dirty state",
                           !pVDev->GetClipState().IsReady());

    vcl::clipping::initDeviceClipRegion(*pVDev);

    CPPUNIT_ASSERT_MESSAGE("Device should be Ready (Valid or Empty) after init",
                           pVDev->GetClipState().IsReady());
}

CPPUNIT_TEST_FIXTURE(test::BootstrapFixture, testActiveClipRegionRetrieval)
{
    ScopedVclPtr<VirtualDevice> pVDev = VclPtr<VirtualDevice>::Create(DeviceFormat::WITHOUT_ALPHA);
    pVDev->SetOutputSizePixel(Size(100, 100));

    vcl::Region aRegion = vcl::clipping::getActiveClipRegion(*pVDev);

    CPPUNIT_ASSERT_MESSAGE("Active region should not be null", !aRegion.IsNull());
}

CPPUNIT_TEST_FIXTURE(test::BootstrapFixture, testClipToPaintRegion)
{
    // 1. Create the owner window (BootstrapFixture prevents the DeInitVCL leak)
    VclPtr<WorkWindow> pOwnerWindow
        = VclPtr<WorkWindow>::Create(static_cast<vcl::Window*>(nullptr), WB_STDWORK);
    pOwnerWindow->SetOutputSizePixel(Size(100, 100));

    // 2. Extract the internal WindowOutputDevice.
    // We cannot construct this directly due to unexported VCL constructors.
    OutputDevice& rOutDev = *pOwnerWindow->GetOutDev();

    // 3. Outside Case
    tools::Rectangle aOutsideRect(200, 200, 300, 300);
    vcl::clipping::clipToPaintRegion(rOutDev, aOutsideRect);
    aOutsideRect.Normalize();

    // Since pImpl->mbInPaint is false outside of a Paint event, clipToPaintRegion safely returns early.
    // Assert that the rectangle was NOT modified or emptied.
    CPPUNIT_ASSERT_EQUAL(tools::Long(200), aOutsideRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(200), aOutsideRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(300), aOutsideRect.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(300), aOutsideRect.Bottom());

    // 4. Partial Case
    tools::Rectangle aPartialRect(50, 50, 150, 150);
    vcl::clipping::clipToPaintRegion(rOutDev, aPartialRect);
    aPartialRect.Normalize();

    // Assert it remains untouched
    CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPartialRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPartialRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(150), aPartialRect.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(150), aPartialRect.Bottom());

    // 5. Clean up the owner window to satisfy BootstrapFixture memory tracking
    pOwnerWindow.disposeAndClear();
}

} // end anonymous namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
