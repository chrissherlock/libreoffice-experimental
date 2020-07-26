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
#include <com/sun/star/container/XSet.hpp>

class TestEnumeration;

class TestSet : public css::container::XSet
{
    friend TestEnumeration;

public:
    virtual ~TestSet() {}
    static css::uno::Type const& static_type(void* = nullptr);

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType);
    virtual void acquire() {}
    virtual void release() {}

    // XElementAccess
    virtual css::uno::Type getElementType();
    virtual sal_Bool hasElements();

    // XEnumerationAccess
    virtual css::uno::Reference<css::container::XEnumeration> createEnumeration();

    // XSet
    sal_Bool has(css::uno::Any const& rElement);
    void insert(css::uno::Any const& rElement);
    void remove(css::uno::Any const& rElement);

private:
    std::vector<sal_Int32> maEnum;
};

css::uno::Type const& TestSet::static_type(void*) { return cppu::UnoType<TestSet>::get(); }

css::uno::Any TestSet::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

sal_Bool TestSet::has(css::uno::Any const& rElement)
{
    sal_Int32 nValue;
    rElement >>= nValue;

    auto it = std::find_if(maEnum.begin(), maEnum.end(),
                           [nValue](sal_Int32 number) { return (number == nValue); });

    return (it != maEnum.end());
}

void TestSet::insert(css::uno::Any const& rElement)
{
    if (rElement.getValueType() != css::uno::Type(css::uno::TypeClass_LONG, "long"))
        throw css::lang::IllegalArgumentException();

    if (has(rElement))
        throw css::container::ElementExistException();

    sal_Int32 nValue;
    rElement >>= nValue;
    maEnum.push_back(nValue);
}

void TestSet::remove(css::uno::Any const& rElement)
{
    if (rElement.getValueType() != css::uno::Type(css::uno::TypeClass_LONG, "long"))
        throw css::lang::IllegalArgumentException();

    sal_Int32 nValue;
    rElement >>= nValue;

    if (!has(rElement))
        throw css::container::NoSuchElementException();

    auto it = std::remove_if(maEnum.begin(), maEnum.end(),
                             [nValue](sal_Int32 const& i) { return i == nValue; });

    maEnum.erase(it);
}

css::uno::Type TestSet::getElementType()
{
    return css::uno::Type(css::uno::TypeClass_LONG, "long");
}

sal_Bool TestSet::hasElements() { return (maEnum.size() != 0); }

class TestEnumeration : public css::container::XEnumeration
{
public:
    friend TestSet;

    TestEnumeration(css::uno::Reference<TestSet> xSet);
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
    css::uno::Reference<TestSet> mxSet;
    sal_uInt32 mnIndex;
};

css::uno::Reference<css::container::XEnumeration> TestSet::createEnumeration()
{
    return css::uno::Reference<css::container::XEnumeration>(
        new TestEnumeration(css::uno::Reference(this)));
}

TestEnumeration::TestEnumeration(css::uno::Reference<TestSet> xSet)
    : mnIndex(0)
{
    mxSet.set(xSet);
}

css::uno::Type const& TestEnumeration::static_type(void*)
{
    return cppu::UnoType<TestEnumeration>::get();
}

css::uno::Any TestEnumeration::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

sal_Bool TestEnumeration::hasMoreElements() { return mnIndex <= mxSet->maEnum.size(); }

css::uno::Any TestEnumeration::nextElement()
{
    ++mnIndex;

    if (mnIndex > mxSet->maEnum.size())
    {
        throw css::container::NoSuchElementException();
    }

    css::uno::Any aElement(mxSet->maEnum[mnIndex - 1]);
    return aElement;
}

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testTestSetType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testTestSetType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testTestSetType()
{
    TestSet aSet;
    CPPUNIT_ASSERT_EQUAL(aSet.getElementType(), css::uno::Type(css::uno::TypeClass_LONG, "long"));
    CPPUNIT_ASSERT(!aSet.hasElements());

    CPPUNIT_ASSERT_NO_THROW(aSet.insert(css::uno::Any(1)));
    CPPUNIT_ASSERT(aSet.hasElements());
    CPPUNIT_ASSERT(aSet.has(css::uno::Any(1)));
    CPPUNIT_ASSERT_THROW(aSet.insert(css::uno::Any(1)), css::container::ElementExistException);

    CPPUNIT_ASSERT_NO_THROW(aSet.insert(css::uno::Any(2)));
    CPPUNIT_ASSERT_NO_THROW(aSet.remove(css::uno::Any(2)));
    CPPUNIT_ASSERT(!aSet.has(css::uno::Any(2)));
    CPPUNIT_ASSERT_THROW(aSet.remove(css::uno::Any(2)), css::container::NoSuchElementException);

    CPPUNIT_ASSERT_THROW(aSet.insert(css::uno::Any(OUString("string"))),
                         css::lang::IllegalArgumentException);
    CPPUNIT_ASSERT_THROW(aSet.remove(css::uno::Any(OUString("string"))),
                         css::lang::IllegalArgumentException);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
