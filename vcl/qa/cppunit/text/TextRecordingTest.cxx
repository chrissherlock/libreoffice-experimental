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
#include <vcl/text/TextLayoutData.hxx>

#include <text/AccessibilityRecorder.hxx>
#include <text/MeasurementRecorder.hxx>

using namespace vcl::text;

class VclTextRecordingTest : public test::BootstrapFixture
{
public:
    VclTextRecordingTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testMeasurementLogic)
{
    TextRecordingState aState;
    std::vector<tools::Rectangle> aRects;
    aState.mpMeasurementVector = &aRects;

    MeasurementRecorder aMeas(aState);
    CPPUNIT_ASSERT_MESSAGE("MeasurementRecorder should be active", aMeas.IsActive());

    TextRecordingState aEmptyState;
    MeasurementRecorder aInactive(aEmptyState);
    CPPUNIT_ASSERT_MESSAGE("Should be inactive with null vector", !aInactive.IsActive());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testAccessibilityLogic)
{
    TextRecordingState aState;
    vcl::text::TextLayoutData aData;
    aState.mpLayoutData = &aData;

    AccessibilityRecorder aAcc(aState);
    CPPUNIT_ASSERT(aAcc.IsActive());

    OUString aText("Visual Line Test");
    aData.m_aDisplayText = aText;

    // bStartVisualLine = true should push current length
    aAcc.Record(*Application::GetDefaultDevice(), Point(0, 0), aText, 0, aText.getLength(), nullptr,
                true);

    CPPUNIT_ASSERT_EQUAL(size_t(1), aData.m_aLineIndices.size());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testAccessibilityDataPayload)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    TextRecordingState aState;
    vcl::text::TextLayoutData aData;
    aState.mpLayoutData = &aData;
    aState.maRecordRect = tools::Rectangle(Point(0, 0), Size(1000, 1000));

    AccessibilityRecorder aAcc(aState);
    OUString aText("Test Payload");

    // Layout the text to get real glyph positions
    std::unique_ptr<SalLayout> pLayout = pVDev->LayoutText(
        vcl::text::TextSpan{ aText, 0, aText.getLength() },
        vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

    CPPUNIT_ASSERT(pLayout);

    aAcc.Record(*pVDev, Point(0, 0), aText, 0, aText.getLength(), pLayout.get(), true);

    // Verify coordinates and text were actually captured
    CPPUNIT_ASSERT_EQUAL(aText, aData.m_aDisplayText);
    CPPUNIT_ASSERT_MESSAGE("Unicode bound rects should be populated",
                           !aData.m_aUnicodeBoundRects.empty());
    CPPUNIT_ASSERT_EQUAL(size_t(aText.getLength()), aData.m_aUnicodeBoundRects.size());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testOutputDeviceIntegration)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    std::vector<tools::Rectangle> aRects;
    OUString aText("Integration Test");

    // Triggers the temporary state logic in DrawText
    pVDev->DrawText(Point(0, 0), aText, 0, aText.getLength(), &aRects);

    CPPUNIT_ASSERT_MESSAGE("Unified DrawText should have populated rectangles", !aRects.empty());
}

CPPUNIT_PLUGIN_IMPLEMENT();

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testAccessibilityClipping)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    TextRecordingState aState;
    vcl::text::TextLayoutData aData;
    aState.mpLayoutData = &aData;

    // Define a recording area that only covers the first few characters
    // Assuming standard font width, a 20x1000 rect should clip most of "Test Payload"
    aState.maRecordRect = tools::Rectangle(Point(0, 0), Size(20, 1000));

    AccessibilityRecorder aAcc(aState);
    OUString aText("Test Payload");

    std::unique_ptr<SalLayout> pLayout = pVDev->LayoutText(
        vcl::text::TextSpan{ aText, 0, aText.getLength() },
        vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

    CPPUNIT_ASSERT(pLayout);

    // Record with the restrictive maRecordRect
    aAcc.Record(*pVDev, Point(0, 0), aText, 0, aText.getLength(), pLayout.get(), false);

    // Verify that clipping occurred
    // Since the rect is small, we expect fewer rectangles than the total length of the string
    CPPUNIT_ASSERT_MESSAGE("Unicode bound rects should be clipped/filtered",
                           aData.m_aUnicodeBoundRects.size()
                               < static_cast<size_t>(aText.getLength()));

    // Ensure we didn't lose everything; at least the first 'T' should be there
    CPPUNIT_ASSERT(!aData.m_aUnicodeBoundRects.empty());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testMeasurementDataPayload)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    TextRecordingState aState;
    std::vector<tools::Rectangle> aRects;
    OUString aDisplayText;
    vcl::Region aClip(tools::Rectangle(Point(0, 0), Size(1000, 1000)));

    // Link the state to our local targets
    aState.mpMeasurementVector = &aRects;
    aState.mpMeasurementString = &aDisplayText;
    aState.mpMeasurementClip = &aClip;

    MeasurementRecorder aMeas(aState);
    OUString aText("Measure Me");

    std::unique_ptr<SalLayout> pLayout = pVDev->LayoutText(
        vcl::text::TextSpan{ aText, 0, aText.getLength() },
        vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

    CPPUNIT_ASSERT(pLayout);

    // Execute the recording
    aMeas.Record(*pVDev, Point(0, 0), aText, 0, aText.getLength(), pLayout.get());

    // Verify the data was captured through the state pointers
    CPPUNIT_ASSERT_EQUAL(aText, aDisplayText);
    CPPUNIT_ASSERT_MESSAGE("Measurement rects should be populated", !aRects.empty());
    CPPUNIT_ASSERT_EQUAL(size_t(aText.getLength()), aRects.size());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testMeasurementInactiveHandling)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    TextRecordingState aState; // No vector linked, IsActive() will be false

    MeasurementRecorder aMeas(aState);
    OUString aText("Should Not Record");

    // Attempt to record with an inactive state
    aMeas.Record(*pVDev, Point(0, 0), aText, 0, aText.getLength(), nullptr);

    // If active check works, no crash occurs even with null layout
    CPPUNIT_ASSERT(!aMeas.IsActive());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
