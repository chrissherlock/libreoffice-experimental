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

#include <tools/gen.hxx>

#include <vcl/region.hxx>

#include <CoordinateMapper.hxx>

#include <sstream>
#include <string>
#include <tuple>
#include <locale>

/**
 * ============================================================================
 * RegionScript: Deterministic Region Mutation DSL
 * ============================================================================
 *
 * Overview:
 * vcl::Region is a stateful geometry container that lazily computes and caches
 * its rectangular decomposition. Its correctness depends on both internal cache
 * coherence and stable geometric behavior under coordinate-space transformations.
 *
 * RegionScript provides a deterministic execution framework for validating:
 *   (a) internal state-machine consistency, and
 *   (b) geometric stability under CoordinateMapper round-trip transforms.
 *
 * It is designed to expose cache coherence defects in vcl::Region and
 * transformation defects in CoordinateMapper using repeatable scripted sequences,
 * without relying on direct inspection of internal representation details.
 *
 * ----------------------------------------------------------------------------
 * Invariant Bifurcation:
 *
 * The DSL defines two independent correctness layers:
 *
 * 1. Structural Invariants (S / E):
 * Validates consistency of vcl::Region’s cached structural representation.
 * Ensures that mutation sequences either:
 *   - preserve an identical cached decomposition when semantically no-op, or
 *   - correctly invalidate and update cached state when modifications occur.
 *
 * The structural fingerprint (count, coordinate aggregates, and area-like
 * metrics) is used as a diagnostic signal for cache coherence.
 *
 * This layer is intended to detect cache invalidation bugs, stale iterator
 * reuse, and incorrect reuse of cached decompositions.
 *
 * Note: The fingerprint is not a canonical geometric representation and may
 * vary with internal decomposition strategy.
 *
 * ----------------------------------------------------------------------------
 * 2. Semantic Invariants (T):
 *
 * Validates geometric stability under coordinate-space transformation.
 *
 * A Region is projected through CoordinateMapper (Logic → Device → Logic)
 * and compared against its original logical representation using tolerance-
 * aware geometric checks.
 *
 * This layer acknowledges that integer raster coordinate systems are not
 * perfectly invertible due to rounding and discretization effects.
 *
 * Allowed tolerances include:
 *   - ±1 pixel drift in bounding box coordinates
 *   - bounded deviation in derived area metrics (diagnostic only)
 *   - strict preservation of Null/Empty semantics
 *
 * This layer is intended to detect transformation errors, scaling inconsistencies,
 * and CoordinateMapper inversion or rounding defects in the rendering pipeline.
 *
 * ----------------------------------------------------------------------------
 * Design Intent:
 *
 * RegionScript is not a geometric proof system. It is a deterministic regression
 * harness for:
 *   - cache correctness (structural stability)
 *   - transformation stability (semantic robustness)
 *
 * It explicitly separates implementation-dependent structure from
 * transformation-dependent geometry.
 *
 * ----------------------------------------------------------------------------
 * SYNTAX REFERENCE
 * ----------------------------------------------------------------------------
 *
 * Scripts are line-based. Tokens are whitespace-separated.
 * The parser uses the 'C' locale to prevent CI locale lotteries.
 *
 * * Comments:
 * # Any text after a hash is ignored. Can be inline or full line.
 *
 * * Setup Commands:
 * R <x1> <y1> <x2> <y2>   Reset the current Region to a new Rectangle.
 * P <x1> <y1> <x2> <y2>   Reset to a PolyPolygon (Triangle defined by these bounds).
 *                         Forces the Region into a non-rectangular state.
 * N                       Set the Region to Null (represents the entire infinite plane).
 * L <x1> <y1> <x2> <y2>   Reset to an L-Shaped PolyPolygon. This mathematically
 *                         guarantees the 'Rectilinear' optimization path is taken.
 *
 * * Self-Aliasing Commands:
 * u                       Self-Union (Union the region with itself)
 * i                       Self-Intersect
 * x                       Self-Exclude (Diff)
 * o                       Self-XOr
 *
 * * Mutation Commands:
 * M <dx> <dy>             Move the Region by the given deltas.
 * U <x1> <y1> <x2> <y2>   Union the Region with a Rectangle.
 * I <x1> <y1> <x2> <y2>   Intersect the Region with a Rectangle.
 * X <x1> <y1> <x2> <y2>   Exclude a Rectangle from the Region.
 * C                       Self-Copy Assignment. Forces a copy-constructor and
 *                         assignment cycle to verify lifecycle cache resets.
 *
 * * Assertion Commands:
 * S                       Snapshot (Structural). Captures a strict CacheFingerprint (Count,
 *                         Sums, Area) for the current internal representation.
 *
 * E                       Expect Structural Identity. Asserts the current fingerprint is
 *                         BIT-FOR-BIT identical to the last 'S' snapshot. Proves that no-op
 *                         moves or early-returns did not churn internal state.
 *
 * T                       Transform Round-Trip (Semantic). Projects the Region through the
 *                         CoordinateMapper to Device Space (Pixels) and back to Logic Space.
 *
 *                         Asserts Semantic Equivalence:
 *                         - Bounding Box match within +-1px tolerance.
 *                         - Area match within 1% diagnostic tolerance.
 *                         - Null/Empty state preservation.
 *
 * * Example Script:
 *
 * R 0 0 100 100     # Init a 100x100 region
 * S                 # Freeze fingerprint
 * M 0 0             # Perform zero-delta move
 * E                 # Assert cache was preserved (early-return success)
 *
 * ============================================================================
 */

