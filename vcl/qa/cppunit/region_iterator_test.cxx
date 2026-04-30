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

// PROVES: Copying a region does not blindly copy the unique_ptr cache,
// ensuring that target cache is cleanly rebuilt on demand.

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCopySemantics)
{
    tools::Rectangle aRect(0, 0, 100, 100);
    vcl::Region aSource(aRect);

    // Prime the cache on the source
    auto it1 = aSource.begin();
    CPPUNIT_ASSERT_EQUAL(aRect, *it1);

    // Copy construct
    vcl::Region aTarget(aSource);

    // Verify target iterator works and cache wasn't blindly shared
    auto it2 = aTarget.begin();
    CPPUNIT_ASSERT_EQUAL(aRect, *it2);

    // Address check to ensure deep geometric isolation (not sharing the same cache vector)
    const uintptr_t nSourceAddr = reinterpret_cast<uintptr_t>(&(*it1));
    const uintptr_t nTargetAddr = reinterpret_cast<uintptr_t>(&(*it2));
    CPPUNIT_ASSERT_MESSAGE("Copy must not share underlying cache memory",
                           nSourceAddr != nTargetAddr);
}

// PROVES: Move semantics cleanly transfer the geometry and completely
// obliterate the source cache to prevent stale reads if the source is reused.

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMoveSemantics)
{
    tools::Rectangle aRect(5, 5, 20, 20);
    vcl::Region aSource(aRect);

    // Prime source cache
    CPPUNIT_ASSERT(aSource.begin() != aSource.end());

    // Move construct
    vcl::Region aTarget(std::move(aSource));

    // Verify target adopted the geometry safely and cache rebuilds
    auto itTarget = aTarget.begin();
    CPPUNIT_ASSERT_EQUAL(aRect, *itTarget);

    // Verify source cache was obliterated and it acts as an empty region
    CPPUNIT_ASSERT_MESSAGE("Moved-from source must have an empty cache",
                           aSource.begin() == aSource.end());

    // Test Move Assignment
    vcl::Region aAssignTarget;
    aAssignTarget = std::move(aTarget);

    // Verify assigned target works
    auto itAssignTarget = aAssignTarget.begin();
    CPPUNIT_ASSERT_EQUAL(aRect, *itAssignTarget);

    // Verify the assigned-from source was cleared
    CPPUNIT_ASSERT_MESSAGE("Move-assigned source must have an empty cache",
                           aTarget.begin() == aTarget.end());
}
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
