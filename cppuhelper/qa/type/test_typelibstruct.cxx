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
    void testDerivedStructType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testStructType);
    CPPUNIT_TEST(testDerivedStructType);
    CPPUNIT_TEST_SUITE_END();

private:
    typelib_TypeDescriptionReference* createStruct(typelib_TypeDescriptionReference* pBase,
                                                   rtl_uString* pTypeName,
                                                   typelib_TypeClass eMemberTypeClass,
                                                   rtl_uString* pMemberTypeName,
                                                   rtl_uString* pMemberName);
};

void Test::testStructType()
{
    typelib_TypeDescriptionReference* pType
        = createStruct(nullptr, OUString("TestStruct").pData, typelib_TypeClass_SHORT,
                       OUString("short").pData, OUString("mnParam1").pData);

    (void)pType;
}

void Test::testDerivedStructType()
{
    typelib_TypeDescriptionReference* pBaseType
        = createStruct(nullptr, OUString("TestStruct").pData, typelib_TypeClass_SHORT,
                       OUString("short").pData, OUString("mnParam1").pData);

    typelib_TypeDescriptionReference* pType
        = createStruct(pBaseType, OUString("TestDerivedStruct").pData, typelib_TypeClass_SHORT,
                       OUString("short").pData, OUString("mnParam2").pData);

    (void)pType;
}

typelib_TypeDescriptionReference* Test::createStruct(typelib_TypeDescriptionReference* pBase,
                                                     rtl_uString* pTypeName,
                                                     typelib_TypeClass eMemberTypeClass,
                                                     rtl_uString* pMemberTypeName,
                                                     rtl_uString* pMemberName)
{
    const sal_uInt32 nMembers = 1;

    typelib_StructMember_Init pMemberInits[nMembers];

    typelib_StructMember_Init* pMemberInit
        = static_cast<typelib_StructMember_Init*>(malloc(sizeof(typelib_StructMember_Init)));
    pMemberInit->aBase.eTypeClass = eMemberTypeClass;
    pMemberInit->aBase.pTypeName = pMemberTypeName;
    rtl_uString_acquire(pMemberInit->aBase.pTypeName);
    pMemberInit->aBase.pMemberName = pMemberName;
    rtl_uString_acquire(pMemberInit->aBase.pMemberName);
    pMemberInit->bParameterizedType = sal_False;

    pMemberInits[0] = *pMemberInit;

    typelib_TypeDescription* pType = nullptr;
    typelib_typedescription_newStruct(&pType, pTypeName, pBase, 1, pMemberInits);

    rtl_uString_release(pMemberInit->aBase.pTypeName);
    rtl_uString_release(pMemberInit->aBase.pMemberName);
    free(pMemberInit);

    typelib_typedescription_register(&pType);

    return reinterpret_cast<typelib_TypeDescriptionReference*>(pType);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
