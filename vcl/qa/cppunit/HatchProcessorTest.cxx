/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/color.hxx>
#include <test/bootstrapfixture.hxx>

#include <basegfx/numeric/ftools.hxx>
#include <tools/line.hxx>
#include <tools/poly.hxx>

#include <vcl/hatch.hxx>
#include <vcl/vclenum.hxx>

#include <HatchProcessor.hxx>

using namespace vcl;

class HatchProcessorTest : public test::BootstrapFixture
{
public:
    HatchProcessorTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testSingleHatchHorizontal)
{
    // 100x100 Square
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    // Horizontal lines (0 degrees), spacing 20
    Hatch aHatch(HatchStyle::Single, COL_BLACK, 20, 0_deg10);

    std::vector<tools::Line> aLines;

    // Shift RefPoint to (0, 10) to avoid boundary issues.
    // Expected lines at Y: 10, 30, 50, 70, 90 (5 lines)
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 10), 1, 20,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should produce 5 horizontal lines", size_t(5), aLines.size());

    for (const auto& line : aLines)
    {
        CPPUNIT_ASSERT_EQUAL(line.GetStart().Y(), line.GetEnd().Y()); // Horizontal
        CPPUNIT_ASSERT_EQUAL(tools::Long(0), line.GetStart().X());
        CPPUNIT_ASSERT_EQUAL(tools::Long(100), line.GetEnd().X());
    }
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testSingleHatchVertical)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    // Vertical lines (90 degrees), spacing 25
    Hatch aHatch(HatchStyle::Single, COL_BLACK, 25, 900_deg10);

    std::vector<tools::Line> aLines;

    // Shift RefPoint to (10, 0).
    // Expected lines at X: 10, 35, 60, 85 (4 lines)
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(10, 0), 1, 25,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should produce 4 vertical lines", size_t(4), aLines.size());

    for (const auto& line : aLines)
    {
        CPPUNIT_ASSERT_EQUAL(line.GetStart().X(), line.GetEnd().X()); // Vertical
    }
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testSingleHatch45Deg)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    Hatch aHatch(HatchStyle::Single, COL_BLACK, 20, 450_deg10);

    std::vector<tools::Line> aLines;
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 0), 1, 20,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT(!aLines.empty());

    // Check slope of middle line
    const auto& l = aLines[aLines.size() / 2];
    double dx = l.GetEnd().X() - l.GetStart().X();
    double dy = l.GetEnd().Y() - l.GetStart().Y();

    if (std::abs(dx) > 0.001)
    {
        double slope = std::abs(dy / dx);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, slope, 0.1);
    }
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testDoubleHatch)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    // Double hatch = Single (0 deg) + 90 deg rotated
    Hatch aHatch(HatchStyle::Double, COL_BLACK, 50, 0_deg10);

    std::vector<tools::Line> aLines;

    // Shift RefPoint to (25, 25).
    // Horz lines: 25, 75. Vert lines: 25, 75.
    // Total 4 lines.
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(25, 25), 1, 50,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT_EQUAL(size_t(4), aLines.size());
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testTripleHatch)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    // Triple = Single + 90 + 45
    Hatch aHatch(HatchStyle::Triple, COL_BLACK, 50, 0_deg10);

    std::vector<tools::Line> aLines;

    // Same offset as double hatch (25, 25).
    // Should contain the 4 grid lines plus diagonals.
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(25, 25), 1, 50,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT_MESSAGE("Triple hatch should add diagonals", aLines.size() > 4);
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testCurveSubdivision)
{
    // Create a polygon with bezier curves manually
    tools::Polygon aCurvePoly(4);
    aCurvePoly[0] = Point(0, 0);
    aCurvePoly[1] = Point(0, 100);
    aCurvePoly[2] = Point(100, 100);
    aCurvePoly[3] = Point(100, 0);

    // Mark index 1 and 2 as control points to ensure it's treated as a curve
    aCurvePoly.SetFlags(1, PolyFlags::Control);
    aCurvePoly.SetFlags(2, PolyFlags::Control);

    tools::PolyPolygon aPoly(aCurvePoly);

    Hatch aHatch(HatchStyle::Single, COL_BLACK, 10, 0_deg10);

    int nCallbacks = 0;
    HatchProcessor::Process(aPoly, aHatch, aPoly.GetBoundRect(), Point(0, 0), 1, 10,
                            [&](const Point&, const Point&) { nCallbacks++; });

    // If subdivision didn't happen, intersections with curves might fail or produce weird results.
    // We mainly want to ensure it produced lines and didn't crash.
    CPPUNIT_ASSERT_MESSAGE("Curve should produce hatch lines", nCallbacks > 0);
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testRefPointShift)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);
    Hatch aHatch(HatchStyle::Single, COL_BLACK, 20, 0_deg10);

    // Case 1: Ref (0,5) -> Lines at Y=5, 25, 45, 65, 85 (5 lines)
    std::vector<Point> aPoints1;
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 5), 1, 20,
                            [&](const Point& p1, const Point&) { aPoints1.push_back(p1); });

    // Case 2: Ref (0,15) -> Lines at Y=15, 35, 55, 75, 95 (5 lines)
    std::vector<Point> aPoints2;
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 15), 1, 20,
                            [&](const Point& p1, const Point&) { aPoints2.push_back(p1); });

    CPPUNIT_ASSERT(!aPoints1.empty());
    CPPUNIT_ASSERT(!aPoints2.empty());

    // Compare Y of first lines
    tools::Long y1 = aPoints1[0].Y();
    tools::Long y2 = aPoints2[0].Y();

    // They should NOT be equal (offset by 10)
    CPPUNIT_ASSERT(y1 != y2);

    // Validate alignment
    CPPUNIT_ASSERT_EQUAL(5, int(y1 % 20));
    CPPUNIT_ASSERT_EQUAL(15, int(y2 % 20));
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testGapCalculations)
{
    // 0..100 rect
    // 0 deg, dist 1000 (larger than rect)

    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);
    Hatch aHatch(HatchStyle::Single, COL_BLACK, 1000, 0_deg10);

    // RefPoint at 50 ensures the single line is at Y=50 (strictly inside)
    int nCallbacks = 0;
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 50), 1, 1000,
                            [&](const Point&, const Point&) { nCallbacks++; });

    // Line at 50 is inside. Next at 1050 is out.
    // Expect 1 line.
    CPPUNIT_ASSERT_EQUAL(1, nCallbacks);
}

