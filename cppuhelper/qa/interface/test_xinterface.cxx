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

#include <osl/interlck.h>
#include <cppu/unotype.hxx>
#include <cppuhelper/queryinterface.hxx>

#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/Type.hxx>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/lang/XEventListener.hpp>

/** Tests XInterface
    An XInterface allows you to acquire and release an interface, to get the
    interface via css::uno::Reference<TestInterface> you need to implement
    queryInterface().
*/

class TestInterface : public css::uno::XInterface
{
public:
    virtual ~TestInterface() {}
    static css::uno::Type const& static_type(void* = nullptr) // loplugin:refcounting
    {
        return cppu::UnoType<TestInterface>::get();
    }

    virtual css::uno::Any queryInterface(css::uno::Type const& rType) override
    {
        return cppu::queryInterface(rType, this);
    }
    virtual void acquire() override { osl_atomic_increment(&m_refCount); }
    virtual void release() override { osl_atomic_decrement(&m_refCount); }

    sal_uInt32 getNumber() { return 5; }

private:
    oslInterlockedCount m_refCount;
};

struct TestStruct
{
    TestStruct()
        : isDisposed(false)
    {
    }

    bool isDisposed;
};

/** Tests XComponent
    An XComponent implements an XInterface but also handles object lifecycle not only
    via dispose(), but also alerts other components that reference this component that
    it is being disposed - the other component adds an event listener, when the component
    is disposed it calls the css::lang::XEventListener::disposing() with an event that
    contains its interface.

    For this test I need to see the isDisposed flag, so I have a pointer to the struct
    which I deliberately don't destroy.
*/

class TestComponent : public TestInterface, public css::lang::XComponent
{
public:
    TestComponent() { pTest = new TestStruct(); }

    virtual ~TestComponent() {}

    // TestInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType) override
    {
        if (rType == cppu::UnoType<css::lang::XComponent>::get())
        {
            void* p = static_cast<css::lang::XComponent*>(this);
            return css::uno::Any(&p, rType);
        }

        return TestInterface::queryInterface(rType);
    }

    virtual void acquire() override { TestInterface::acquire(); }
    virtual void release() override { TestInterface::release(); }

    // XComponent
    virtual void dispose() override
    {
        for (auto& listener : aListeners)
        {
            css::lang::EventObject aEvent(static_cast<TestInterface*>(this));
            listener->disposing(aEvent);
        }

        pTest->isDisposed = true;
    }

    virtual void
    addEventListener(css::uno::Reference<css::lang::XEventListener> const& rListener) override
    {
        aListeners.push_back(rListener);
    }

    virtual void
    removeEventListener(css::uno::Reference<css::lang::XEventListener> const& rListener) override
    {
        std::remove(aListeners.begin(), aListeners.end(), rListener);
    }

    TestStruct* GetTestStruct() { return pTest; }

private:
    std::vector<css::uno::Reference<css::lang::XEventListener>> aListeners;
    TestStruct* pTest;
};

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testInterface();
    void testComponent();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testInterface);
    CPPUNIT_TEST(testComponent);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testInterface()
{
    sal_uInt32 expected = 5;
    css::uno::Reference<TestInterface> xTestInterface(new TestInterface);
    CPPUNIT_ASSERT(xTestInterface.is());
    CPPUNIT_ASSERT_EQUAL(expected, xTestInterface->getNumber());
}

void Test::testComponent()
{
    bool bExpected = true;
    css::uno::Reference<TestComponent> xTestComponent(new TestComponent);
    CPPUNIT_ASSERT(xTestComponent.is());

    TestStruct* pTest = xTestComponent->GetTestStruct();
    xTestComponent->dispose();

    CPPUNIT_ASSERT_EQUAL(bExpected, pTest->isDisposed);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
