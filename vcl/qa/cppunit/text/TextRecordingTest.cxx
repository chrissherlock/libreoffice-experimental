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

    // Verify IsActive logic uses the state pointers
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

    // Verify line index tracking
    OUString aText("Visual Line Test");
    aData.m_aDisplayText = aText;

    // bStartVisualLine = true should push current length
    aAcc.Record(*Application::GetDefaultDevice(), Point(0, 0), aText, 0, aText.getLength(), nullptr,
                true);

    CPPUNIT_ASSERT_EQUAL(size_t(1), aData.m_aLineIndices.size());
}

CPPUNIT_TEST_FIXTURE(VclTextRecordingTest, testOutputDeviceIntegration)
{
    // Test the unified logic inside OutputDevice::DrawText
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    std::vector<tools::Rectangle> aRects;
    OUString aText("Integration Test");

    // This triggers the temporary state logic we refactored
    pVDev->DrawText(Point(0, 0), aText, 0, aText.getLength(), &aRects);

    CPPUNIT_ASSERT_MESSAGE("Unified DrawText should have populated rectangles", !aRects.empty());
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
