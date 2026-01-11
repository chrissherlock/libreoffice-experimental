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

#include <vcl/virdev.hxx>

#include "fontmocks.hxx"

class VclFontSelectionTest : public test::BootstrapFixture
{
public:
    VclFontSelectionTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(VclFontSelectionTest, testFontRealizationCycle)
{
    ScopedVclPtr<VirtualDevice> pDev = VclPtr<VirtualDevice>::Create();

    vcl::Font aFont(u"TestFont"_ustr, Size(0, 20));
    pDev->SetFont(aFont);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Logical font name should be preserved", u"TestFont"_ustr,
                                 pDev->GetFont().GetFamilyName());

    tools::Long nHeight = pDev->GetTextHeight();
    CPPUNIT_ASSERT_MESSAGE("Realized text height should be positive", nHeight > 0);

    aFont.SetWeight(FontWeight::WEIGHT_BOLD);
    pDev->SetFont(aFont);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Updated weight should be reflected in the device",
                                 FontWeight::WEIGHT_BOLD, pDev->GetFont().GetWeight());
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
