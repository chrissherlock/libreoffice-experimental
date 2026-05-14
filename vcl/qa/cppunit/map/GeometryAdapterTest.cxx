
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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
#include <cppunit/plugin/TestPlugIn.h>

#include <tools/gen.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/GeometryAdapter.hxx>
#include <vcl/TransformTypes.hxx>

#include <TransformCompiler.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testHairlineClampSize)
{
    // Create a matrix that drastically scales down.
    // e.g., 15 twips * 0.02 = 0.3 pixels (which would round to 0 without clamping)
    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02);

    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    // Standard text caret thickness (15 twips)
    Size aHairline(15, 240);
    Size aResult = vcl::GeometryAdapter::Apply(aPlan, aHairline);

    // Assert that the dimension did not collapse to 0
    CPPUNIT_ASSERT_MESSAGE("Hairline width collapsed to 0!", aResult.Width() != 0);

    // It should be strictly clamped to 1 pixel
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.Width());

    // The height (240 * 0.02 = 4.8) should round normally to 5
    CPPUNIT_ASSERT_EQUAL(tools::Long(5), aResult.Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testHairlineClampRectangle)
{
    // Extreme scale-down matrix
    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02);

    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    // Simulate a caret positioned at (100, 100) with a 15-unit width
    // Rectangle constructor is (Left, Top, Right, Bottom)
    // Note: VCL rectangles are inclusive, so width 15 is Right = Left + 14
    tools::Rectangle aCaretRect(100, 100, 114, 339);

    tools::Rectangle aResult = vcl::GeometryAdapter::Apply(aPlan, aCaretRect);

    // Without clamping, a 0.3px width flags as a "ghost line" and becomes empty
    CPPUNIT_ASSERT_MESSAGE("Rectangle collapsed to an Empty state!", !aResult.IsEmpty());

    // Ensure the resulting physical footprint is exactly 1 pixel wide
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.GetWidth());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testSubPixelBoundaryCrossover)
{
    // We need to trigger the exact floating-point rounding trap where:
    // fLeft rounds UP, but fRight rounds DOWN, causing Right < Left (an Empty rect).
    // Let's force the math to yield: fLeft = 5.5, fRight = 6.1

    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.6, 0.6); // Scale first
    aMat.translate(5.5, 5.5); // Translate second

    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    // A 1x1 logical rectangle at the origin.
    // In VCL, bounds are inclusive, so [Left=0, Right=0] means Width = 1.
    tools::Rectangle aLogicalRect(0, 0, 0, 0);

    // MATHEMATICAL PROOF OF THE CROSSOVER:
    // fLeft  = (0 * 0.6) + 5.5 = 5.5  -> std::round(5.5) = 6
    // fRight = (1 * 0.6) + 5.5 = 6.1  -> std::round(6.1) - 1 = 5
    // Without our clamp, Right (5) < Left (6), so VCL flags it as IsEmpty() = true!

    tools::Rectangle aResult = vcl::GeometryAdapter::Apply(aPlan, aLogicalRect);

    // Ensure the clamp caught the crossover and prevented the viewport from vanishing
    CPPUNIT_ASSERT_MESSAGE("Sub-pixel crossover caused the geometry to collapse to Empty!",
                           !aResult.IsEmpty());

    // Explicitly verify the bounds did not cross
    CPPUNIT_ASSERT_MESSAGE("Right bound crossed over Left bound!",
                           aResult.Right() >= aResult.Left());
    CPPUNIT_ASSERT_MESSAGE("Bottom bound crossed over Top bound!",
                           aResult.Bottom() >= aResult.Top());

    // It should have safely clamped to a 1x1 physical pixel footprint at index 6
    CPPUNIT_ASSERT_EQUAL(tools::Long(6), aResult.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(6), aResult.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(6), aResult.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(6), aResult.Bottom());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
