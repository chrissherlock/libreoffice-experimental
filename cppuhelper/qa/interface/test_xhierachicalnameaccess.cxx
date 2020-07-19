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

#include <rtl/ustring.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/queryinterface.hxx>

#include <com/sun/star/container/XHierarchicalNameAccess.hpp>

#include <vector>
#include <utility>

class TestHierarchicalNameAccess : public css::container::XHierarchicalNameAccess
{
public:
    TestHierarchicalNameAccess();
    virtual ~TestHierarchicalNameAccess() {}

    static css::uno::Type const& static_type(void* = nullptr)
    {
        return cppu::UnoType<TestHierarchicalNameAccess>::get();
    }

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType) override;
    virtual void acquire() override {}
    virtual void release() override {}

    // XHierarchicalNameAccess
    virtual css::uno::Any getByHierarchicalName(OUString const& rName) override;
    virtual sal_Bool hasByHierarchicalName(OUString const& rName) override;

private:
    std::vector<std::pair<OUString, OUString>> maNames;
};

TestHierarchicalNameAccess::TestHierarchicalNameAccess()
{
    std::pair aPair1(OUString("name1"), OUString("value1"));
    std::pair aPair2(OUString("name2"), OUString("value2"));
    maNames.push_back(aPair1);
    maNames.push_back(aPair2);
}

css::uno::Any TestHierarchicalNameAccess::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

css::uno::Any TestHierarchicalNameAccess::getByHierarchicalName(OUString const& rName)
{
    auto it = std::find_if(
        maNames.begin(), maNames.end(),
        [&rName](std::pair<OUString, OUString> const& element) { return element.first == rName; });

    if (it != maNames.end())
        return css::uno::makeAny(it->second);
    else
        return css::uno::Any();
}

sal_Bool TestHierarchicalNameAccess::hasByHierarchicalName(OUString const& rName)
{
    css::uno::Any result = getByHierarchicalName(rName);
    return result.has<OUString>();
}

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testHierarchicalNameAccessType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testHierarchicalNameAccessType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testHierarchicalNameAccessType()
{
    TestHierarchicalNameAccess aHierarchicalNameAccess;

    OUString aResult;
    aHierarchicalNameAccess.getByHierarchicalName(OUString("name1")) >>= aResult;
    CPPUNIT_ASSERT_EQUAL(OUString("value1"), aResult);

    OUString aNoResult;
    aHierarchicalNameAccess.getByHierarchicalName(OUString("nonexistent")) >>= aNoResult;
    CPPUNIT_ASSERT_EQUAL(OUString(), aNoResult);

    CPPUNIT_ASSERT(aHierarchicalNameAccess.hasByHierarchicalName(OUString("name1")));
    CPPUNIT_ASSERT(!aHierarchicalNameAccess.hasByHierarchicalName(OUString("nonexistent")));
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
