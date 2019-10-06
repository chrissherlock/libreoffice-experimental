/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <test/bootstrapfixture.hxx>

#include <vcl/virdev.hxx>

class PixelTest : public test::BootstrapFixture
{
public:
    PixelTest()
        : BootstrapFixture(true, false)
    {
    }
    void testPixel();

    CPPUNIT_TEST_SUITE(PixelTest);
    CPPUNIT_TEST(testPixel);

    CPPUNIT_TEST_SUITE_END();
};

void PixelTest::testPixel()
{
    VirtualDevice aRenderContext;

    aRenderContext.DrawPixel(Point(8, 1), COL_GREEN);
    CPPUNIT_ASSERT_EQUAL(COL_GREEN, aRenderContext.GetPixel(Point(8, 1)));

    aRenderContext.DrawPixel(Point(1, 8), COL_BLUE);
    CPPUNIT_ASSERT_EQUAL(COL_BLUE, aRenderContext.GetPixel(Point(1, 8)));
}

CPPUNIT_TEST_SUITE_REGISTRATION(PixelTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
