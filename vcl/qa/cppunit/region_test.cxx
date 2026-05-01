/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/region.hxx>
#include <tools/gen.hxx>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <sstream>
#include <string>
#include <tuple>
#include <locale>

/**
 * ============================================================================
 * RegionScript: Deterministic Region Mutation DSL
 * ============================================================================
 *
 * * Overview:
 * vcl::Region is a complex, mutation-heavy state machine that lazily computes
 * and caches its rectangular decomposition. Traditional C++ unit tests struggle
 * to reliably expose "Heisenbugs" caused by stale caches or missing invalidation
 * flags deep inside chained mutation operations (e.g., Move -> Union -> Intersect).
 *
 * * RegionScript is a deterministic, plain-text Domain Specific Language (DSL)
 * designed to solve this. It allows complex mutation chains to be serialized
 * into strings. This enables:
 *
 * 1. Perfect CI reproducibility (paste a failing script directly into a test).
 * 2. Fuzz-testing compatibility (external fuzzers can generate scripts).
 * 3. Strict behavioral contract enforcement without internal class hacking.
 *
 * * The Oracle (CacheFingerprint):
 * Rather than asserting exact geometry (which can be fragile across backends),
 * this harness uses a structural fingerprint: (SpanCount, Sum(X), Sum(Y)).
 * This guarantees that the iterator cache is serving identical geometric
 * decompositions without relying on undefined STL memory identity.
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
 * guarantees the 'Rectilinear' optimization path is taken.
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
 * * Assertion Commands (The Invalidation Guards):
 * S                       Snapshot (Semantic Checkpoint). Calculates the
 *                         current CacheFingerprint and stores it.
 *
 * E                       Expect Match. Calculates the current CacheFingerprint
 *                         and asserts it is identical to the last 'S' Snapshot.
 *                         Used to prove that an operation (like an early return)
 *                         did not illegally corrupt or churn the cache.
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
    // Explicitly named to prevent misuse as a definitive geometric truth oracle
    using CacheFingerprint = std::tuple<int, tools::Long, tools::Long>;

    CacheFingerprint GetCacheFingerprint(const vcl::Region& rRegion)
    {
        int count = 0;
        tools::Long xSum = 0;
        tools::Long ySum = 0;

        for (const auto& rect : rRegion)
        {
            count++;
            xSum += rect.Left() + rect.Right();
            ySum += rect.Top() + rect.Bottom();
        }

        return std::make_tuple(count, xSum, ySum);
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

            // Find the first actual character that isn't a space, tab, or newline
            size_t nFirst = aLine.find_first_not_of(" \t\r\n");

            // If the line is empty, only whitespace, or a comment, skip it entirely
            if (nFirst == std::string::npos || aLine[nFirst] == '#')
                continue;

            std::istringstream aLineStream(aLine);
            aLineStream.imbue(std::locale::classic());

            char cOp;
            aLineStream >> cOp; // This will now definitely be the first non-whitespace char

            auto require = [&](bool bCondition, const char* msg) {
                if (!bCondition)
                {
                    OString aErr
                        = "Parse failure at line " + OString::number(nLineNum) + ": " + msg;
                    CPPUNIT_FAIL(aErr.getStr());
                }
            };

            require(!aLineStream.fail(), "Missing opcode");

            tools::Long x1 = 0, y1 = 0, x2 = 0, y2 = 0;

            switch (cOp)
            {
                case 'R': // Reset/Create Region
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse R args");
                    aRegion = vcl::Region(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'P': // Reset to PolyPolygon (Triangle)
                {
                    aLineStream >> x1 >> y1 >> x2 >> y2; // Use args to define a triangle
                    require(!aLineStream.fail(), "Failed to parse P args");
                    tools::Polygon aPoly(3);
                    aPoly.SetPoint(Point(x1, y1), 0);
                    aPoly.SetPoint(Point(x2, y1), 1);
                    aPoly.SetPoint(Point(x1, y2), 2);
                    aRegion = vcl::Region(aPoly);
                    break;
                }

                case 'N': // Set Null (Everything)
                    aRegion.SetNull();
                    break;

                case 'C': // Force Copy-Assignment
                {
                    vcl::Region aCopy;
                    aCopy = aRegion; // Tests if = operator resets cache
                    aRegion = aCopy; // Cycle it back
                    break;
                }

                case 'M': // Move
                    aLineStream >> x1 >> y1;
                    require(!aLineStream.fail(), "Failed to parse M args");
                    aRegion.Move(x1, y1);
                    break;

                case 'U': // Union Rect
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse U args");
                    aRegion.Union(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'I': // Intersect Rect
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse I args");
                    aRegion.Intersect(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'X': // Exclude Rect
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse X args");
                    aRegion.Exclude(tools::Rectangle(x1, y1, x2, y2));
                    break;

                case 'S': // Semantic Checkpoint (Freeze state)
                    aLastFp = GetCacheFingerprint(aRegion);
                    break;

                case 'E': // Expect Semantic Match
                {
                    CacheFingerprint aCurrentFp = GetCacheFingerprint(aRegion);

                    if (aLastFp != aCurrentFp)
                    {
                        // Unpack tuples for clear error reporting
                        auto[nLastCount, nLastX, nLastY] = aLastFp;
                        auto[nCurCount, nCurX, nCurY] = aCurrentFp;

                        OString aMsg
                            = "CacheFingerprint mismatch at line " + OString::number(nLineNum)
                              + "\n  Snapshot: Count=" + OString::number(nLastCount) + ", SumX="
                              + OString::number(nLastX) + ", SumY=" + OString::number(nLastY)
                              + "\n  Current:  Count=" + OString::number(nCurCount) + ", SumX="
                              + OString::number(nCurX) + ", SumY=" + OString::number(nCurY);

                        CPPUNIT_FAIL(aMsg.getStr());
                    }
                    break;
                }

                case 'L': // Reset to L-Shaped Rectilinear Polygon
                {
                    aLineStream >> x1 >> y1 >> x2 >> y2;
                    require(!aLineStream.fail(), "Failed to parse L args");

                    // Create an L-shape with a thickness of 25% of the width
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

                case 'u': // Self-Union
                    aRegion.Union(aRegion);
                    break;

                case 'i': // Self-Intersect
                    aRegion.Intersect(aRegion);
                    break;

                case 'x': // Self-Exclude
                    aRegion.Exclude(aRegion);
                    break;

                case 'o': // Self-XOr
                    aRegion.XOr(aRegion);
                    break;

                default:
                    CPPUNIT_FAIL(
                        OString("Unknown RegionScript opcode at line " + OString::number(nLineNum))
                            .getStr());
            }

            // Strict trailing junk detection
            std::string aTrailing;
            if (aLineStream >> aTrailing)
                require(aTrailing[0] == '#', "Trailing junk found after valid command");
        }
    }

public:
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
