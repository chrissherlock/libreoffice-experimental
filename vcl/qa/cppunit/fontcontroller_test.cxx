/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>

#include <vcl/font.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>

#include <FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontCollection.hxx>

using namespace vcl;
using namespace vcl::font;

namespace
{
class VclFontControllerTest : public test::BootstrapFixture
{
public:
    VclFontControllerTest()
        : BootstrapFixture(true, false)
    {
    }

    void testGetTextLayoutFlags();
    void testCalculateTextOffsets();
    void testNeedsUpdate();
    void testCreateFontInstance();

    CPPUNIT_TEST_SUITE(VclFontControllerTest);
    CPPUNIT_TEST(testGetTextLayoutFlags);
    CPPUNIT_TEST(testCalculateTextOffsets);
    CPPUNIT_TEST(testNeedsUpdate);
    CPPUNIT_TEST(testCreateFontInstance);
    CPPUNIT_TEST_SUITE_END();
};

void VclFontControllerTest::testGetTextLayoutFlags()
{
    FontController aController;
    vcl::Font aFont;

    // Test Case 1: Plain Font
    aFont.SetFamilyName("Liberation Sans");
    auto[bLines, bSpecial] = aController.GetTextLayoutFlags(aFont);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Plain font should have no line decorations", false, bLines);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Plain font should have no special effects", false, bSpecial);

    // Test Case 2: Underline
    aFont.SetUnderline(LINESTYLE_SINGLE);
    std::tie(bLines, bSpecial) = aController.GetTextLayoutFlags(aFont);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Underlined font should trigger line decorations", true, bLines);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Underline is not a 'special' effect", false, bSpecial);

    // Test Case 3: Shadow
    aFont.SetShadow(true);
    std::tie(bLines, bSpecial) = aController.GetTextLayoutFlags(aFont);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Shadowed font should trigger special effects", true, bSpecial);
}

void VclFontControllerTest::testCalculateTextOffsets()
{
    FontController aController;
    vcl::Font aFont;

    // Logic should handle null LogicalFontInstance safely
    auto[nX, nY, nAsc, nDesc] = aController.CalculateTextOffsets(aFont, nullptr);

    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(0), nX);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(0), nY);
}

void VclFontControllerTest::testNeedsUpdate()
{
    FontController aController;
    vcl::Font aFont("Liberation Sans", Size(0, 12));

    // Initially, there is no font instance, so an update is required
    CPPUNIT_ASSERT_EQUAL_MESSAGE("New controller must need update", true,
                                 aController.NeedsUpdate(aFont, false));
}

void VclFontControllerTest::testCreateFontInstance()
{
    ScopedVclPtr<VirtualDevice> pVDev = VclPtr<VirtualDevice>::Create();
    CPPUNIT_ASSERT(pVDev);

    FontController aController;

    // We use the VirtualDevice to provide the FontCollection.
    // In headless mode, this might rely on generic fallbacks, which is what we want to test.
    vcl::font::PhysicalFontCollection* pPFC = pVDev->GetFontCollection();

    // If pPFC is null (headless environment often has no fonts initially),
    // we force an initialization via the device to trigger the fallback logic.
    if (!pPFC)
    {
        // Trigger internal initialization by asking for a metric
        pVDev->GetFontMetric(vcl::Font("Liberation Sans", Size(0, 12)));
        pPFC = pVDev->GetFontCollection();
    }

    if (!pPFC)
    {
        printf("Skipping testCreateFontInstance: No PhysicalFontCollection available.\n");
        return;
    }

    vcl::Font aFont("Liberation Sans", Size(0, 12));
    SalGraphics* pGraphics = pVDev->GetGraphics();
    CoordinateMapper aMapper(pVDev.get());
    long nDPIY = 96;
    AntialiasingFlags eAA = AntialiasingFlags::Enable;
    StyleSettings aStyle;

    rtl::Reference<LogicalFontInstance> pInstance
        = aController.CreateFontInstance(pPFC, aFont, pGraphics, aMapper, nDPIY, eAA, aStyle);

    CPPUNIT_ASSERT_MESSAGE("CreateFontInstance returned null", pInstance.is());
}

} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(VclFontControllerTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
