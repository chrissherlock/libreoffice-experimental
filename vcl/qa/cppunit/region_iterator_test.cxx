/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <vcl/region.hxx>
#include <tools/gen.hxx>

namespace
{
// PROVES: Range-based for loops and iterators correctly lazily evaluate
// and yield the expected underlying geometry.

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testBasicIteration)
{
    tools::Rectangle aRect(10, 10, 50, 50);
    vcl::Region aRegion(aRect);

    int count = 0;
    for (const auto& rIterRect : aRegion)
    {
        CPPUNIT_ASSERT_EQUAL(aRect, rIterRect);
        count++;
    }
    CPPUNIT_ASSERT_EQUAL(1, count);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCopySemantics)
{
    vcl::Region aSource(tools::Rectangle(0, 0, 10, 10));

    // Get initial rect
    tools::Rectangle aInitialRect = *aSource.begin();

    // PERFORM COPY
    vcl::Region aTarget = aSource;

    // VALIDATE ISOLATION (Cache is handle-local)
    // The iterator addresses should be different because caches are not shared.
    CPPUNIT_ASSERT_MESSAGE("Cache must be local to the handle",
                           &(*aSource.begin()) != &(*aTarget.begin()));

    // PERFORM MUTATION (Trigger Detach)
    aTarget.Move(5, 5);

    // VALIDATE COW ISOLATION
    // The source should remain unchanged.
    CPPUNIT_ASSERT_EQUAL(aInitialRect, *aSource.begin());
    // The target should reflect the mutation.
    CPPUNIT_ASSERT_EQUAL(tools::Rectangle(5, 5, 15, 15), *aTarget.begin());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMoveSemantics)
{
    tools::Rectangle aRect(5, 5, 20, 20);
    vcl::Region aSource(aRect);

    // Prime source cache
    CPPUNIT_ASSERT(!aSource.IsEmpty());

    // Move construct
    vcl::Region aTarget(std::move(aSource));

    // Verify target adopted the geometry safely
    CPPUNIT_ASSERT_EQUAL(aRect, *aTarget.begin());

    // VERIFY NULL-OBJECT PATTERN (Hard COW)
    // In our final COW model, mpData is NEVER nullptr. It points to the
    // immortal singleton which has mbIsNull = true.
    CPPUNIT_ASSERT_MESSAGE("Moved-from source must be Null", aSource.IsNull());

    // Proves that the singleton safely handles iterator requests without crashing
    CPPUNIT_ASSERT_MESSAGE("Moved-from source must return safe empty iterators",
                           aSource.begin() == aSource.end());

    // Test Move Assignment
    vcl::Region aAssignTarget;
    aAssignTarget = std::move(aTarget);

    // Verify assigned target works
    CPPUNIT_ASSERT_EQUAL(aRect, *aAssignTarget.begin());

    // Verify the assigned-from source was cleared to the Null singleton
    CPPUNIT_ASSERT_MESSAGE("Move-assigned source must be Null", aTarget.IsNull());
    CPPUNIT_ASSERT_MESSAGE("Move-assigned source iterators must be safe",
                           aTarget.begin() == aTarget.end());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testNullMathematicalProperties)
{
    // A Null region represents Infinite logical space
    vcl::Region aInfiniteRegion(true);
    vcl::Region aShape(tools::Rectangle(10, 10, 50, 50));

    // Intersect: Infinite ∩ Shape = Shape
    vcl::Region aTestIntersect = aInfiniteRegion;
    aTestIntersect.Intersect(aShape);
    CPPUNIT_ASSERT_MESSAGE("Infinite intersected with Shape must equal Shape",
                           !aTestIntersect.IsNull());
    CPPUNIT_ASSERT_EQUAL(aShape.GetBoundRect(), aTestIntersect.GetBoundRect());

    // Union: Infinite ∪ Shape = Infinite
    vcl::Region aTestUnion = aInfiniteRegion;
    aTestUnion.Union(aShape);
    CPPUNIT_ASSERT_MESSAGE("Infinite unioned with Shape must remain Infinite", aTestUnion.IsNull());

    // Exclude: Infinite - Shape = (Cannot be easily represented, VCL usually ignores or throws)
    // But Shape - Infinite = Empty
    vcl::Region aTestExclude = aShape;
    aTestExclude.Exclude(aInfiniteRegion);
    CPPUNIT_ASSERT_MESSAGE("Shape excluding Infinite space must result in Empty space",
                           aTestExclude.IsEmpty());
}
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
