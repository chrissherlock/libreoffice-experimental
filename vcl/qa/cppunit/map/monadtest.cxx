/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <unotest/bootstrapfixturebase.hxx>

#include <vcl/TransformTypes.hxx>
#include <tools/gen.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMapFunctorChaining)
{
    // Setup a baseline logic size: 10x20
    vcl::LogicSize aLogicSize(Size(10, 20));

    // Use map to multiply dimensions (Functor translation layer)
    auto aMapped = aLogicSize.map(
        [](const Size& rSize) { return Size(rSize.Width() * 2, rSize.Height() * 3); });

    // Assert structural type consistency and value mapping: 20x60
    CPPUNIT_ASSERT_EQUAL(tools::Long(20), aMapped.get().Width());
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aMapped.get().Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAndThenLeftIdentity)
{
    // Monadic Law 1: unit(x).and_then(f) == f(x)
    Size aRawSize(100, 200);
    auto f = [](const Size& s) { return vcl::LogicSize(Size(s.Width() + 10, s.Height() + 20)); };

    vcl::LogicSize aWrapped(aRawSize);
    auto aMonadicResult = aWrapped.and_then(f);
    auto aDirectResult = f(aRawSize);

    CPPUNIT_ASSERT_EQUAL(aDirectResult.get().Width(), aMonadicResult.get().Width());
    CPPUNIT_ASSERT_EQUAL(aDirectResult.get().Height(), aMonadicResult.get().Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAndThenRightIdentity)
{
    // Monadic Law 2: m.and_then(unit) == m
    vcl::LogicSize m(Size(50, 50));

    auto aResult = m.and_then([](const Size& s) { return vcl::LogicSize(s); });

    CPPUNIT_ASSERT_EQUAL(m.get().Width(), aResult.get().Width());
    CPPUNIT_ASSERT_EQUAL(m.get().Height(), aResult.get().Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAndThenAssociativity)
{
    // Monadic Law 3: m.and_then(f).and_then(g) == m.and_then([Base](x) { return f(x).and_then(g); })
    vcl::LogicSize m(Size(10, 10));

    auto f = [](const Size& s) { return vcl::LogicSize(Size(s.Width() * 2, s.Height() * 2)); };
    auto g = [](const Size& s) { return vcl::LogicSize(Size(s.Width() + 5, s.Height() + 5)); };

    // Left-hand side chaining pipeline
    auto aLhs = m.and_then(f).and_then(g);

    // Right-hand side nested evaluation pass
    auto aRhs = m.and_then([&f, &g](const Size& s) { return f(s).and_then(g); });

    CPPUNIT_ASSERT_EQUAL(aLhs.get().Width(), aRhs.get().Width());
    CPPUNIT_ASSERT_EQUAL(aLhs.get().Height(), aRhs.get().Height());
    CPPUNIT_ASSERT_EQUAL(tools::Long(25), aLhs.get().Width());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
