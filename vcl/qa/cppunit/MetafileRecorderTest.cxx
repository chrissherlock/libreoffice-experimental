/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>

#include <tools/color.hxx>

#include <vcl/virdev.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetaActionType.hxx>

#include <metafile/MetafileRecorder.hxx>

using namespace ::com::sun::star;

class MetafileRecorderTest : public test::BootstrapFixture
{
public:
    MetafileRecorderTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(MetafileRecorderTest, testBitmapActionDispatch)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    GDIMetaFile aMtf;
    pDev->SetConnectMetaFile(&aMtf);

    vcl::MetafileRecorder aRecorder(*pDev);

    Point aPt(10, 10);
    Size aSz(50, 50);
    Bitmap aBmp(aSz, vcl::PixelFormat::N24_BPP);

    // Test 1: Simple BMP
    aRecorder.RecordBitmapAction(MetaActionType::BMP, aPt, aSz, aPt, aSz, aBmp);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aMtf.GetActionSize());
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMP, aMtf.GetAction(0)->GetType());

    // Test 2: Scale BMP
    aRecorder.RecordBitmapAction(MetaActionType::BMPSCALE, aPt, aSz, aPt, aSz, aBmp);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aMtf.GetActionSize());
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPSCALE, aMtf.GetAction(1)->GetType());

    // Test 3: Scale Part
    aRecorder.RecordBitmapAction(MetaActionType::BMPSCALEPART, aPt, aSz, aPt, aSz, aBmp);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), aMtf.GetActionSize());
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPSCALEPART, aMtf.GetAction(2)->GetType());
}

CPPUNIT_TEST_FIXTURE(MetafileRecorderTest, testTextLineAction)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    GDIMetaFile aMtf;
    pDev->SetConnectMetaFile(&aMtf);

    vcl::MetafileRecorder aRecorder(*pDev);

    Point aPt(10, 10);
    // Use LINESTYLE_SINGLE instead of LINESTYLE_SOLID
    aRecorder.RecordTextLine(aPt, 100, STRIKEOUT_SINGLE, LINESTYLE_SINGLE, LINESTYLE_NONE);

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aMtf.GetActionSize());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::TEXTLINE, pAction->GetType());

    auto* pTextLine = static_cast<MetaTextLineAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(STRIKEOUT_SINGLE, pTextLine->GetStrikeout());
    // Use GetUnderline() instead of GetUnderlineStyle()
    CPPUNIT_ASSERT_EQUAL(LINESTYLE_SINGLE, pTextLine->GetUnderline());
}

CPPUNIT_TEST_FIXTURE(MetafileRecorderTest, testScopedSuspend)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    GDIMetaFile aMtf;
    pDev->SetConnectMetaFile(&aMtf); // Start Recording

    // 1. Record initial action
    {
        vcl::MetafileRecorder(*pDev).RecordComment("Start");
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aMtf.GetActionSize());
    }

    // 2. Activate ScopedSuspend
    {
        vcl::MetafileRecorder::ScopedSuspend aSuspend(*pDev);

        // Try to record - should be ignored
        vcl::MetafileRecorder(*pDev).RecordComment("Ignored");

        // Verify underlying device state
        CPPUNIT_ASSERT(pDev->GetConnectMetaFile() == nullptr);

        // Assert nothing added to original mtf
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aMtf.GetActionSize());
    }
    // Destructor of ScopedSuspend runs here

    // 3. Verify Restoration
    CPPUNIT_ASSERT(pDev->GetConnectMetaFile() == &aMtf);

    // 4. Record again
    vcl::MetafileRecorder(*pDev).RecordComment("End");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aMtf.GetActionSize());
}

CPPUNIT_TEST_FIXTURE(MetafileRecorderTest, testScopedSwitch)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;

    GDIMetaFile aPrimaryMtf;
    GDIMetaFile aSecondaryMtf;

    pDev->SetConnectMetaFile(&aPrimaryMtf);

    // 1. Record to Primary
    vcl::MetafileRecorder(*pDev).RecordComment("Primary1");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aPrimaryMtf.GetActionSize());

    // 2. Switch to Secondary
    {
        vcl::MetafileRecorder::ScopedSwitch aSwitch(*pDev, &aSecondaryMtf);

        // Assert OutputDevice points to secondary
        CPPUNIT_ASSERT(pDev->GetConnectMetaFile() == &aSecondaryMtf);

        // Record - should go to secondary
        vcl::MetafileRecorder(*pDev).RecordComment("Secondary1");

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                             aPrimaryMtf.GetActionSize()); // Primary unchanged
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
                             aSecondaryMtf.GetActionSize()); // Secondary grew
    }
    // Destructor of ScopedSwitch runs here

    // 3. Verify Restoration
    CPPUNIT_ASSERT(pDev->GetConnectMetaFile() == &aPrimaryMtf);

    // 4. Record to Primary again
    vcl::MetafileRecorder(*pDev).RecordComment("Primary2");
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aPrimaryMtf.GetActionSize());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aSecondaryMtf.GetActionSize());
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
