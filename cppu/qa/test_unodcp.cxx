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
#include <cppu/EnvDcp.hxx>

namespace
{
class Test : public CppUnit::TestFixture
{
public:
    /** Test environment descriptor's type name

        Environment types are named via the format <OBI>[:<purpose>]*",
        The type name is the OBI (Object Binary Interface)
    */
    void testUnoEnvDcpTypeName();

    /** Test environment descriptor's purpose

        Environment types are named via the format <OBI>[:<purpose>]*",
        The  purpose is what the environment is used for.
    */
    void testUnoEnvDcpPurpose();

    /** Test environment descriptor's chained purpose

        Environment types are named via the format <OBI>[:<purpose>]*",
        The purpose is what the environment is used for, and can be
        chained.
    */
    void testUnoEnvDcpChainedPurpose();

    CPPUNIT_TEST_SUITE(Test);
    CPPUNIT_TEST(testUnoEnvDcpTypeName);
    CPPUNIT_TEST(testUnoEnvDcpPurpose);
    CPPUNIT_TEST(testUnoEnvDcpChainedPurpose);
    CPPUNIT_TEST_SUITE_END();
};

void Test::testUnoEnvDcpTypeName()
{
    OUString sEnvDcp("gcc3:debug");
    OUString sEnvDcpExpectedTypeName("gcc3");

    rtl_uString* pEnvTypeName = nullptr;
    uno_EnvDcp_getTypeName(sEnvDcp.pData, &pEnvTypeName);

    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedTypeName, OUString(pEnvTypeName));
    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedTypeName, cppu::EnvDcp::getTypeName(sEnvDcp));
}

void Test::testUnoEnvDcpPurpose()
{
    OUString sEnvDcp("gcc3:debug");
    OUString sEnvDcpExpectedPurpose(":debug");

    rtl_uString* pEnvPurpose = nullptr;
    uno_EnvDcp_getPurpose(sEnvDcp.pData, &pEnvPurpose);

    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedPurpose, OUString(pEnvPurpose));
    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedPurpose, cppu::EnvDcp::getPurpose(sEnvDcp));
}

void Test::testUnoEnvDcpChainedPurpose()
{
    OUString sEnvDcp("uno:log:affine");
    OUString sEnvDcpExpectedPurpose(":log:affine");

    rtl_uString* pEnvPurpose = nullptr;
    uno_EnvDcp_getPurpose(sEnvDcp.pData, &pEnvPurpose);

    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedPurpose, OUString(pEnvPurpose));
    CPPUNIT_ASSERT_EQUAL(sEnvDcpExpectedPurpose, cppu::EnvDcp::getPurpose(sEnvDcp));
}

CPPUNIT_TEST_SUITE_REGISTRATION(Test);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
