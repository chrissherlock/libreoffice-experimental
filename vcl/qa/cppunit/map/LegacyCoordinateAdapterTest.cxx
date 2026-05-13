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

#include <vcl/mapmod.hxx>

#include <LegacyCoordinateAdapter.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testYAxisInversionRectangle)
{
    // Source MapMode: 100th MM with an INVERTED Y-axis (ScaleY = -1.0).
    // This simulates a Cartesian coordinate system where Y grows UP.
    MapMode aSourceMap(MapUnit::Map100thMM, Point(0, 0), 1.0, -1.0);

    // Dest MapMode: Standard Pixels.
    // This simulates screen coordinates where Y grows DOWN.
    MapMode aDestMap(MapUnit::MapPixel);

    // Create a valid rectangle in the source coordinate system.
    // Note: VCL tools::Rectangle requires Top <= Bottom to be valid.
    tools::Rectangle aSourceRect(100, 500, 200, 1000);

    CPPUNIT_ASSERT_MESSAGE("Source rectangle must initially be valid", !aSourceRect.IsEmpty());

    // Map the rectangle. Because the Y-axis direction flips, the mapped 'Top'
    // will mathematically become a larger number than the mapped 'Bottom'.
    tools::Rectangle aMappedRect = LogicToLogic(aSourceRect, aSourceMap, aDestMap);

    // If LegacyCoordinateAdapter forgets to call .Normalize(), the rectangle
    // will collapse into an Empty state and trigger assertions downstream.
    CPPUNIT_ASSERT_MESSAGE("Rectangle collapsed to Empty due to Y-axis crossover!",
                           !aMappedRect.IsEmpty());

    // Explicitly verify the coordinates sorted themselves correctly
    CPPUNIT_ASSERT_MESSAGE("Mapped Rectangle Top must be <= Bottom",
                           aMappedRect.Top() <= aMappedRect.Bottom());
    CPPUNIT_ASSERT_MESSAGE("Mapped Rectangle Left must be <= Right",
                           aMappedRect.Left() <= aMappedRect.Right());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
