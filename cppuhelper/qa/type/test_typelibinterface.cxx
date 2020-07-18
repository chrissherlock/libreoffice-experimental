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
    void testInterfaceType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testInterfaceType);
    CPPUNIT_TEST_SUITE_END();

private:
    typelib_TypeDescriptionReference* createInterface(rtl_uString* pTypeName,
                                                      typelib_TypeClass eMemberTypeClass,
                                                      rtl_uString* pMemberTypeName,
                                                      rtl_uString* pMemberName);
    typelib_InterfaceMethodTypeDescription*
    createInterfaceMethod(sal_Int32 nAbsolutePosition, sal_Bool bOneWay, rtl_uString* pMethodName,
                          typelib_TypeClass eReturnTypeClass, rtl_uString* pReturnTypeName,
                          sal_Int32 nParams, typelib_Parameter_Init* pParams, sal_Int32 nExceptions,
                          rtl_uString** ppExceptionNames);
};

void Test::testInterfaceType() {}

typelib_InterfaceMethodTypeDescription*
Test::createInterfaceMethod(sal_Int32 nAbsolutePosition, sal_Bool bOneWay, rtl_uString* pMethodName,
                            typelib_TypeClass eReturnTypeClass, rtl_uString* pReturnTypeName,
                            sal_Int32 nParams, typelib_Parameter_Init* pParams,
                            sal_Int32 nExceptions, rtl_uString** ppExceptionNames)
{
    typelib_InterfaceMethodTypeDescription* pMethod = nullptr;
    typelib_typedescription_newInterfaceMethod(&pMethod, nAbsolutePosition, bOneWay, pMethodName,
                                               eReturnTypeClass, pReturnTypeName, nParams, pParams,
                                               nExceptions, ppExceptionNames);
    return pMethod;
}

typelib_TypeDescriptionReference* Test::createInterface(rtl_uString* pTypeName,
                                                        typelib_TypeClass eMemberTypeClass,
                                                        rtl_uString* pMemberTypeName,
                                                        rtl_uString* pMemberName)
{
    const sal_Int32 nInterfaceMembers = 1;

    typelib_InterfaceMethodTypeDescription* pMethod = createInterfaceMethod(
        0, sal_True, pMemberTypeName, eMemberTypeClass, pMemberName, 0, nullptr, 0, nullptr);
    typelib_TypeDescriptionReference* ppMembers[nInterfaceMembers];
    ppMembers[0] = reinterpret_cast<typelib_TypeDescriptionReference*>(pMethod);

    typelib_InterfaceTypeDescription* pType = nullptr;
    typelib_typedescription_newMIInterface(&pType, pTypeName, 0, 0, 0, 0, 0, 0, nullptr, 1,
                                           ppMembers);

    typelib_typedescription_register(reinterpret_cast<typelib_TypeDescription**>(&pType));

    return reinterpret_cast<typelib_TypeDescriptionReference*>(pType);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
