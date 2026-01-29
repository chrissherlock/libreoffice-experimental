/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>

#include <vcl/svapp.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/vcllayout.hxx>
#include <vcl/text/TextRecordingState.hxx>
#include <vcl/text/TextRecordingDispatcher.hxx>
#include <vcl/text/TextLayoutData.hxx>

#include <text/TextLayoutEngine.hxx> // For TextSpan/LayoutConstraints

using namespace vcl::text;

class VclTextRecordingDispatcherTest : public test::BootstrapFixture
{
public:
    VclTextRecordingDispatcherTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(VclTextRecordingDispatcherTest, testDispatcherRouting)
{
    TextRecordingState aState;
    std::vector<tools::Rectangle> aMeasuredRects;
    vcl::Region aClip(tools::Rectangle(Point(0, 0), Size(1000, 1000)));

    aState.mpMeasurementVector = &aMeasuredRects;
    aState.mpMeasurementClip = &aClip;

    ScopedVclPtrInstance<VirtualDevice> pVDev;
    OUString aText(u"Dispatcher Test"_ustr);

    std::unique_ptr<SalLayout> pLayout = pVDev->LayoutText(
        vcl::text::TextSpan{ aText, 0, aText.getLength() },
        vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

    CPPUNIT_ASSERT_MESSAGE("Layout creation failed", pLayout);

    TextRecordingDispatcher::Dispatch(aState, *pVDev, aText, 0, aText.getLength(), pLayout.get());

    CPPUNIT_ASSERT_MESSAGE("Dispatcher failed to route to MeasurementRecorder",
                           !aMeasuredRects.empty());
    CPPUNIT_ASSERT_EQUAL(size_t(aText.getLength()), aMeasuredRects.size());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingDispatcherTest, testDispatcherSafetyWithNullLayout)
{
    TextRecordingState aState;
    std::vector<tools::Rectangle> aMeasuredRects;
    aState.mpMeasurementVector = &aMeasuredRects;

    ScopedVclPtrInstance<VirtualDevice> pVDev;
    OUString aText(u"Safety Test"_ustr);

    TextRecordingDispatcher::Dispatch(aState, *pVDev, aText, 0, aText.getLength(), nullptr);

    CPPUNIT_ASSERT_MESSAGE("Dispatcher should imply no-op on null layout", aMeasuredRects.empty());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingDispatcherTest, testDispatcherAccessibilityRouting)
{
    TextRecordingState aState;
    vcl::text::TextLayoutData aData;
    aState.mpLayoutData = &aData;
    aState.maRecordRect = tools::Rectangle(Point(0, 0), Size(1000, 1000));

    ScopedVclPtrInstance<VirtualDevice> pVDev;
    OUString aText(u"Accessibility Route"_ustr);

    std::unique_ptr<SalLayout> pLayout = pVDev->LayoutText(
        vcl::text::TextSpan{ aText, 0, aText.getLength() },
        vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

    TextRecordingDispatcher::Dispatch(aState, *pVDev, aText, 0, aText.getLength(), pLayout.get(),
                                      true);

    CPPUNIT_ASSERT_MESSAGE("Dispatcher failed to route to AccessibilityRecorder",
                           !aData.m_aUnicodeBoundRects.empty());

    if (!aData.m_aLineIndices.empty())
        CPPUNIT_ASSERT_EQUAL(size_t(1), aData.m_aLineIndices.size());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
