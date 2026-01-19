/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <cppunit/TestAssert.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/fontcapabilities.hxx>
#include <vcl/metric.hxx>

#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/FontController.hxx>
#include <CoordinateMapper.hxx>

using namespace vcl::font;

class FontControllerTest : public test::BootstrapFixture
{
public:
    FontControllerTest()
        : BootstrapFixture(true, false)
    {
    }

    void testCreateFontInstance()
    {
        ScopedVclPtr<VirtualDevice> pVDev = VclPtr<VirtualDevice>::Create();
        CPPUNIT_ASSERT(pVDev);

        //  Discover a Valid Font Name via Metric Resolution
        // Instead of iterating, we ask the device: "What would you use for 'Default'?"
        vcl::Font aRefFont("Default", Size(0, 12));
        FontMetric aMetric = pVDev->GetFontMetric(aRefFont);

        OUString sFontName = aMetric.GetFamilyName();

        // obustness: If the system has NO fonts, it might return empty or "Default".
        // In that case, we can't really test specific realization, so we check for basic sanity.
        PhysicalFontCollection* pPFC = pVDev->GetFontCollection();

        if (!pPFC || pPFC->Count() == 0)
        {
            printf(
                "Skipping testCreateFontInstance: No fonts available in headless environment.\n");
            return;
        }

        // If sFontName is empty, try a fallback like "Liberation Sans" or "Arial" just in case.
        if (sFontName.isEmpty())
            sFontName = "Liberation Sans";

        FontController aController;
        vcl::Font aFont(sFontName, Size(0, 12));
        SalGraphics* pGraphics = pVDev->GetGraphics();
        CoordinateMapper aMapper; // Default identity map
        long nDPIY = 96;
        AntialiasingFlags eAA = AntialiasingFlags::Enable;
        StyleSettings aStyle;

        rtl::Reference<LogicalFontInstance> pInstance
            = aController.CreateFontInstance(pPFC, aFont, pGraphics, aMapper, nDPIY, eAA, aStyle);

        CPPUNIT_ASSERT_MESSAGE("CreateFontInstance returned null", pInstance.is());
    }

    CPPUNIT_TEST_SUITE(FontControllerTest);
    CPPUNIT_TEST(testCreateFontInstance);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FontControllerTest);
