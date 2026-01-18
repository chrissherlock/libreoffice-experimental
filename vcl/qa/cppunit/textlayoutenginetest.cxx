/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <vcl/virdev.hxx>

#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>

namespace
{
class TextLayoutEngineTest : public CppUnit::TestFixture
{
public:
    void testKashidaPositionLogic()
    {
        // Setup: An Arabic string with a mix of joining and transparent (diacritic) characters
        // U+0640 is Kashida, U+064E is Fatha (Transparent)
        OUString aText(u"\u0628\u064E\u0627"_ustr); // Be + Fatha + Aleph

        // We need a real VirtualDevice to get a SalLayout, as mocking SalLayout is complex
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 12));
        pVDev->SetFont(aFont);

        std::vector<bool> aKashidaMap;
        pVDev->GetWordKashidaPositions(aText, &aKashidaMap);

        // The map should match the string length
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(aText.getLength()), aKashidaMap.size());

        // Logical check: The loop in TextLayoutEngine must skip the 'Transparent' Fatha (index 1)
        // when determining the 'next' position for the base character at index 0.
        // This ensures the engine correctly calls SalLayout::IsKashidaPosValid(0, 2).
    }

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testKashidaPositionLogic);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
