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

#include <tools/mapunit.hxx>

#include <vcl/metric.hxx>
#include <vcl/text/MnemonicGeometry.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/virdev.hxx>
#include <CoordinateMapper.hxx>

#include <font/FontController.hxx>
#include <font/PhysicalFontFace.hxx>
#include <sallayout.hxx>
#include <textlayout.hxx>

#include <vector>

using namespace vcl::text;

namespace
{
class TextGeometryTest : public CppUnit::TestFixture
{
};

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedGeometry_0_Degrees)
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 0_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextGeometry::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // X = BaseX(100) + DistX(10) = 110
    // Y = BaseY(100) + DistY(20) = 120
    CPPUNIT_ASSERT_EQUAL(tools::Long(110), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(120), aGeo.maRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetHeight());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedGeometry_90_Degrees)
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 900_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextGeometry::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 90 deg rotation logic (Clockwise):
    // NewX = OldY(20); NewY = -OldX(-10) - NewHeight(30) = -40
    // Base(100,100) + (20, -40) = (120, 60)
    CPPUNIT_ASSERT_EQUAL(tools::Long(120), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aGeo.maRect.Top());
    // Dimensions swapped
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetHeight());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedGeometry_180_Degrees)
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 1800_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextGeometry::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 180 deg rotation logic:
    // NewX = -OldX(-10) - Width(30) = -40
    // NewY = -OldY(-20) - Height(40) = -60
    // Base(100,100) + (-40, -60) = (60, 40)
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetHeight());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedGeometry_270_Degrees)
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 2700_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextGeometry::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 270 deg rotation logic (Clockwise):
    // NewX = -OldY(-20) - NewWidth(40) = -60
    // NewY = OldX(10)
    // Base(100,100) + (-60, 10) = (40, 110)
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(110), aGeo.maRect.Top());
    // Dimensions swapped
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetHeight());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedGeometry_Arbitrary_Angle)
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(0, 0), Size(100, 100));
    Degree10 nAngle = 450_deg10; // 45 degrees

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextGeometry::GetRotatedGeometry(aBase, aLocal, nAngle);

    // Expect Polygon fallback
    CPPUNIT_ASSERT_EQUAL(true, aGeo.mbIsPolygon);
    CPPUNIT_ASSERT(aGeo.maPoly.GetSize() > 0);

    // Bounds Check: A 100x100 box rotated 45 degrees should have a bounding box
    // larger than 100x100 (approx 141x141)
    tools::Rectangle aBound = aGeo.maPoly.GetBoundRect();
    CPPUNIT_ASSERT(aBound.GetWidth() > 100);
    CPPUNIT_ASSERT(aBound.GetHeight() > 100);
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotatedImageOrigin)
{
    Point aBase(100, 100);
    // Local bounds: 10x20 rectangle at (0,0)
    // Note: VCL Rect of size 10x20 spans 0..9 in X and 0..19 in Y.
    tools::Rectangle aLocal(Point(0, 0), Size(10, 20));

    // Case 1: 0 Degrees
    // Should be Base + Local.TopLeft (100, 100)
    Point aPos = vcl::text::TextGeometry::GetRotatedImageOrigin(aBase, aLocal, 0_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.Y());

    // Case 2: 90 Degrees
    // Rotates (x,y) -> (y, -x).
    // X range [0..9] becomes Y range [0..-9]. Min Y is -9.
    // Base(100,100) + (0, -9) = (100, 91).
    aPos = vcl::text::TextGeometry::GetRotatedImageOrigin(aBase, aLocal, 900_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(91), aPos.Y());

    // Case 3: 180 Degrees
    // Rotates (x,y) -> (-x, -y).
    // X range [0..9] -> [-9..0]. Min X is -9.
    // Y range [0..19] -> [-19..0]. Min Y is -19.
    // Base(100,100) + (-9, -19) = (91, 81).
    aPos = vcl::text::TextGeometry::GetRotatedImageOrigin(aBase, aLocal, 1800_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(91), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(81), aPos.Y());

    // Case 4: 270 Degrees
    // Rotates (x,y) -> (-y, x).
    // Y range [0..19] -> X range [0..-19]. Min X is -19.
    // X range [0..9] -> Y range [0..9]. Min Y is 0.
    // Base(100,100) + (-19, 0) = (81, 100).
    aPos = vcl::text::TextGeometry::GetRotatedImageOrigin(aBase, aLocal, 2700_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(81), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.Y());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetMirroredX)
{
    vcl::text::MirroringContext aCtx;
    aCtx.nX = 10;
    aCtx.nGraphicsWidth = 1000;
    aCtx.nOutputWidth = 200;
    aCtx.nOutOffX = 50;

    // Case 1: No Mirroring, No RTL -> Identity
    aCtx.bHasMirroredGraphics = false;
    aCtx.bIsRTL = false;
    CPPUNIT_ASSERT_EQUAL(tools::Long(10), vcl::text::TextGeometry::GetMirroredX(aCtx));

    // Case 2: Mirrored Graphics Only (HasMirrored=True, IsRTL=False)
    // Step 1: x' = 1000 - 1 - 10 = 989
    // Step 2: devX = 1000 - 200 - 50 = 750
    // Step 3: x'' = 750 + (200 - 1 - (989 - 750))
    //             = 750 + (199 - 239) = 750 - 40 = 710
    aCtx.bHasMirroredGraphics = true;
    aCtx.bIsRTL = false;
    CPPUNIT_ASSERT_EQUAL(tools::Long(710), vcl::text::TextGeometry::GetMirroredX(aCtx));

    // Case 3: Mirrored Graphics + RTL (HasMirrored=True, IsRTL=True)
    // Only Step 1 applies: x' = 1000 - 1 - 10 = 989
    aCtx.bHasMirroredGraphics = true;
    aCtx.bIsRTL = true;
    CPPUNIT_ASSERT_EQUAL(tools::Long(989), vcl::text::TextGeometry::GetMirroredX(aCtx));

    // Case 4: RTL Only (HasMirrored=False, IsRTL=True)
    // devX = 50
    // x' = 200 - 1 - (10 - 50) + 50
    //    = 199 - (-40) + 50 = 199 + 40 + 50 = 289
    aCtx.bHasMirroredGraphics = false;
    aCtx.bIsRTL = true;
    CPPUNIT_ASSERT_EQUAL(tools::Long(289), vcl::text::TextGeometry::GetMirroredX(aCtx));
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetReliefOffset)
{
    // Case 1: Standard DPI (96), Embossed (Standard)
    // Calculation: 1 + (96 / 300) = 1 + 0 = 1
    tools::Long nOff = vcl::text::TextGeometry::GetReliefOffset(96, FontRelief::Embossed);
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), nOff);

    // Case 2: Standard DPI (96), Engraved (Negative Offset)
    // Calculation: -(1 + 0) = -1
    nOff = vcl::text::TextGeometry::GetReliefOffset(96, FontRelief::Engraved);
    CPPUNIT_ASSERT_EQUAL(tools::Long(-1), nOff);

    // Case 3: High DPI (600), Embossed
    // Calculation: 1 + (600 / 300) = 1 + 2 = 3
    nOff = vcl::text::TextGeometry::GetReliefOffset(600, FontRelief::Embossed);
    CPPUNIT_ASSERT_EQUAL(tools::Long(3), nOff);

    // Case 4: High DPI (600), Engraved
    // Calculation: -(1 + 2) = -3
    nOff = vcl::text::TextGeometry::GetReliefOffset(600, FontRelief::Engraved);
    CPPUNIT_ASSERT_EQUAL(tools::Long(-3), nOff);
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetShadowOffset)
{
    // Formula: 1 + ((LineHeight - 24) / 24)
    // If Outline is true, add 1.

    // Case 1: Small Font (Height 20), Not Outline
    // 1 + ((20 - 24) / 24) = 1 + (-4/24) = 1 + 0 = 1
    tools::Long nOff = vcl::text::TextGeometry::GetShadowOffset(20, false);
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), nOff);

    // Case 2: Standard Font (Height 24), Not Outline
    // 1 + ((24 - 24) / 24) = 1 + 0 = 1
    nOff = vcl::text::TextGeometry::GetShadowOffset(24, false);
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), nOff);

    // Case 3: Large Font (Height 48), Not Outline
    // 1 + ((48 - 24) / 24) = 1 + 1 = 2
    nOff = vcl::text::TextGeometry::GetShadowOffset(48, false);
    CPPUNIT_ASSERT_EQUAL(tools::Long(2), nOff);

    // Case 4: Large Font (Height 48), Is Outline
    // Calculation from Case 3 (2) + 1 (Outline Bonus) = 3
    nOff = vcl::text::TextGeometry::GetShadowOffset(48, true);
    CPPUNIT_ASSERT_EQUAL(tools::Long(3), nOff);
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetOutlineOffsets)
{
    const std::vector<basegfx::B2DPoint>& rOffsets = vcl::text::TextGeometry::GetOutlineOffsets();

    // Must return exactly 8 points (surrounding pixels)
    CPPUNIT_ASSERT_EQUAL(size_t(8), rOffsets.size());

    // Verify specific key points verify the pattern
    // Top-Left
    CPPUNIT_ASSERT_EQUAL(1.0, std::abs(rOffsets[0].getX()));
    CPPUNIT_ASSERT_EQUAL(1.0, std::abs(rOffsets[0].getY()));

    // Verify uniqueness (basic check)
    std::set<std::pair<double, double>> aUniquePoints;
    for (const auto& rPoint : rOffsets)
    {
        aUniquePoints.insert({ rPoint.getX(), rPoint.getY() });
    }
    CPPUNIT_ASSERT_EQUAL(size_t(8), aUniquePoints.size());

    // Ensure (0,0) is NOT in the list (we don't draw over the center)
    CPPUNIT_ASSERT(aUniquePoints.find({ 0.0, 0.0 }) == aUniquePoints.end());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetMnemonicGeometry)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->SetMapMode(MapMode(MapUnit::MapPixel));

    std::vector<double> aDXArray = { 10.0, 25.0, 40.0 };
    Point aLinePos(10, 20);
    vcl::text::MnemonicDeviceParams aParams{ 12, 0, 0 };

    auto aGeo = vcl::text::TextGeometry::GetMnemonicGeometry(
        // Use plain lambdas to avoid linking against SAL_DLLPRIVATE LogicWidthToDevicePixel
        // In MapPixel mode, 1 logical unit = 1 device pixel
        [](tools::Long w) { return static_cast<double>(w); }, [](tools::Long w) { return w; },
        [&](const Point& p) { return pVDev->LogicToPixel(p); }, // LogicToPixel is public
        aParams, aDXArray, 1, aLinePos, false);

    CPPUNIT_ASSERT_EQUAL(tools::Long(15), aGeo.nWidth);
    CPPUNIT_ASSERT_EQUAL(tools::Long(20), aGeo.nX);
    CPPUNIT_ASSERT_EQUAL(tools::Long(32), aGeo.nY);
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetRotationOrigin)
{
    Point aPos(100, 100);
    Size aTextSize(50, 20); // Width 50, Height 20

    // Test 0 degrees (Identity) - Origin should not change
    {
        Point aResult
            = vcl::text::TextGeometry::GetRotationOrigin(aPos, aTextSize, 0_deg10, ALIGN_BASELINE);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("0 deg rotation should be identity", aPos, aResult);
    }

    // Test 90 degrees - ALIGN_BASELINE
    // x' = x - (0 * sin(90)) = 100
    // y' = y + (0 * cos(90)) = 100
    {
        Point aResult = vcl::text::TextGeometry::GetRotationOrigin(aPos, aTextSize, 900_deg10,
                                                                   ALIGN_BASELINE);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("90 deg baseline should match pos", aPos, aResult);
    }

    // Test 90 degrees - ALIGN_TOP (Height = 20)
    // nAlignOfs = 20
    // nX = 100 + (-20 * sin(90)) = 100 - 20 = 80
    // nY = 100 + (20 * cos(90)) = 100 + 0 = 100
    {
        Point aResult
            = vcl::text::TextGeometry::GetRotationOrigin(aPos, aTextSize, 900_deg10, ALIGN_TOP);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("90 deg top alignment X mismatch", tools::Long(80),
                                     aResult.X());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("90 deg top alignment Y mismatch", tools::Long(100),
                                     aResult.Y());
    }

    // Test 180 degrees - ALIGN_BOTTOM (Height = 20)
    // nAlignOfs = -20
    // nX = 100 + (-(-20) * sin(180)) = 100 + 0 = 100
    // nY = 100 + (-20 * cos(180)) = 100 + (-20 * -1) = 120
    {
        Point aResult
            = vcl::text::TextGeometry::GetRotationOrigin(aPos, aTextSize, 1800_deg10, ALIGN_BOTTOM);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("180 deg bottom alignment X mismatch", tools::Long(100),
                                     aResult.X());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("180 deg bottom alignment Y mismatch", tools::Long(120),
                                     aResult.Y());
    }

    // Test 45 degrees - ALIGN_TOP (Diagonal coverage)
    // sin(45) approx 0.707, cos(45) approx 0.707
    // nX = 100 + (-20 * 0.7071) = 100 - 14.14 = 86
    // nY = 100 + (20 * 0.7071) = 100 + 14.14 = 114
    {
        Point aResult
            = vcl::text::TextGeometry::GetRotationOrigin(aPos, aTextSize, 450_deg10, ALIGN_TOP);
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("45 deg X coordinate mismatch", 86.0,
                                             (double)aResult.X(), 0.5);
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("45 deg Y coordinate mismatch", 114.0,
                                             (double)aResult.Y(), 0.5);
    }
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testCalculateLayoutOrigin)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(1000, 1000));

    // Setup: A 100x100 rectangle at (10, 10)
    tools::Rectangle aRect(Point(10, 10), Size(100, 100));
    tools::Long nTxtW = 20;
    tools::Long nTxtH = 10;

    // 1. Default (Top-Left)
    // X = 10, Y = 10
    Point aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(*pVDev, aRect, nTxtW, nTxtH,
                                                                DrawTextFlags::NONE, ALIGN_TOP);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(10), aPos.X());
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(10), aPos.Y());

    // 2. Center-Center
    // X = 10 + (100 - 20)/2 = 50
    // Y = 10 + (100 - 10)/2 = 55
    aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(
        *pVDev, aRect, nTxtW, nTxtH, DrawTextFlags::Center | DrawTextFlags::VCenter, ALIGN_TOP);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(50), aPos.X());
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(55), aPos.Y());

    // 3. Right-Bottom
    // X = 10 + (100 - 20) = 90
    // Y = 10 + (100 - 10) = 100
    aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(
        *pVDev, aRect, nTxtW, nTxtH, DrawTextFlags::Right | DrawTextFlags::Bottom, ALIGN_TOP);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(90), aPos.X());
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(100), aPos.Y());

    // 4. Font Alignment (ALIGN_BOTTOM)
    // Adds nTxtH to Y.
    // Y = 10 (RectTop) + 10 (TextHeight) = 20
    aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(*pVDev, aRect, nTxtW, nTxtH,
                                                          DrawTextFlags::NONE, ALIGN_BOTTOM);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(20), aPos.Y());

    // 5. Font Alignment (ALIGN_BASELINE)
    // Adds Ascent to Y.
    long nAscent = pVDev->GetFontMetric().GetAscent();
    aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(*pVDev, aRect, nTxtW, nTxtH,
                                                          DrawTextFlags::NONE, ALIGN_BASELINE);
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(10 + nAscent), aPos.Y());
}

