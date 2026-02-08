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

#include <vcl/text/TextGeometry.hxx>

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

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
