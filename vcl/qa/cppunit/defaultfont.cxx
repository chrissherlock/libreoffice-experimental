/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <unotools/fontdefs.hxx>

#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/outdev.hxx>
#include <vcl/font.hxx>
#include <vcl/svapp.hxx>

#include <FontController.hxx>

using namespace vcl;

class FontControllerTest : public CppUnit::TestFixture
{
public:
    void testGetDefaultFontSanity()
    {
        // Test 1: Basic Sans Serif request
        vcl::Font aFont = vcl::font::FontController::GetDefaultFont(
            DefaultFontType::SANS_UNICODE, LANGUAGE_ENGLISH_US, GetDefaultFontFlags::NONE,
            nullptr // No specific output device, use default
        );

        // We expect at least a family to be set
        CPPUNIT_ASSERT_EQUAL(FAMILY_SWISS, aFont.GetFamilyType());

        // In a typical test env, we might get "Liberation Sans" or similar,
        // but let's just assert we got *something* back.
        CPPUNIT_ASSERT(!aFont.GetFamilyName().isEmpty());
    }

    void testGetDefaultFontFixed()
    {
        // Test 2: Fixed width request
        vcl::Font aFont = vcl::font::FontController::GetDefaultFont(
            DefaultFontType::FIXED, LANGUAGE_ENGLISH_US, GetDefaultFontFlags::NONE, nullptr);

        CPPUNIT_ASSERT_EQUAL(FAMILY_MODERN, aFont.GetFamilyType());
        CPPUNIT_ASSERT_EQUAL(PITCH_FIXED, aFont.GetPitch());
    }

    // Define the test suite
    CPPUNIT_TEST_SUITE(FontControllerTest);
    CPPUNIT_TEST(testGetDefaultFontSanity);
    CPPUNIT_TEST(testGetDefaultFontFixed);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FontControllerTest);