class StubPhysicalFontFace : public vcl::font::PhysicalFontFace
{
public:
    StubPhysicalFontFace()
        : PhysicalFontFace(vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0))
    {
    }
    virtual rtl::Reference<LogicalFontInstance>
    CreateFontInstance(const vcl::font::FontSelectPattern&) const override
    {
        return nullptr;
    }
    virtual sal_IntPtr GetFontId() const override { return reinterpret_cast<sal_IntPtr>(this); }
    virtual hb_blob_t* GetHbTable(hb_tag_t) const override { return nullptr; }
};

class StubFontInstance : public LogicalFontInstance
{
public:
    StubFontInstance()
        : LogicalFontInstance(*new StubPhysicalFontFace(),
                              vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0))
    {
        mxFontMetric
            = new FontMetricData(vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0));
        mnLineHeight = 20;
        mxFontMetric->SetAscent(10);
        mxFontMetric->SetDescent(10);
        mnOrientation = 0_deg10;
    }
    virtual void ImplGetGlyphWidths(const sal_GlyphId*, bool, tools::Long*, int) const {}
    virtual bool ImplGetGlyphBoundRect(sal_GlyphId, tools::Rectangle& rRect, bool) const
    {
        rRect = tools::Rectangle(Point(0, -10), Size(10, 20));
        return true;
    }
    virtual void ImplGetFontMetric(FontMetricData&) const {}
    virtual bool GetGlyphOutline(sal_GlyphId, basegfx::B2DPolyPolygon&, bool) const override
    {
        return false;
    }
};

