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
#include <cppunit/plugin/TestPlugIn.h>

#include <test/bootstrapfixture.hxx>
#include <vcl/text/MultiLineEngine.hxx>
#include <vcl/outdev.hxx>

using namespace vcl::text;

namespace
{
class MultiLineEngineTest : public test::BootstrapFixture
{
public:
    MultiLineEngineTest()
        : BootstrapFixture(true, false)
    {
    }

    // A simple width simulator: 1 char = 10 units
    long MockGetTextWidth(const OUString& rStr) { return rStr.getLength() * 10; }
};

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_NoTruncation)
{
    // "Hello" is 50 units wide. Max is 100. Should fit.
    OUString aInput = "Hello";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 100, DrawTextFlags::EndEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });

    CPPUNIT_ASSERT_EQUAL(aInput, aResult);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_EndEllipsis)
{
    // "Hello World" is 110 units. Max is 80.
    // "Hello..." is 80 units.
    OUString aInput = "Hello World";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 80, DrawTextFlags::EndEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });

    CPPUNIT_ASSERT(aResult.endsWith("..."));
    CPPUNIT_ASSERT(aResult.getLength() < aInput.getLength());
    CPPUNIT_ASSERT(MockGetTextWidth(aResult) <= 80);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_CenterEllipsis)
{
    // "A very long string indeed" -> "A ve...deed"
    OUString aInput = "A very long string indeed";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 100, DrawTextFlags::CenterEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });

    // Check format A...B
    CPPUNIT_ASSERT(aResult.startsWith("A"));
    CPPUNIT_ASSERT(aResult.endsWith("d"));
    CPPUNIT_ASSERT(aResult.indexOf("...") != -1);
    CPPUNIT_ASSERT(MockGetTextWidth(aResult) <= 100);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_PathEllipsis)
{
    // Path ellipsis preserves the file name after the last separator
    OUString aInput = "/usr/local/bin/libreoffice";
    // Width 260. Limit to 170 to ensure "/.../libreoffice" fits (Length 16 * 10 = 160).

    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 170, DrawTextFlags::PathEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });

    // Must preserve the filename
    CPPUNIT_ASSERT(aResult.endsWith("/libreoffice"));
    CPPUNIT_ASSERT(aResult.startsWith("/"));
    CPPUNIT_ASSERT(aResult.indexOf("...") != -1);
    CPPUNIT_ASSERT(MockGetTextWidth(aResult) <= 170);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_ClipFallback)
{
    // Tiny width, can't even fit "...". Should return clipped string "H" if Clip flag set.
    OUString aInput = "Hello";
    // 10 width = 1 char.
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 10, DrawTextFlags::EndEllipsis | DrawTextFlags::Clip,
        [this](const OUString& s) { return MockGetTextWidth(s); });

    CPPUNIT_ASSERT_EQUAL(OUString("H"), aResult);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