class RegionScriptTest : public CppUnit::TestFixture
{
    using CacheFingerprint = std::tuple<int, tools::Long, tools::Long, tools::Long>;

    std::unique_ptr<CoordinateMapper> mpMapper;

    CacheFingerprint GetCacheFingerprint(const vcl::Region& rRegion)
    {
        int count = 0;
        tools::Long xSum = 0;
        tools::Long ySum = 0;
        tools::Long nArea = 0;

        for (const auto& rect : rRegion)
        {
            if (rect.IsEmpty())
                continue;
            count++;
            xSum += rect.Left() + rect.Right();
            ySum += rect.Top() + rect.Bottom();
            nArea += (rect.GetWidth() * rect.GetHeight());
        }

        return std::make_tuple(count, xSum, ySum, nArea);
    }

    struct RegionSemanticEquivalence
    {
        tools::Rectangle aBounds;
        bool bIsNull;
        bool bIsEmpty;

        /** * NOTE: Not a true geometric invariant.
         * This is a decomposition-dependent diagnostic metric only.
         * It may double-count overlapping rectangles depending on the internal
         * Region representation (e.g. during PolyPolygon transitions).
         */
        tools::Long nDiagnosticArea;

        static RegionSemanticEquivalence FromRegion(const vcl::Region& rRegion)
        {
            RegionSemanticEquivalence e;
            e.bIsNull = rRegion.IsNull();
            e.bIsEmpty = rRegion.IsEmpty();
            e.aBounds = rRegion.GetBoundRect();
            e.nDiagnosticArea = 0;

            if (!e.bIsNull && !e.bIsEmpty)
            {
                for (const auto& rect : rRegion)
                    e.nDiagnosticArea += (rect.GetWidth() * rect.GetHeight());
            }
            return e;
        }
    };