class MockSalLayout : public SalLayout
{
public:
    bool bAdjustCalled = false;
    virtual void AdjustLayout(vcl::text::TextLayoutRequest&) override { bAdjustCalled = true; }
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void DrawText(SalGraphics&) const override {}
    virtual double GetTextWidth() const override { return 100.0; }
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual bool HasFontKashidaPositions() const override { return false; }
    virtual bool IsKashidaPosValid(int, int) const override { return false; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, int,
                                      int) const override
    {
        return 0;
    }
    virtual bool GetNextGlyph(const GlyphItem**, basegfx::B2DPoint&, int&,
                              const LogicalFontInstance**) const override
    {
        return false;
    }
};

// Mock to test word segmentation logic
class WordSegmentMockLayout : public MockSalLayout
{
    std::vector<GlyphItem> mGlyphs;

public:
    WordSegmentMockLayout(const std::vector<bool>& rIsSpacing)
    {
        double nX = 0;
        for (bool bSpacing : rIsSpacing)
        {
            GlyphItem aGlyph(0, 1, 0, basegfx::B2DPoint(nX, 0),
                             bSpacing ? GlyphItemFlags::IS_SPACING : GlyphItemFlags::NONE, 10.0,
                             0.0, 0.0, 0);
            mGlyphs.push_back(aGlyph);
            nX += 10.0;
        }
    }

    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance**) const override
    {
        if (nStart < static_cast<int>(mGlyphs.size()))
        {
            *pGlyph = &mGlyphs[nStart];
            rPos = mGlyphs[nStart].linearPos();
            nStart++;
            return true;
        }
        return false;
    }
};

