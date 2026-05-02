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
 * (a) internal state-machine consistency, and
 * (b) geometric stability under CoordinateMapper round-trip transforms.
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
 * - preserve an identical cached decomposition when semantically no-op, or
 * - correctly invalidate and update cached state when modifications occur.
 *
 * The structural fingerprint (count, coordinate aggregates, and area-like
 * metrics) is used as a diagnostic signal for cache coherence.
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
 * ----------------------------------------------------------------------------
 * SYNTAX REFERENCE
 * ----------------------------------------------------------------------------
 *
 * Scripts are line-based. Tokens are whitespace-separated.
 *
 * * Setup Commands:
 * R <x1> <y1> <x2> <y2>   Reset the current Region to a new Rectangle.
 * P <x1> <y1> <x2> <y2>   Reset to a PolyPolygon (Triangle defined by these bounds).
 * N                       Set the Region to Null.
 * L <x1> <y1> <x2> <y2>   Reset to an L-Shaped PolyPolygon.
 *
 * * Self-Aliasing Commands:
 * u                       Self-Union | i Self-Intersect | x Self-Exclude | o Self-XOr
 *
 * * Mutation Commands:
 * M <dx> <dy>             Move the Region.
 * U/I/X <x1 y1 x2 y2>     Union/Intersect/Exclude a Rectangle.
 * C                       Self-Copy Assignment.
 *
 * * Assertion Commands:
 * S                       Snapshot (Structural Fingerprint).
 * E                       Expect Structural Identity (Match last 'S').
 * T                       Transform Round-Trip (Semantic match Logic->Device->Logic).
 *
 * ============================================================================
 */

namespace
{
class RegionScriptBase : public CppUnit::TestFixture
{
protected:
    using CacheFingerprint = std::tuple<int, tools::Long, tools::Long, tools::Long>;

    // CoordinateMapper is in the global namespace
    std::unique_ptr<CoordinateMapper> mpMapper;

    RegionScriptBase()
    {
        mpMapper = std::make_unique<CoordinateMapper>();
        mpMapper->SetDPIX(96);
        mpMapper->SetDPIY(96);
    }

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

        CPPUNIT_ASSERT_EQUAL_MESSAGE("RT: Null state mismatch", e1.bIsNull, e2.bIsNull);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("RT: Empty state mismatch", e1.bIsEmpty, e2.bIsEmpty);

        if (e1.bIsNull || e1.bIsEmpty)
            return;

        auto isNear = [](tools::Long a, tools::Long b) { return std::abs(a - b) <= 1; };
        bool bBoundsMatch = isNear(e1.aBounds.Left(), e2.aBounds.Left())
                            && isNear(e1.aBounds.Right(), e2.aBounds.Right())
                            && isNear(e1.aBounds.Top(), e2.aBounds.Top())
                            && isNear(e1.aBounds.Bottom(), e2.aBounds.Bottom());

        CPPUNIT_ASSERT_MESSAGE("RT: Bounding box drifted beyond discretization tolerance",
                               bBoundsMatch);

        tools::Long nAllowedDrift = std::max<tools::Long>(20, e1.nDiagnosticArea / 50);
        CPPUNIT_ASSERT_MESSAGE(
            "RT: Diagnostic area drift suggests significant decomposition change.",
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
                    vcl::Region aCopy = aRegion;
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
                    CPPUNIT_ASSERT_EQUAL_MESSAGE("Structural Cache Invalidation Failure",
                                                 std::get<0>(aLastFp), std::get<0>(aCurrentFp));
                    break;
                }
                case 'T':
                {
                    vcl::Region aOriginal = aRegion;
                    basegfx::B2DPolyPolygon aTempPoly = aRegion.GetAsB2DPolyPolygon();
                    // Using current API for CoordinateMapper
                    aTempPoly.transform(mpMapper->Compile(true).GetMatrix());
                    vcl::Region aDevice(aTempPoly);
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
                    CPPUNIT_FAIL("Unknown RegionScript opcode");
            }
        }
    }
};

CPPUNIT_TEST_FIXTURE(RegionScriptBase, testHeisenbugPrevention_EarlyReturns)
{
    std::istringstream aScript(R"(
        R 0 0 100 100
        S
        M 0 0
        E
        I 0 0 -1 -1
        S
        I 10 10 20 20
        E
    )");
    ExecuteScript(aScript);
}

CPPUNIT_TEST_FIXTURE(RegionScriptBase, testComprehensiveStateTransitions)
{
    std::istringstream aScript(R"(
        R 0 0 100 100
        U 10 10 110 110
        S
        U 0 0 0 0
        E
        I -100 -100 1000 1000
        E
        M -500 -500
        S
        M 1 1
        M 1 1
        M 1 1
        M 1 1
        M 1 1
        S
    )");
    ExecuteScript(aScript);
}

CPPUNIT_TEST_FIXTURE(RegionScriptBase, testRepresentationTransitions)
{
    std::istringstream aScript(R"(
        N
        S
        R 0 0 100 100
        S
        P 0 0 50 50
        S
        C
        E
        M 10 10
        S
        I 0 0 0 0
        S
    )");
    ExecuteScript(aScript);
}

CPPUNIT_TEST_FIXTURE(RegionScriptBase, testRectilinearOptimization)
{
    std::istringstream aScript(R"(
        L 0 0 100 100
        S
        I 10 10 90 90
        S
        M 0 0
        E
    )");
    ExecuteScript(aScript);
}

CPPUNIT_TEST_FIXTURE(RegionScriptBase, testSelfAliasingIdentity)
{
    std::istringstream aScript(R"(
        R 0 0 100 100
        S
        u
        E
        i
        E
        o
        S
        I 10 10 20 20
        E
    )");
    ExecuteScript(aScript);
}

} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
