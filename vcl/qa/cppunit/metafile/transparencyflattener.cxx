/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <test/bootstrapfixture.hxx>
#include <tools/poly.hxx>

#include <vcl/virdev.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/metafile/TransparencyFlattener.hxx>

using namespace vcl::metafile;

class TransparencyFlattenerTest : public CppUnit::TestFixture
{
};

CPPUNIT_TEST_FIXTURE(TransparencyFlattenerTest, testFlattenSimpleIntersection)
{
    // Initialize without alpha to bypass macOS CoreGraphics defaults
    ScopedVclPtrInstance<VirtualDevice> pRefDev(DeviceFormat::WITHOUT_ALPHA);
    pRefDev->SetOutputSizePixel(Size(1000, 1000));

    GDIMetaFile aInputMtf;
    pRefDev->SetConnectMetaFile(&aInputMtf);

    pRefDev->SetFillColor(COL_WHITE);
    pRefDev->DrawRect(tools::Rectangle(0, 0, 500, 500));

    tools::PolyPolygon aPoly(tools::Rectangle(250, 250, 750, 750));
    pRefDev->DrawTransparent(aPoly, 50 /* 50% transparent */);

    pRefDev->SetConnectMetaFile(nullptr);
    aInputMtf.WindStart();

    FlatteningOptions aOpts;
    aOpts.nMaxBmpDPIX = 300;
    aOpts.nMaxBmpDPIY = 300;
    aOpts.aBackground = COL_WHITE;

#ifdef MACOSX
    aOpts.bReduceTransparency = true;
    aOpts.bTransparencyAutoMode = false;
#else
    aOpts.bReduceTransparency = false;
    aOpts.bTransparencyAutoMode = true;
#endif

    GDIMetaFile aOutputMtf;
    bool bWasFlattened = TransparencyFlattener::Flatten(aInputMtf, aOutputMtf, *pRefDev, aOpts);

    CPPUNIT_ASSERT_MESSAGE("Flattener should have detected and processed transparencies",
                           bWasFlattened);

    bool bHasTransparentAction = false;
    for (size_t i = 0; i < aOutputMtf.GetActionSize(); ++i)
    {
        if (aOutputMtf.GetAction(i)->GetType() == MetaActionType::Transparent)
            bHasTransparentAction = true;
    }

    CPPUNIT_ASSERT_MESSAGE("Output metafile must not contain transparent actions",
                           !bHasTransparentAction);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