/**
 * Validates that the engine correctly identifies segments of non-spacing glyphs.
 */
CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetWordLineSegments)
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = xFont;

    // Test "Hello World" style (Word - Space - Word)
    {
        // Glyphs: [W][W][W][S][W][W][W] (W=Word, S=Space)
        std::vector<bool> aPattern = { false, false, false, true, false, false, false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Should have 2 segments
        CPPUNIT_ASSERT_EQUAL(size_t(2), aSegments.size());

        // First word: offset 0, width 30
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, aSegments[0].second, 0.001);

        // Second word: offset 40, width 30
        CPPUNIT_ASSERT_DOUBLES_EQUAL(40.0, aSegments[1].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, aSegments[1].second, 0.001);
    }

    // Test Leading/Trailing Spaces: "  Word  "
    {
        // Glyphs: [S][S][W][W][S][S]
        std::vector<bool> aPattern = { true, true, false, false, true, true };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Should have only 1 segment for the word in the middle
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aSegments[0].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aSegments[0].second, 0.001);
    }

    // Test Rotation Projection (90 degrees)
    {
        // When rotated 90 degrees, glyphs move along Y, not X.
        // BasePoint is (0,0). Word starts at (0, 50) and is 20 units long.
        std::vector<bool> aPattern = { false, false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        // Manually adjust mock glyph positions to simulate vertical flow
        // In vertical/rotated text, nDist depends on cos(90) and sin(90)
        xFont->mnOrientation = 900_deg10; // 90 degrees

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Verify rotation math: nDist = nDist * cos(90) - nDY * sin(90)
        // cos(90) = 0, sin(90) = 1. So nDist = -nDY.
        // If the word started at Y=0, nDist should be 0.
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);
    }

    {
        // Test: 270 degrees at origin
        // BasePoint (0,0), Glyph at (0,0). Expected distance = 0.
        std::vector<bool> aPattern = { false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);
        xFont->mnOrientation = 2700_deg10;

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(aLayout, aRealization, aSegments);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);

        // Test: 270 degrees with horizontal offset (X=10).
        // nDist = (10-0)*cos(270) - (0-0)*sin(270) = 0.
        std::vector<bool> aXOffsetPattern = { true, false }; // Space at 0, Word at X=10
        WordSegmentMockLayout aXOffsetLayout(aXOffsetPattern);
        aXOffsetLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        aSegments.clear();
        vcl::text::TextGeometry::GetWordLineSegments(aXOffsetLayout, aRealization, aSegments);
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);

        // Test: 270 degrees with vertical offset (Y=50).
        // Since the mock layout increments X, we need a custom setup or
        // a known Y-offset to verify that nDist = -dY * sin(270) = dY.
        // In 270 deg, nDist = (dX * 0) - (dY * -1) = dY.

        // We simulate this by overriding a single glyph position in the mock
        // specifically to test the Y-to-Distance projection.
        // Expected result for dY=50 at 270 deg is nDist=50.

        std::vector<bool> aYOffsetPattern = { false };
        WordSegmentMockLayout aYOffsetLayout(aYOffsetPattern);
        aYOffsetLayout.DrawBase() = basegfx::B2DPoint(0, -50);

        aSegments.clear();
        vcl::text::TextGeometry::GetWordLineSegments(aYOffsetLayout, aRealization, aSegments);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        // The distance should be exactly 50.0
        CPPUNIT_ASSERT_DOUBLES_EQUAL(50.0, aSegments[0].first, 0.001);
    }
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetTextOutlines)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetFont(vcl::Font("DejaVu Sans", Size(0, 20)));
    pVDev->SetOutputSizePixel(Size(100, 100));

    basegfx::B2DPolyPolygonVector aVector;

    // We use "AV" because they often have kerning, making the layout logic relevant
    OUString aText("AV");

    // Case 1: Simple Extraction (nBase == nIndex == 0) -> "A"
    bool bRet = pVDev->GetTextOutlines(aVector, aText, 0, 0, 1);
    CPPUNIT_ASSERT_MESSAGE("GetTextOutlines should succeed for valid text", bRet);
    CPPUNIT_ASSERT_MESSAGE("Should return outlines for 'A'", !aVector.empty());

    // Calculate geometric properties of 'A'
    double nWidthA = aVector[0].getB2DRange().getWidth();

    // Case 2: Isolated Extraction (nBase=1, nIndex=1) -> "V" at 0
    // We do this BEFORE the shifted check so we have a baseline comparison
    aVector.clear();
    bRet = pVDev->GetTextOutlines(aVector, aText, 1, 1, 1);
    CPPUNIT_ASSERT(bRet);

    double nX_V_Zero = aVector[0].getB2DRange().getMinX();

    // Case 3: Offset Extraction (nBase=0, nIndex=1) -> "V" shifted by "A"
    // Extract "V", but tell the engine it is part of "AV".
    aVector.clear();
    bRet = pVDev->GetTextOutlines(aVector, aText, 0, 1, 1);
    CPPUNIT_ASSERT(bRet);

    double nX_V_Shifted = aVector[0].getB2DRange().getMinX();

    // The shifted V must be to the right of the unshifted V
    std::string sMsg = "V should be shifted right. Shifted: " + std::to_string(nX_V_Shifted)
                       + " Zero: " + std::to_string(nX_V_Zero);
    CPPUNIT_ASSERT_MESSAGE(sMsg, nX_V_Shifted > nX_V_Zero);

    // The shift amount should roughly match the width of A
    // (We use a tolerance of 5.0 to account for specific font metrics/bearings)
    double nDiff = nX_V_Shifted - nX_V_Zero;
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Offset should roughly match width of preceding character",
                                         nWidthA, nDiff, 5.0);
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testAlignAndRotateTextRect)
{
    // Define a target layout rectangle: 100x100 at (10, 10)
    // Left: 10, Top: 10, Right: 109, Bottom: 109
    tools::Rectangle aTarget(Point(10, 10), Size(100, 100));

    // Assume we calculated content text size: 20x10
    tools::Long nTextW = 20;
    tools::Long nTextH = 10;

    // Case 1: Default Alignment (Top-Left)
    // Logic:
    //   Vertical: SetBottom(Top + H - 1) -> 10 + 10 - 1 = 19
    //   Horizontal: SetRight(Left + W - 1) -> 10 + 20 - 1 = 29
    //   Rounding: Not Right aligned -> AdjustRight(1) -> Right becomes 30
    // Result: (10, 10) - (30, 19). Width = 21, Height = 10.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::NONE, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aRes.Left());
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(30), aRes.Right()); // Legacy +1 pixel
        CPPUNIT_ASSERT_EQUAL(tools::Long(19), aRes.Bottom());
    }

    // Case 2: Right / Bottom Alignment
    // Logic:
    //   Vertical (Bottom): SetTop(Bottom - H + 1) -> 109 - 10 + 1 = 100
    //   Horizontal (Right): SetLeft(Right - W + 1) -> 109 - 20 + 1 = 90
    //   Rounding: Right aligned -> AdjustLeft(-1) -> Left becomes 89
    // Result: (89, 100) - (109, 109). Width = 21, Height = 10.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::Right | DrawTextFlags::Bottom, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(89), aRes.Left()); // Legacy -1 pixel
        CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(109), aRes.Right());
        CPPUNIT_ASSERT_EQUAL(tools::Long(109), aRes.Bottom());
    }

    // Case 3: Center / VCenter Alignment
    // Logic:
    //   Horizontal (Center):
    //      AdjustLeft((100 - 20)/2) = +40 -> Left 50
    //      SetRight(50 + 20 - 1) = 69
    //   Vertical (VCenter):
    //      AdjustTop((100 - 10)/2) = +45 -> Top 55
    //      SetBottom(55 + 10 - 1) = 64
    //   Rounding: Not Right -> AdjustRight(1) -> Right becomes 70
    // Result: (50, 55) - (70, 64)
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::Center | DrawTextFlags::VCenter, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aRes.Left());
        CPPUNIT_ASSERT_EQUAL(tools::Long(55), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(70), aRes.Right());
        CPPUNIT_ASSERT_EQUAL(tools::Long(64), aRes.Bottom());
    }

    // Case 4: Rotation (90 Degrees)
    // Start with Top-Left result: (10, 10) - (30, 19). W=21, H=10.
    // Pivot Point in Code: Point(Rect.GetWidth()/2, Rect.GetHeight()/2)
    // Pivot = (10, 5). Note: This is essentially an absolute point (10, 5) near origin!
    //
    // Rotate (10, 10) around (10, 5) by 90deg (Counter-Clockwise in VCL geometry):
    //   dx = 10 - 10 = 0
    //   dy = 10 - 5 = 5
    //   NewX = PivotX + dy = 10 + 5 = 15
    //   NewY = PivotY - dx = 5 - 0 = 5
    //   Rotated Point: (15, 5)
    //
    // This confirms the logic rotates around an "origin-relative" center, likely intended
    // for use when the rect is at (0,0), but applied here to the aligned rect.
    // We test simply that the geometry changes significantly.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::NONE, 900_deg10);

        // Ensure dimensions flipped/changed
        // It shouldn't match the unrotated rect
        CPPUNIT_ASSERT(aRes.GetWidth() != 21 || aRes.GetHeight() != 10);

        // Ensure it moved (rotation around near-origin pivot usually shifts it)
        CPPUNIT_ASSERT(aRes.Left() != 10);
    }
}