CPPUNIT_TEST_FIXTURE(HatchProcessorTest, testSingleHatchSteepDiagonal)
{
    // Test for 'Steep' diagonals (e.g. 80 degrees) which trigger the Vertical Scan logic
    // inside lcl_CalcDiagonalHatchVerticalScan.

    tools::Rectangle aRect(0, 0, 100, 100);
    tools::PolyPolygon aPoly(aRect);

    // 80 degrees, spacing 20
    Hatch aHatch(HatchStyle::Single, COL_BLACK, 20, 800_deg10);

    std::vector<tools::Line> aLines;
    HatchProcessor::Process(aPoly, aHatch, aRect, Point(0, 0), 1, 20,
                            [&](const Point& p1, const Point& p2) { aLines.emplace_back(p1, p2); });

    CPPUNIT_ASSERT_MESSAGE("Should produce lines for steep diagonal", !aLines.empty());

    // Verify slope is steep (dy > dx)
    const auto& l = aLines[aLines.size() / 2];
    double dx = std::abs(l.GetEnd().X() - l.GetStart().X());
    double dy = std::abs(l.GetEnd().Y() - l.GetStart().Y());

    // For 80 degrees, tan(80) ~= 5.67, so dy should be > dx
    CPPUNIT_ASSERT_MESSAGE("Steep diagonal should have dy > dx", dy > dx);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