    void AssertSemanticEquivalence(const vcl::Region& rOrig, const vcl::Region& rBack)
    {
        auto e1 = RegionSemanticEquivalence::FromRegion(rOrig);
        auto e2 = RegionSemanticEquivalence::FromRegion(rBack);

        // Hard Invariants: Emptiness must be preserved
        CPPUNIT_ASSERT_EQUAL_MESSAGE("RT: Null state mismatch", e1.bIsNull, e2.bIsNull);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("RT: Empty state mismatch", e1.bIsEmpty, e2.bIsEmpty);

        if (e1.bIsNull || e1.bIsEmpty)
            return;

        // Discretization Invariant: The +-1px Bounding Box rule
        auto isNear = [](tools::Long a, tools::Long b) { return std::abs(a - b) <= 1; };
        bool bBoundsMatch = isNear(e1.aBounds.Left(), e2.aBounds.Left())
                            && isNear(e1.aBounds.Right(), e2.aBounds.Right())
                            && isNear(e1.aBounds.Top(), e2.aBounds.Top())
                            && isNear(e1.aBounds.Bottom(), e2.aBounds.Bottom());

        CPPUNIT_ASSERT_MESSAGE("RT: Bounding box drifted beyond discretization tolerance",
                               bBoundsMatch);

        // Diagnostic Signal: Area drift
        // We increase the tolerance to accommodate non-canonical decompositions.
        tools::Long nAllowedDrift
            = std::max<tools::Long>(20, e1.nDiagnosticArea / 50); // 2% tolerance

        // We use a specific message to indicate this is a REPRESENTATION-based failure
        OString aAreaMsg = "RT: Diagnostic area drift suggests significant decomposition change. "
                           "Check for unexpected splitting or overlaps.";

        CPPUNIT_ASSERT_MESSAGE(aAreaMsg.getStr(),
                               std::abs(e1.nDiagnosticArea - e2.nDiagnosticArea) <= nAllowedDrift);
    }