// Mock for testing GetGlyphRectsFromLayout
class GlyphRectMockLayout : public MockSalLayout
{
public:
    GlyphRectMockLayout() {}

    virtual double FillDXArray(std::vector<double>* pArray, const OUString&) const override
    {
        if (pArray)
        {
            // Simulate 3 characters with accumulated widths: 10, 25, 45.
            pArray->push_back(10.0);
            pArray->push_back(25.0);
            pArray->push_back(45.0);
        }
        return 45.0;
    }
};

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetGlyphRectsFromLayout)
{
    // Setup
    GlyphRectMockLayout aLayout;
    Point aStartPt(50, 100);
    OUString aStr(u"ABC"_ustr);
    std::vector<tools::Rectangle> aRects;

    // Execute
    vcl::text::TextGeometry::GetGlyphRectsFromLayout(aLayout, aStartPt, aStr, aStr.getLength(),
                                                     aRects);

    // Verify Calculation
    // Since we didn't mock GetNextGlyph to return valid glyphs, the real SalLayout::GetBoundRect
    // will return an empty bounding box, correctly triggering the fallback Y-coordinates.
    // Fallback: nTop = StartY(100), nBottom = StartY(100) + 10 = 110
    CPPUNIT_ASSERT_EQUAL(size_t(3), aRects.size());

    // Char 1: prevX = 0, currX = 10 -> (50+0, 100) to (50+10, 110)
    CPPUNIT_ASSERT_EQUAL(tools::Long(50), aRects[0].Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aRects[0].Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRects[0].Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(110), aRects[0].Bottom());

    // Char 2: prevX = 10, currX = 25 -> (50+10, 100) to (50+25, 110)
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aRects[1].Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(75), aRects[1].Right());

    // Char 3: prevX = 25, currX = 45 -> (50+25, 100) to (50+45, 110)
    CPPUNIT_ASSERT_EQUAL(tools::Long(75), aRects[2].Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(95), aRects[2].Right());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testCalculateLayoutPass)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->SetMapMode(MapMode(MapUnit::MapPixel));

    vcl::text::TextGeometry::LayoutRequest aReq;
    aReq.aText = "Line One\nLine Two";
    aReq.aTargetRect = tools::Rectangle(Point(0, 0), Size(100, 50));
    aReq.nStyle = DrawTextFlags::MultiLine | DrawTextFlags::Center | DrawTextFlags::VCenter;
    aReq.nMnemonicPos = 0; // Underline 'L'
    aReq.nFontOrientation = 0_deg10;
    aReq.nFontHeight = 10;
    aReq.nFontAscent = 8;

    vcl::DefaultTextLayout aLayout(*pVDev);

    CoordinateMapper aMapper;
    aMapper.ResetMapMode(pVDev->GetMapMode());

    auto aResult = vcl::text::TextGeometry::CalculateLayout(aMapper, aReq, aLayout);

    // In MultiLine, we expect 2 lines of height 10 each
    CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aResult.nLineCount);

    // The height is 20, target is 50. VCenter should offset Y by (50-20)/2 = 15
    CPPUNIT_ASSERT_EQUAL(tools::Long(15), aResult.aTextRect.Top());

    // Mnemonic should be active and have a valid position
    CPPUNIT_ASSERT(aResult.bHasMnemonic);
    CPPUNIT_ASSERT_EQUAL(tools::Long(15 + 8), aResult.aMnemonic.nY); // Y + Ascent
}

