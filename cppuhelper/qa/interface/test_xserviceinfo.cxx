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

class ServiceInfo : public css::lang::XServiceInfo
{
public:
    virtual ~ServiceInfo() {}
    static css::uno::Type const& static_type(void* = nullptr)
    {
        return cppu::UnoType<ServiceInfo>::get();
    }

    // XInterface
    virtual css::uno::Any queryInterface(css::uno::Type const& rType);
    virtual void acquire() {}
    virtual void release() {}

    // XServiceInfo
    OUString getImplementationName() { return "org.libreoffice.test.impl.ServiceInfo"; }
    sal_Bool supportsService(OUString const& rServiceName)
    {
        return cppu::supportsService(this, rServiceName);
    }
    css::uno::Sequence<OUString> getSupportedServiceNames()
    {
        return { "org.libreoffice.test.ServiceInfo" };
    }
};

css::uno::Any ServiceInfo::queryInterface(css::uno::Type const& rType)
{
    return cppu::queryInterface(rType, this);
}

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testServiceInfoType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testServiceInfoType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testServiceInfoType()
{
    ServiceInfo aServiceInfo;
    CPPUNIT_ASSERT_EQUAL(OUString("org.libreoffice.test.impl.ServiceInfo"),
                         aServiceInfo.getImplementationName());
    CPPUNIT_ASSERT(aServiceInfo.supportsService("org.libreoffice.test.ServiceInfo"));
    css::uno::Sequence<OUString> aSeq(aServiceInfo.getSupportedServiceNames());
    CPPUNIT_ASSERT_EQUAL(OUString("org.libreoffice.test.ServiceInfo"), aSeq[0]);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
