/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <cppunit/TestFixture.h>
#include <cppunit/plugin/TestPlugIn.h>
#include <cppunit/extensions/HelperMacros.h>

#include <sal/types.h>
#include <rtl/ustring.hxx>
#include <uno/environment.hxx>
#include <uno/lbnames.h>

namespace
{
class Test : public CppUnit::TestFixture
{
public:
    void testUnoEnvironment();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testUnoEnvironment);
    CPPUNIT_TEST_SUITE_END();
};

static void testFunc(va_list* pParam)
{
    sal_uInt32* pnTestNum = va_arg(*pParam, sal_uInt32*);
    *pnTestNum = 5;
}

void Test::testUnoEnvironment()
{
    uno_Environment* pEnv = nullptr;
    uno_getCurrentEnvironment(&pEnv, OUString(CPPU_CURRENT_LANGUAGE_BINDING_NAME).pData);
    uno_dumpEnvironment(stderr, pEnv, nullptr);

    css::uno::Environment aEnv(pEnv);
    CPPUNIT_ASSERT_EQUAL(OUString(CPPU_CURRENT_LANGUAGE_BINDING_NAME), aEnv.getTypeName());

    uno_Environment_enter(pEnv);
    CPPUNIT_ASSERT(uno_Environment_isValid(pEnv, nullptr));

    const sal_uInt32 nExpected = 5;
    sal_uInt32* pnTest = static_cast<sal_uInt32*>(malloc(sizeof(sal_uInt32)));
    *pnTest = 0;
    uno_Environment_invoke(pEnv, testFunc, pnTest);
    CPPUNIT_ASSERT_EQUAL(*pnTest, nExpected);

    *pnTest = 0;
    aEnv.invoke(testFunc, pnTest);
    CPPUNIT_ASSERT_EQUAL(*pnTest, nExpected);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
