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

#include <tools/degree.hxx>

#include <vcl/font.hxx>

#include <font/FontMetricResolver.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/FontMetricData.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontFace.hxx>

namespace
{
/**
 * A dummy physical font face to satisfy the LogicalFontInstance constructor.
 */
class StubFontFace : public vcl::font::PhysicalFontFace
{
public:
    StubFontFace(const vcl::font::FontSelectPattern& rPattern)
        : vcl::font::PhysicalFontFace(rPattern)
    {
    }

    virtual rtl::Reference<LogicalFontInstance>
    CreateFontInstance(const vcl::font::FontSelectPattern&) const override
    {
        return nullptr;
    }
    virtual sal_IntPtr GetFontId() const override { return 0; }
};

/**
 * A lightweight stub to simulate a font instance without needing
 * to load the actual FreeType/CoreText backends.
 */
class StubFontInstance : public LogicalFontInstance
{
public:
    // Pass the dummy face in by reference so it is fully constructed beforehand
    StubFontInstance(const vcl::font::PhysicalFontFace& rFace,
                     const vcl::font::FontSelectPattern& rPattern)
        : LogicalFontInstance(rFace, rPattern)
    {
        // Mock the native font metric data
        mxFontMetric = new FontMetricData(rPattern);

        // Let's pretend our physical font file only supports 0 degrees (no native rotation)
        mxFontMetric->SetOrientation(Degree10(0));

        // Set some dummy ascent/descent for line height calculation
        mxFontMetric->SetAscent(100);
        mxFontMetric->SetDescent(25);
    }

    // Satisfy the pure virtual method required by the base class
    virtual bool GetGlyphOutline(sal_GlyphId, basegfx::B2DPolyPolygon&, bool) const override
    {
        return false;
    }
};

} // namespace

class FontMetricResolverTest : public CppUnit::TestFixture
{
};

CPPUNIT_TEST_FIXTURE(FontMetricResolverTest, testResolveStandardMetrics)
{
    // Initialize a dummy pattern and face
    vcl::font::FontSelectPattern aPattern(vcl::Font(), OUString(), Size(10, 10), 10.0f);
    rtl::Reference<StubFontFace> xFace(new StubFontFace(aPattern));
    rtl::Reference<StubFontInstance> xInstance(new StubFontInstance(*xFace, aPattern));

    vcl::font::DeviceFontCapabilities aCaps;

    vcl::font::FontMetricResolver::ResolveMetrics(aCaps, xInstance.get());

    // Ascent (100) + Descent (25) = 125
    CPPUNIT_ASSERT_EQUAL(tools::Long(125), xInstance->mnLineHeight);
}

CPPUNIT_TEST_FIXTURE(FontMetricResolverTest, testSyntheticOrientationAllowed)
{
    vcl::font::FontSelectPattern aPattern(vcl::Font(), OUString(), Size(10, 10), 10.0f);

    // The user requests a font rotated at 45 degrees
    aPattern.mnOrientation = Degree10(450);

    rtl::Reference<StubFontFace> xFace(new StubFontFace(aPattern));
    rtl::Reference<StubFontInstance> xInstance(new StubFontInstance(*xFace, aPattern));

    // The device IS capable of synthetic typography (e.g., VirtualDevice/Screen)
    vcl::font::DeviceFontCapabilities aCaps;
    aCaps.bSupportsGlyphSynthesis = true;

    vcl::font::FontMetricResolver::ResolveMetrics(aCaps, xInstance.get());

    // Use direct boolean assertions to avoid the Degree10 operator<< printing error
    CPPUNIT_ASSERT_MESSAGE("Resolver should synthesize orientation",
                           xInstance->mnOwnOrientation == Degree10(450));
    CPPUNIT_ASSERT_MESSAGE("Final orientation should reflect synthesized rotation",
                           xInstance->mnOrientation == Degree10(450));
}

CPPUNIT_TEST_FIXTURE(FontMetricResolverTest, testSyntheticOrientationBlocked)
{
    vcl::font::FontSelectPattern aPattern(vcl::Font(), OUString(), Size(10, 10), 10.0f);
    aPattern.mnOrientation = Degree10(450);

    rtl::Reference<StubFontFace> xFace(new StubFontFace(aPattern));
    rtl::Reference<StubFontInstance> xInstance(new StubFontInstance(*xFace, aPattern));

    // The device is NOT capable of synthetic typography (e.g., Physical Printer)
    vcl::font::DeviceFontCapabilities aCaps;
    aCaps.bSupportsGlyphSynthesis = false;

    vcl::font::FontMetricResolver::ResolveMetrics(aCaps, xInstance.get());

    CPPUNIT_ASSERT_MESSAGE("Resolver MUST NOT synthesize orientation for strict hardware",
                           xInstance->mnOwnOrientation == Degree10(0));
    CPPUNIT_ASSERT_MESSAGE("Final orientation must be strictly the native metric",
                           xInstance->mnOrientation == Degree10(0));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
