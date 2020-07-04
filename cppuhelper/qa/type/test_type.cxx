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

#include <com/sun/star/uno/Type.hxx>

namespace
{
class Test : public ::CppUnit::TestFixture
{
public:
    void testVoidType();
    void testCharType();
    void testBooleanType();
    void testByteType();
    void testShortType();
    void testUnsignedShortType();
    void testLongType();
    void testUnsignedLongType();
    void testHyperType();
    void testUnsignedHyperType();
    void testFloatType();
    void testDoubleType();
    void testStringType();
    void testTypeType();
    void testAnyType();
    void testInterfaceType();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testVoidType);
    CPPUNIT_TEST(testCharType);
    CPPUNIT_TEST(testBooleanType);
    CPPUNIT_TEST(testByteType);
    CPPUNIT_TEST(testShortType);
    CPPUNIT_TEST(testUnsignedLongType);
    CPPUNIT_TEST(testHyperType);
    CPPUNIT_TEST(testUnsignedHyperType);
    CPPUNIT_TEST(testFloatType);
    CPPUNIT_TEST(testDoubleType);
    CPPUNIT_TEST(testStringType);
    CPPUNIT_TEST(testTypeType);
    CPPUNIT_TEST(testAnyType);
    CPPUNIT_TEST(testInterfaceType);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testVoidType()
{
    css::uno::Type aType;
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_VOID, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("void"), aType.getTypeName());
    CPPUNIT_ASSERT(aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testCharType()
{
    css::uno::Type aType(css::uno::TypeClass_CHAR, "char");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_CHAR, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("char"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testBooleanType()
{
    css::uno::Type aType(css::uno::TypeClass_BOOLEAN, "boolean");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_BOOLEAN, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("boolean"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testByteType()
{
    css::uno::Type aType(css::uno::TypeClass_BYTE, "byte");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_BYTE, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("byte"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testShortType()
{
    css::uno::Type aType(css::uno::TypeClass_SHORT, "short");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_SHORT, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("short"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testUnsignedShortType()
{
    css::uno::Type aType(css::uno::TypeClass_UNSIGNED_SHORT, "unsigned short");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned short"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testLongType()
{
    css::uno::Type aType(css::uno::TypeClass_SHORT, "short");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_SHORT, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("short"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testUnsignedLongType()
{
    css::uno::Type aType(css::uno::TypeClass_UNSIGNED_LONG, "unsigned long");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned long"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testHyperType()
{
    css::uno::Type aType(css::uno::TypeClass_HYPER, "hyper");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_HYPER, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("hyper"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testUnsignedHyperType()
{
    css::uno::Type aType(css::uno::TypeClass_UNSIGNED_HYPER, "unsigned hyper");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("unsigned hyper"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testFloatType()
{
    css::uno::Type aType(css::uno::TypeClass_FLOAT, "float");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_FLOAT, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("float"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testDoubleType()
{
    css::uno::Type aType(css::uno::TypeClass_DOUBLE, "double");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_DOUBLE, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("double"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testStringType()
{
    css::uno::Type aType(css::uno::TypeClass_STRING, "string");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_STRING, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("string"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testTypeType()
{
    css::uno::Type aType(css::uno::TypeClass_TYPE, "type");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_TYPE, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("type"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testAnyType()
{
    css::uno::Type aType(css::uno::TypeClass_ANY, "any");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_ANY, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("any"), aType.getTypeName());
    CPPUNIT_ASSERT(aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
    CPPUNIT_ASSERT(aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_INTERFACE, "com.sun.star.uno.XInterface")));
}

void Test::testInterfaceType()
{
    css::uno::Type aType(css::uno::TypeClass_INTERFACE, "com.sun.star.uno.XInterface");
    CPPUNIT_ASSERT_EQUAL(css::uno::TypeClass::TypeClass_INTERFACE, aType.getTypeClass());
    CPPUNIT_ASSERT_EQUAL(OUString("com.sun.star.uno.XInterface"), aType.getTypeName());
    CPPUNIT_ASSERT(!aType.isAssignableFrom(css::uno::Type()));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_CHAR, "char")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BYTE, "byte")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_SHORT, "short")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_SHORT, "unsigned short")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_LONG, "long")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_LONG, "unsigned long")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_HYPER, "hyper")));
    CPPUNIT_ASSERT(!aType.isAssignableFrom(
        css::uno::Type(css::uno::TypeClass::TypeClass_UNSIGNED_HYPER, "unsigned hyper")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_FLOAT, "float")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_DOUBLE, "double")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_STRING, "string")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_TYPE, "type")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_ANY, "any")));
    CPPUNIT_ASSERT(
        !aType.isAssignableFrom(css::uno::Type(css::uno::TypeClass::TypeClass_BOOLEAN, "boolean")));
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