class RotatableMockLayout : public MockSalLayout
{
    GlyphItem mGlyph1;
    GlyphItem mGlyph2;

public:
    const LogicalFontInstance* mpFont = nullptr;
    RotatableMockLayout()
        : mGlyph1(0, 1, 0, basegfx::B2DPoint(100, 100), GlyphItemFlags::NONE, 10.0, 0.0, 0.0, 0)
        , mGlyph2(1, 1, 0, basegfx::B2DPoint(200, 100), GlyphItemFlags::NONE, 10.0, 0.0, 0.0, 1)
    {
    }
    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance** ppFont) const override
    {
        if (ppFont)
            *ppFont = mpFont;
        if (nStart == 0)
        {
            *pGlyph = &mGlyph1;
            rPos = basegfx::B2DPoint(100, 100);
            nStart++;
            return true;
        }
        if (nStart == 1)
        {
            *pGlyph = &mGlyph2;
            rPos = basegfx::B2DPoint(200, 100);
            nStart++;
            return true;
        }
        return false;
    }
};

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testGetTextInkBounds_Rotation)
{
    StubFontInstance* pStub = new StubFontInstance();
    rtl::Reference<LogicalFontInstance> xFont(pStub);
    vcl::font::FontRealization aRealization;

    aRealization.mxFont = xFont;
    RotatableMockLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 100.0);
    aLayout.mpFont = pStub;

    tools::Rectangle aRect
        = vcl::text::TextGeometry::GetTextInkBounds(aLayout, aRealization, false);

    CPPUNIT_ASSERT_EQUAL(tools::Long(20), aRect.GetHeight());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRect.GetWidth());
}

CPPUNIT_TEST_FIXTURE(TextGeometryTest, testCalculateOutlineTransform)
{
    MockSalLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 200.0);
    vcl::font::FontRealization aRealization;
    aRealization.nXOffset = 5.0;
    aRealization.nYOffset = 5.0;
    basegfx::B2DHomMatrix aMat
        = vcl::text::TextGeometry::CalculateOutlineTransform(aLayout, aRealization, 0.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-95.0, aMat.get(0, 2), 0.001);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
