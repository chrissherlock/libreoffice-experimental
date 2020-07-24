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

#include <sal/log.hxx>
#include <rtl/ustring.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/queryinterface.hxx>

#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/container/XEnumeration.hpp>

class TestEnumeration : public css::container::XEnumeration
{
public:
    TestEnumeration();
    virtual ~TestEnumeration() {}
    static css::uno::Type const& static_type(void* = nullptr);

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType);
    virtual void acquire() {}
    virtual void release() {}

    // XEnumeration
    sal_Bool hasMoreElements();
    css::uno::Any nextElement();

private:
    std::vector<sal_Int32> maEnum;
    sal_uInt32 mnIndex;
};

TestEnumeration::TestEnumeration()
    : mnIndex(0)
{
    maEnum.push_back(1);
    maEnum.push_back(3);
    maEnum.push_back(4);
}

css::uno::Type const& TestEnumeration::static_type(void*)
{
    return cppu::UnoType<TestEnumeration>::get();
}

css::uno::Any TestEnumeration::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

sal_Bool TestEnumeration::hasMoreElements() { return mnIndex <= maEnum.size(); }

css::uno::Any TestEnumeration::nextElement()
{
    ++mnIndex;

    if (mnIndex > maEnum.size())
    {
        throw css::container::NoSuchElementException();
    }

    css::uno::Any aElement(maEnum[mnIndex - 1]);
    return aElement;
}

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testTestEnumerationType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testTestEnumerationType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testTestEnumerationType()
{
    TestEnumeration aTestEnumeration;

    sal_Int32 nExpected = 1;
    sal_Int32 nActual;
    aTestEnumeration.nextElement() >>= nActual;

    CPPUNIT_ASSERT(aTestEnumeration.hasMoreElements());
    CPPUNIT_ASSERT_EQUAL(nExpected, nActual);

    nExpected = 3;
    aTestEnumeration.nextElement() >>= nActual;

    CPPUNIT_ASSERT(aTestEnumeration.hasMoreElements());
    CPPUNIT_ASSERT_EQUAL(nExpected, nActual);

    nExpected = 4;
    aTestEnumeration.nextElement() >>= nActual;

    CPPUNIT_ASSERT(aTestEnumeration.hasMoreElements());
    CPPUNIT_ASSERT_EQUAL(nExpected, nActual);

    CPPUNIT_ASSERT_THROW(aTestEnumeration.nextElement(), css::container::NoSuchElementException);
    CPPUNIT_ASSERT(!aTestEnumeration.hasMoreElements());
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
