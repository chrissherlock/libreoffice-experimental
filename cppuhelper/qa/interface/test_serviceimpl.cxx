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

#include <osl/mutex.hxx>
#include <rtl/ustring.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/queryinterface.hxx>

#include <com/sun/star/uno/XWeak.hpp>
#include <com/sun/star/uno/XAdapter.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XTypeProvider.hpp>

class CoreComponentInterfaces;

static osl::Mutex& getWeakMutex()
{
    static osl::Mutex* spMutex = new osl::Mutex();
    return *spMutex;
}

class Adapter : public css::uno::XAdapter
{
public:
    explicit Adapter(CoreComponentInterfaces* pWeakObj)
        : mnRefCount(0)
        , mpWeakObject(pWeakObj)
    {
    }

    virtual ~Adapter() {}

    Adapter(Adapter const&) = delete;
    Adapter const& operator=(Adapter const&) = delete;

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType) override;
    virtual void acquire() override;
    virtual void release() override;

    // XAdapter
    virtual css::uno::Reference<css::uno::XInterface> queryAdapted() override;
    virtual void addReference(css::uno::Reference<css::uno::XReference> const& xRef) override;
    virtual void removeReference(css::uno::Reference<css::uno::XReference> const& xRef) override;

private:
    oslInterlockedCount mnRefCount;
    CoreComponentInterfaces* mpWeakObject;
    std::vector<css::uno::Reference<css::uno::XReference>> maReferences;
};

class CoreComponentInterfaces : public css::lang::XServiceInfo,
                                public css::lang::XTypeProvider,
                                public css::uno::XWeak
{
    friend class Adapter;

public:
    CoreComponentInterfaces()
        : mnRefCount(0)
        , mpAdapter(nullptr)
    {
    }

    virtual ~CoreComponentInterfaces() {}

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType);
    virtual void acquire();
    virtual void release();

    // XServiceInfo
    virtual OUString getImplementationName();
    virtual sal_Bool supportsService(OUString const& rServiceName);
    virtual css::uno::Sequence<OUString> getSupportedServiceNames();

    // XTypeProvider
    virtual css::uno::Sequence<css::uno::Type> getTypes();
    virtual css::uno::Sequence<sal_Int8> getImplementationId(); // deprecated

    // XWeak
    virtual css::uno::Reference<css::uno::XAdapter> queryAdapter();

private:
    oslInterlockedCount mnRefCount;
    Adapter* mpAdapter;
};

css::uno::Any Adapter::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

void Adapter::acquire() { osl_atomic_increment(&mnRefCount); }

void Adapter::release()
{
    if (!osl_atomic_decrement(&mnRefCount))
        delete this;
}

css::uno::Reference<css::uno::XInterface> Adapter::queryAdapted()
{
    css::uno::Reference<CoreComponentInterfaces> ret;

    osl::ClearableMutexGuard guard(getWeakMutex());

    if (mpWeakObject)
    {
        oslInterlockedCount n = osl_atomic_increment(&mpWeakObject->mnRefCount);

        if (n > 1)
        {
            guard.clear();
            ret = mpWeakObject;
            osl_atomic_decrement(&mpWeakObject->mnRefCount);
        }
        else
        {
            osl_atomic_decrement(&mpWeakObject->mnRefCount);
        }
    }

    return ret;
}

void Adapter::addReference(css::uno::Reference<css::uno::XReference> const& rRef)
{
    osl::MutexGuard aGuard(getWeakMutex());
    maReferences.push_back(rRef);
}

void Adapter::removeReference(css::uno::Reference<css::uno::XReference> const& rRef)
{
    osl::MutexGuard aGuard(getWeakMutex());

    auto it = std::find(maReferences.rbegin(), maReferences.rend(), rRef);

    if (it != maReferences.rend())
        maReferences.erase(it.base() - 1);
}

