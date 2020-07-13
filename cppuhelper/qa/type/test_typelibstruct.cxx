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
#include <typelib/typedescription.h>

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testStructType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testStructType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testStructType()
{
    typelib_TypeDescription* pType = nullptr;
    rtl_uString* sType = nullptr;
    rtl_uString_newFromAscii(&sType, "TestStruct\0");
    const sal_uInt32 nMembers = 1;

    typelib_StructMember_Init pMemberInits[nMembers];

    typelib_StructMember_Init* pMemberInit
        = static_cast<typelib_StructMember_Init*>(alloca(sizeof(typelib_StructMember_Init)));
    pMemberInit->aBase.eTypeClass = typelib_TypeClass_SHORT;
    pMemberInit->aBase.pTypeName = nullptr;
    rtl_uString_newFromAscii(&pMemberInit->aBase.pTypeName, "short\0");
    rtl_uString_acquire(pMemberInit->aBase.pTypeName);
    rtl_uString_newFromAscii(&pMemberInit->aBase.pMemberName, "mnParam1\0");
    rtl_uString_acquire(pMemberInit->aBase.pMemberName);
    pMemberInit->bParameterizedType = sal_False;

    pMemberInits[0] = *pMemberInit;

    typelib_typedescription_newStruct(&pType, sType, nullptr, 1, pMemberInits);

    rtl_uString_release(pMemberInit->aBase.pTypeName);
    rtl_uString_release(pMemberInit->aBase.pMemberName);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
