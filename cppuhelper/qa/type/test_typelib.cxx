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
#include <typelib/typedescription.h>

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testTypelibNames();
    void testNewEnum();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testTypelibNames);
    CPPUNIT_TEST(testNewEnum);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testTypelibNames()
{
    typelib_TypeDescriptionReference* pType
        = *typelib_static_type_getByTypeClass(typelib_TypeClass_VOID);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("void"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_BOOLEAN);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("boolean"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_CHAR);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("char"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_BYTE);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("byte"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_SHORT);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("short"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_LONG);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("long"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_HYPER);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("hyper"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_UNSIGNED_SHORT);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned short"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_UNSIGNED_LONG);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned long"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_UNSIGNED_HYPER);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned hyper"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_FLOAT);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("float"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_DOUBLE);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("double"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);

    pType = *typelib_static_type_getByTypeClass(typelib_TypeClass_STRING);
    typelib_typedescriptionreference_acquire(pType);
    CPPUNIT_ASSERT_EQUAL(OUString("string"), OUString(pType->pTypeName));
    typelib_typedescriptionreference_release(pType);
}

void Test::testNewEnum()
{
    // create an array of strings { "enum1", "enum2" }
    rtl_uString* sEnumName1 = nullptr;
    rtl_uString_newFromAscii(&sEnumName1, "enum1\0");

    rtl_uString* sEnumName2 = nullptr;
    rtl_uString_newFromAscii(&sEnumName2, "enum2\0");

    rtl_uString** pEnumNames;
    pEnumNames = (rtl_uString**)malloc(sizeof(struct _rtl_uString*) * 2);
    pEnumNames[0] = sEnumName1;
    pEnumNames[1] = sEnumName2;

    // create an array of corresponding enum values for each name (this *could* be { 1, 3 })
    sal_Int32 pEnumValues[2] = { 1, 2 };

    // now we create the new enumerator type description called "testenum"
    typelib_TypeDescription* pType = nullptr;
    typelib_typedescription_newEnum(&pType, OUString("testenum").pData, 1, 1, pEnumNames,
                                    pEnumValues);

    // we need to register this new enum type
    typelib_typedescription_register(&pType);

    // now we need to get a reference to the type
    typelib_TypeDescriptionReference* pTypeRef = nullptr;
    typelib_typedescriptionreference_new(&pTypeRef, typelib_TypeClass_ENUM,
                                         OUString("testenum").pData);

    typelib_typedescriptionreference_acquire(pTypeRef);
    CPPUNIT_ASSERT_EQUAL(OUString("testenum"), OUString(pTypeRef->pTypeName));
    typelib_typedescriptionreference_release(pTypeRef);

    rtl_uString_release(sEnumName1);
    rtl_uString_release(sEnumName2);
    free(pEnumNames);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
