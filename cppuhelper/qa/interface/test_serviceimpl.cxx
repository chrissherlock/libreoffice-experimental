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

#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XTypeProvider.hpp>

class CoreComponentInterfaces : public css::lang::XServiceInfo, public css::lang::XTypeProvider
{
public:
    CoreComponentInterfaces()
        : mnRefCount(0)
    {
    }

    virtual ~CoreComponentInterfaces() {}

    static css::uno::Type const& static_type(void* = nullptr);

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

private:
    oslInterlockedCount mnRefCount;
};

css::uno::Type const& CoreComponentInterfaces::static_type(void*)
{
    return cppu::UnoType<XInterface>::get();
}

css::uno::Any CoreComponentInterfaces::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
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
    css::uno::Sequence<css::uno::Type> seq(2);
    seq[0] = cppu::UnoType<css::lang::XTypeProvider>::get();
    seq[1] = cppu::UnoType<css::lang::XServiceInfo>::get();
    return seq;
}

css::uno::Sequence<sal_Int8> CoreComponentInterfaces::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
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
    sal_Int32 nExpectedSeqCount = 2;

    css::uno::Sequence<css::uno::Type> aTypes(aCoreComponentInterfaces.getTypes());
    CPPUNIT_ASSERT_EQUAL(aTypes.getLength(), nExpectedSeqCount);
    CPPUNIT_ASSERT_EQUAL(aTypes[0], cppu::UnoType<css::lang::XTypeProvider>::get());
    CPPUNIT_ASSERT_EQUAL(aTypes[1], cppu::UnoType<css::lang::XServiceInfo>::get());

    css::uno::Sequence<OUString> aServiceNames(aCoreComponentInterfaces.getSupportedServiceNames());
    CPPUNIT_ASSERT_EQUAL(aServiceNames[0],
                         OUString("org.libreoffice.test.CoreComponentInterfaces"));
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
