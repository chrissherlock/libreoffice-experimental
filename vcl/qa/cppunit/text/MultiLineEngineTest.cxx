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
#include <vcl/virdev.hxx>
#include <vcl/font.hxx>

#include <textlayout.hxx>
#include <textlineinfo.hxx>

using namespace vcl::text;

namespace
{
class MockTextLayout : public vcl::TextLayoutCommon
{
public:
    long mnMaxTextWidth = 50;
    int mnLinesToCreate = 1;
    OUString maEllipsisResult = "MOCKED...";

    virtual long GetTextLines(tools::Rectangle const&, long, ImplMultiTextLineInfo& rLineInfo, long,
                              OUString const&, DrawTextFlags) const override
    {
        for (int i = 0; i < mnLinesToCreate; ++i)
        {
            // ImplTextLineInfo(long nWidth, sal_Int32 nIndex, sal_Int32 nLen)
            rLineInfo.AddLine(ImplTextLineInfo(50, 0, 5));
        }
        return mnMaxTextWidth;
    }

    virtual OUString GetEllipsisString(OUString const&, long, DrawTextFlags) const override
    {
        return maEllipsisResult;
    }

    // --- Pure Virtual Stubs (Must match textlayout.hxx exactly) ---

    virtual long GetTextWidth(OUString const&, int, int) const override { return 0; }

    // Removed 'const' to match interface
    virtual void DrawText(Point const&, OUString const&, int, int, std::vector<tools::Rectangle>*,
                          OUString*) override
    {
    }

    virtual bool DecomposeTextRectAction() const override { return false; }

    // Changed return type from double to tools::Long
    virtual tools::Long GetTextArray(OUString const&, KernArray*, int, int, bool) const override
    {
        return 0;
    }

    // Missing method implementation
    virtual sal_Int32 GetTextBreak(const OUString&, tools::Long, sal_Int32,
                                   sal_Int32) const override
    {
        return 0;
    }
};

class MultiLineEngineTest : public test::BootstrapFixture
{
public:
    MultiLineEngineTest()
        : BootstrapFixture(true, false)
    {
    }
    long MockGetTextWidth(const OUString& rStr) { return rStr.getLength() * 10; }
};

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_NoTruncation)
{
    OUString aInput = "Hello";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 100, DrawTextFlags::EndEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });
    CPPUNIT_ASSERT_EQUAL(aInput, aResult);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_EndEllipsis)
{
    OUString aInput = "Hello World";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 80, DrawTextFlags::EndEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });
    CPPUNIT_ASSERT(aResult.endsWith("..."));
    CPPUNIT_ASSERT(MockGetTextWidth(aResult) <= 80);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_CenterEllipsis)
{
    OUString aInput = "A very long string indeed";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 100, DrawTextFlags::CenterEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });
    CPPUNIT_ASSERT(aResult.indexOf("...") != -1);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_PathEllipsis)
{
    OUString aInput = "/usr/local/bin/libreoffice";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 170, DrawTextFlags::PathEllipsis,
        [this](const OUString& s) { return MockGetTextWidth(s); });
    CPPUNIT_ASSERT(aResult.startsWith("/"));
    CPPUNIT_ASSERT(aResult.indexOf("...") != -1);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetEllipsisString_ClipFallback)
{
    OUString aInput = "Hello";
    OUString aResult = MultiLineEngine::GetEllipsisString(
        aInput, 10, DrawTextFlags::EndEllipsis | DrawTextFlags::Clip,
        [this](const OUString& s) { return MockGetTextWidth(s); });
    CPPUNIT_ASSERT_EQUAL(OUString("H"), aResult);
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testCalculateMultiLineLayout_Fits)
{
    MockTextLayout aMockLayout;
    aMockLayout.mnLinesToCreate = 1;
    MultiLineLayout aRes;

    MultiLineEngine::CalculateMultiLineLayout(aMockLayout, aRes, tools::Rectangle(0, 0, 100, 20),
                                              10, 100, 20, "Test", DrawTextFlags::NONE);

    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aRes.nFormatLines);
    CPPUNIT_ASSERT(!(aRes.nResultStyle & DrawTextFlags::Clip));
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testCalculateMultiLineLayout_ClipVertical)
{
    MockTextLayout aMockLayout;
    aMockLayout.mnLinesToCreate = 3;
    MultiLineLayout aRes;

    MultiLineEngine::CalculateMultiLineLayout(aMockLayout, aRes, tools::Rectangle(0, 0, 100, 20),
                                              10, 100, 20, "Test", DrawTextFlags::NONE);

    CPPUNIT_ASSERT_EQUAL(sal_Int32(3), aRes.nFormatLines);
    CPPUNIT_ASSERT(bool(aRes.nResultStyle & DrawTextFlags::Clip));
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testCalculateMultiLineLayout_EndEllipsis)
{
    MockTextLayout aMockLayout;
    aMockLayout.mnLinesToCreate = 3;
    MultiLineLayout aRes;

    MultiLineEngine::CalculateMultiLineLayout(aMockLayout, aRes, tools::Rectangle(0, 0, 100, 20),
                                              10, 100, 20, "Test", DrawTextFlags::EndEllipsis);

    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aRes.nFormatLines);
    CPPUNIT_ASSERT_EQUAL(OUString("MOCKED..."), aRes.aLastLine);
    CPPUNIT_ASSERT(bool(aRes.nResultStyle & DrawTextFlags::Top));
}

CPPUNIT_TEST_FIXTURE(MultiLineEngineTest, testGetTextBreak_Integration)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    vcl::Font aFont(u"Liberation Sans"_ustr, Size(0, 12));
    pVDev->SetFont(aFont);

    OUString aText = "Hello World";
    long nWidth = pVDev->GetTextWidth(aText);
    sal_Int32 nBreak = pVDev->GetTextBreak(aText, nWidth / 2, 0);

    CPPUNIT_ASSERT(nBreak > 0);
    CPPUNIT_ASSERT(nBreak < aText.getLength());
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