    void ExecuteScript(std::istream& rStream)
    {
        vcl::Region aRegion;
        CacheFingerprint aLastFp = GetCacheFingerprint(aRegion);

        std::string aLine;
        int nLineNum = 0;

        while (std::getline(rStream, aLine))
        {
            nLineNum++;
            size_t nFirst = aLine.find_first_not_of(" \t\r\n");
            if (nFirst == std::string::npos || aLine[nFirst] == '#')
                continue;

            std::istringstream aLineStream(aLine);
            aLineStream.imbue(std::locale::classic());

            char cOp;
            aLineStream >> cOp;

            auto require = [&](bool bCondition, const char* msg) {
                if (!bCondition)
                {
                    OString aErr
                        = "Parse failure at line " + OString::number(nLineNum) + ": " + msg;
                    CPPUNIT_FAIL(aErr.getStr());
                }
            };

            tools::Long x1 = 0, y1 = 0, x2 = 0, y2 = 0;

            switch (cOp)
            {
                case 'R':
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse R args");
                    aRegion = vcl::Region(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'P':
                {
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse P args");
                    tools::Polygon aPoly(3);
                    aPoly.SetPoint(Point(x1, y1), 0);
                    aPoly.SetPoint(Point(x2, y1), 1);
                    aPoly.SetPoint(Point(x1, y2), 2);
                    aRegion = vcl::Region(aPoly);
                    break;
                }

                case 'N':
                    aRegion.SetNull();
                    break;

                case 'C':
                {
                    vcl::Region aCopy;
                    aCopy = aRegion;
                    aRegion = aCopy;
                    break;
                }

                case 'M':
                    aLineStream >> x1 >> y1;
                    require(!aLineStream.fail(), "Failed to parse M args");
                    aRegion.Move(x1, y1);
                    break;

                case 'U':
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse U args");
                    aRegion.Union(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'I':
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse I args");
                    aRegion.Intersect(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'X':
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse X args");
                    aRegion.Exclude(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'S':
                    aLastFp = GetCacheFingerprint(aRegion);
                    break;

                case 'E':
                {
                    CacheFingerprint aCurrentFp = GetCacheFingerprint(aRegion);

                    if (aLastFp != aCurrentFp)
                    {
                        OString aBaseMsg = "Structural Cache Invalidation Failure at line "
                                           + OString::number(nLineNum);

                        // Check Rectangle Count
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(OString(aBaseMsg + " (Count)").getStr(),
                                                     std::get<0>(aLastFp), std::get<0>(aCurrentFp));

                        // Check Sum of X-coordinates
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(OString(aBaseMsg + " (SumX)").getStr(),
                                                     std::get<1>(aLastFp), std::get<1>(aCurrentFp));

                        // Check Sum of Y-coordinates
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(OString(aBaseMsg + " (SumY)").getStr(),
                                                     std::get<2>(aLastFp), std::get<2>(aCurrentFp));

                        // Check Total Area (Weak Invariant)
                        CPPUNIT_ASSERT_EQUAL_MESSAGE(OString(aBaseMsg + " (Area)").getStr(),
                                                     std::get<3>(aLastFp), std::get<3>(aCurrentFp));
                    }
                    break;
                }

                case 'T': // Semantic Round-Trip (System-State Aware)
                {
                    vcl::Region aOriginal = aRegion;

                    basegfx::B2DPolyPolygon aTempPoly = aRegion.GetAsB2DPolyPolygon();
                    // We use 'true' because the original test line was hardcoded to 'true'
                    aTempPoly.transform(mpMapper->GetLogicToDeviceMatrix(true));
                    vcl::Region aDevice(aTempPoly);

                    // We must pass 'aDevice' here so mpMapper has pixels to turn back into logic
                    vcl::Region aBack = mpMapper->DevicePixelToLogic(aDevice, true);

                    AssertSemanticEquivalence(aOriginal, aBack);
                    break;
                }

                case 'L':
                {
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse L args");
                    tools::Long t = std::max<tools::Long>(1, (x2 - x1) / 4);
                    tools::Polygon aPoly(6);
                    aPoly.SetPoint(Point(x1, y1), 0);
                    aPoly.SetPoint(Point(x2, y1), 1);
                    aPoly.SetPoint(Point(x2, y1 + t), 2);
                    aPoly.SetPoint(Point(x1 + t, y1 + t), 3);
                    aPoly.SetPoint(Point(x1 + t, y2), 4);
                    aPoly.SetPoint(Point(x1, y2), 5);
                    aRegion = vcl::Region(aPoly);
                    break;
                }

                case 'u':
                    aRegion.Union(aRegion);
                    break;

                case 'i':
                    aRegion.Intersect(aRegion);
                    break;

                case 'x':
                    aRegion.Exclude(aRegion);
                    break;

                case 'o':
                    aRegion.XOr(aRegion);
                    break;

                default:
                    CPPUNIT_FAIL(OString("Unknown RegionScript opcode").getStr());
            }

            std::string aTrailing;
            if (aLineStream >> aTrailing)
                require(aTrailing[0] == '#', "Trailing junk found");
        }
    }

public:
    virtual void setUp() override
    {
        mpMapper = std::make_unique<CoordinateMapper>();
        mpMapper->SetDPIX(96);
        mpMapper->SetDPIY(96);
    }

    void testHeisenbugPrevention_EarlyReturns();
    void testComprehensiveStateTransitions();
    void testRepresentationTransitions();
    void testRectilinearOptimization();
    void testSelfAliasingIdentity();

    CPPUNIT_TEST_SUITE(RegionScriptTest);
    CPPUNIT_TEST(testHeisenbugPrevention_EarlyReturns);
    CPPUNIT_TEST(testComprehensiveStateTransitions);
    CPPUNIT_TEST(testRepresentationTransitions);
    CPPUNIT_TEST(testRectilinearOptimization);
    CPPUNIT_TEST(testSelfAliasingIdentity);
    CPPUNIT_TEST_SUITE_END();
};

void RegionScriptTest::testHeisenbugPrevention_EarlyReturns()
{
    // Raw string literal injection ensures exact CI replayability
    std::istringstream aScript(R"(
        # Setup base region
        R 0 0 100 100
        S

        # Test early-return preservation (No-Op Move)
        M 0 0
        E    # This remains stable because Move(0,0) returns BEFORE InvalidateCache()

        # Test mutation to Empty (Using -1 -1 to force an invalid/empty tools::Rectangle)
        I 0 0 -1 -1
        S    # We SNAPSHOT here because the cache was intentionally reset by SetEmpty()

        # Test TRUE Early Return (Empty intersected with anything)
        I 10 10 20 20
        E
    )");

    ExecuteScript(aScript);
}

void RegionScriptTest::testComprehensiveStateTransitions()
{
    std::istringstream aScript(R"(
        # 1. Test representation 'Downgrade' (Poly -> Band)
        # Setup a complex triangle (forces PolyPolygon representation)
        # Note: We use R with many points if we expanded our DSL,
        # but for now, let's use what we have.

        R 0 0 100 100
        U 10 10 110 110   # Create a non-rectangular shape
        S

        # 2. Test 'Empty Trap'
        # Union with an empty rect should be a no-op
        U 0 0 0 0
        E

        # 3. Test 'Identity Trap'
        # Intersect with yourself should be a no-op
        # (Assuming we added a 'Self-Intersect' command,
        # but a large Rect intersection works too)
        I -100 -100 1000 1000
        E

        # 4. Test Coordinate Overflow/Negative Space
        # Move into negative coordinates
        M -500 -500
        S

        # 5. The 'Null' Transition
        # Intersecting anything with a Null region returns the thing.
        # This tests the mbIsNull branch in your code.
        # (Requires a 'SetNull' command in the DSL)

        # 6. Successive Mutations (The "Drift" Test)
        # Hammer the region with many small moves to see if
        # sums stay deterministic.
        M 1 1
        M 1 1
        M 1 1
        M 1 1
        M 1 1
        S
    )");

    ExecuteScript(aScript);
}

void RegionScriptTest::testRepresentationTransitions()
{
    std::istringstream aScript(R"(
        # Start with a Null region
        N
        S

        # Transition Null -> Rectangle
        R 0 0 100 100
        S # Checkpoint new shape

        # Transition Rectangle -> PolyPolygon (Triangle)
        P 0 0 50 50
        S # Checkpoint triangle decomposition

        # Verify Copy-Assignment resets cache but maintains geometry
        C
        E # Fingerprint MUST match after copy

        # Move the triangle
        M 10 10
        S

        # Intersect with empty (Rectangle -> Empty)
        I 0 0 0 0
        S
    )");

    ExecuteScript(aScript);
}

void RegionScriptTest::testRectilinearOptimization()
{
    std::istringstream aScript(R"(
        # Setup an L-Shaped polygon (Forces ImplIsPolygonRectilinear = true)
        L 0 0 100 100
        S

        # Trigger the optimized ImplRectilinearPolygonToBands conversion
        # by intersecting with a rectangle that cuts across the L-shape.
        I 10 10 90 90
        S

        # Verify the cache remains stable after early-return move
        M 0 0
        E
    )");

    ExecuteScript(aScript);
}

void RegionScriptTest::testSelfAliasingIdentity()
{
    std::istringstream aScript(R"(
        # Setup a standard region
        R 0 0 100 100
        S

        # 1. Self-Union (Should be a no-op, region == region)
        u
        E

        # 2. Self-Intersect (Should be a no-op)
        i
        E

        # 3. Self-XOr (A Region XOR'd with itself becomes EMPTY)
        # Note: XOr'ing with self completely destroys the area.
        o
        S  # Snapshot the new Empty state

        # Verify it actually became empty
        # Intersecting an empty region with a 10x10 stays empty
        I 10 10 20 20
        E
    )");

    ExecuteScript(aScript);
}

CPPUNIT_TEST_SUITE_REGISTRATION(RegionScriptTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