css::uno::Any CoreComponentInterfaces::queryInterface(css::uno::Type const& rType)
{
    if (rType.equals(cppu::UnoType<css::lang::XTypeProvider>::get())
        || rType.equals(cppu::UnoType<css::uno::XInterface>::get()))
    {
        css::uno::Reference<css::uno::XInterface> ref(static_cast<css::lang::XTypeProvider*>(this));
        return makeAny(ref);
    }

    if (rType.equals(cppu::UnoType<css::lang::XServiceInfo>::get()))
    {
        css::uno::Reference<css::lang::XServiceInfo> ref(
            static_cast<css::lang::XServiceInfo*>(this));
        return makeAny(ref);
    }

    return css::uno::Any();
}

void CoreComponentInterfaces::acquire() { osl_atomic_increment(&mnRefCount); }

void CoreComponentInterfaces::release()
{
    if (osl_atomic_decrement(&mnRefCount) == 0)
        delete this;
}

OUString CoreComponentInterfaces::getImplementationName()
{
    return "org.libreoffice.test.impl.CoreComponentInterfaces";
}

sal_Bool CoreComponentInterfaces::supportsService(OUString const& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

css::uno::Sequence<OUString> CoreComponentInterfaces::getSupportedServiceNames()
{
    return { "org.libreoffice.test.CoreComponentInterfaces" };
}

css::uno::Sequence<css::uno::Type> CoreComponentInterfaces::getTypes()
{
    css::uno::Sequence<css::uno::Type> seq(3);
    seq[0] = cppu::UnoType<css::lang::XTypeProvider>::get();
    seq[1] = cppu::UnoType<css::lang::XServiceInfo>::get();
    seq[2] = cppu::UnoType<css::uno::XWeak>::get();
    return seq;
}

css::uno::Sequence<sal_Int8> CoreComponentInterfaces::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

css::uno::Reference<css::uno::XAdapter> CoreComponentInterfaces::queryAdapter()
{
    if (!mpAdapter)
    {
        osl::MutexGuard aGuard(getWeakMutex());

        if (!mpAdapter)
        {
            Adapter* pAdapter = new Adapter(this);
            pAdapter->acquire();
            mpAdapter = pAdapter;
        }
    }

    return mpAdapter;
}

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testCCIServiceInfo();
    void testCCITypeProvider();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testCCIServiceInfo);
    CPPUNIT_TEST(testCCITypeProvider);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testCCIServiceInfo()
{
    CoreComponentInterfaces aCoreComponentInterfaces;
    CPPUNIT_ASSERT_EQUAL(OUString("org.libreoffice.test.impl.CoreComponentInterfaces"),
                         aCoreComponentInterfaces.getImplementationName());
    CPPUNIT_ASSERT(
        aCoreComponentInterfaces.supportsService("org.libreoffice.test.CoreComponentInterfaces"));
    css::uno::Sequence<OUString> aSeq(aCoreComponentInterfaces.getSupportedServiceNames());
    CPPUNIT_ASSERT_EQUAL(OUString("org.libreoffice.test.CoreComponentInterfaces"), aSeq[0]);
}

void Test::testCCITypeProvider()
{
    CoreComponentInterfaces aCoreComponentInterfaces;
    sal_Int32 nExpectedSeqCount = 3;

    css::uno::Sequence<css::uno::Type> aTypes(aCoreComponentInterfaces.getTypes());
    CPPUNIT_ASSERT_EQUAL(aTypes.getLength(), nExpectedSeqCount);
    CPPUNIT_ASSERT_EQUAL(aTypes[0], cppu::UnoType<css::lang::XTypeProvider>::get());
    CPPUNIT_ASSERT_EQUAL(aTypes[1], cppu::UnoType<css::lang::XServiceInfo>::get());
    CPPUNIT_ASSERT_EQUAL(aTypes[2], cppu::UnoType<css::uno::XWeak>::get());

    css::uno::Sequence<OUString> aServiceNames(aCoreComponentInterfaces.getSupportedServiceNames());
    CPPUNIT_ASSERT_EQUAL(aServiceNames[0],
                         OUString("org.libreoffice.test.CoreComponentInterfaces"));
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
