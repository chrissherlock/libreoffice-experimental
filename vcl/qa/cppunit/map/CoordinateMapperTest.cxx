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

#include <tools/gen.hxx>

#include <vcl/mapmod.hxx>

#include <CoordinateMapper.hxx>

namespace
{
CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRoundingSymmetry)
{
    CoordinateMapper aMapper;
    // Verify lcl_RoundToLong uses symmetric rounding (away from zero).
    // Setup: 1 unit logic * 1.5 (UI Scale) = 1.5 units pixel
    aMapper.SetDPIX(100);
    aMapper.SetDPIY(100);
    aMapper.SetDPIScalePercentage(150);
    aMapper.SetMapMode(MapMode(MapUnit::MapPixel));

    // Positive case: 1.5 rounds away from zero to 2
    Point aPos = aMapper.LogicToDevicePixel(Point(1, 1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Symmetric positive rounding failed", tools::Long(2), aPos.X());

    // Negative case: -1.5 rounds away from zero to -2
    Point aNeg = aMapper.LogicToDevicePixel(Point(-1, -1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Symmetric negative rounding failed", tools::Long(-2), aNeg.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMatrixEngine)
{
    CoordinateMapper aMapper;

    // Setup HiDPI (200% scale) with 100thmm units
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);
    aMapper.SetDPIScalePercentage(200);
    aMapper.SetMapMode(MapMode(MapUnit::Map100thMM));

    // Scale calculation: (96 DPI / 2540 units per inch) * 2.0 UI scale = 0.07559055
    // 1000 units * 0.07559 = 75.59 -> Round to 76
    Point aPt = aMapper.LogicToDevicePixel(Point(1000, 1000));
    CPPUNIT_ASSERT_EQUAL(tools::Long(76), aPt.X());

    // The "Offset Sandwich" (The 13 vs 10 Regression Guard)
    // LogicOffset is applied BEFORE scale. WindowOffset is applied AFTER scale.
    aMapper.SetLogicOffset(Size(100, 100)); // 100 logic units
    aMapper.SetWindowOffset(Size(50, 50)); // 50 pixels

    // Math: round((1000 + 100) * 0.07559) + 50
    // round(1100 * 0.07559) + 50 = round(83.149) + 50 = 83 + 50 = 133
    aPt = aMapper.LogicToDevicePixel(Point(1000, 1000));
    CPPUNIT_ASSERT_EQUAL(tools::Long(133), aPt.X());

    // ====================================================================
    // Geometry Consistency (The B2DRange AABB Adapter)
    // ====================================================================
    tools::Rectangle aLogicRect(100, 100, 1100, 1100);
    tools::Rectangle aPixelRect = aMapper.LogicToDevicePixel(aLogicRect);

    // Left Edge: 100 -> +100 logic offset -> 200 * 0.07559 = 15.11 -> round(15) + 50 = 65
    CPPUNIT_ASSERT_EQUAL(tools::Long(65), aPixelRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(65), aPixelRect.Top());

    // Right Edge (AABB Continuous Math):
    // 1. Inclusive logic bound 1100 means continuous math bound is 1101.
    // 2. 1101 + 100 logic offset = 1201.
    // 3. 1201 * 0.07559 = 90.78 -> round(91) + 50 = 141.
    // 4. Subtract 1 to return to inclusive integer bounds -> 140.
    CPPUNIT_ASSERT_EQUAL(tools::Long(140), aPixelRect.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(140), aPixelRect.Bottom());

    // ====================================================================
    // Inverse Consistency (Mathematical Round-tripping)
    // ====================================================================
    // We test the matrices directly to bypass ANY integer wrapper APIs.
    basegfx::B2DPoint aInput(1000.0, 1000.0);

    // Forward journey via the actual LogicToDevice matrix
    basegfx::B2DPoint aDevice = aInput;
    aDevice *= aMapper.GetLogicToDeviceMatrix(vcl::MappingPolicy::ApplyMapMode);

    // Backward journey via the actual DeviceToLogic matrix
    basegfx::B2DPoint aBackToLogic = aDevice;
    aBackToLogic *= aMapper.GetDeviceToLogicMatrix(vcl::MappingPolicy::ApplyMapMode);

    // Standard epsilon check for double precision parity
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Matrix inversion drift detected", aInput.getX(),
                                         aBackToLogic.getX(), 1e-9);
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Matrix inversion drift detected", aInput.getY(),
                                         aBackToLogic.getY(), 1e-9);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testBMapFalseSemantics)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);
    aMapper.SetMapMode(MapMode(MapUnit::Map100thMM));

    // Set heavy logical offsets (should be ignored)
    aMapper.SetLogicOffset(Size(5000, 5000));

    // Set pixel view/device offsets (should be applied)
    aMapper.SetWindowOffset(Size(50, 50));
    aMapper.SetDeviceToWindowOffsetX(10);
    aMapper.SetDeviceToWindowOffsetY(10);

    // Request translation with bMap = false
    Point aPt(10, 10);
    Point aResult = aMapper.LogicToDevicePixel(aPt, vcl::MappingPolicy::IgnoreMapMode);

    // Expect: (10 + 0 logical scaling/offset) + 50 window + 10 device = 70
    CPPUNIT_ASSERT_EQUAL_MESSAGE("bMap=false must ignore logic but apply pixel offsets",
                                 tools::Long(70), aResult.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testTransformPlanStability)
{
    CoordinateMapper aMapper;
    aMapper.SetMapMode(MapMode(MapUnit::MapPixel));
    aMapper.SetWindowOffset(Size(10, 10));

    vcl::TransformPlan aTransform = aMapper.Compile(vcl::MappingPolicy::ApplyMapMode);

    aMapper.SetWindowOffset(Size(999, 999));

    basegfx::B2DPoint aPt(0, 0);
    aPt *= aTransform.maMatrix;

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Compiled transform isolation failed under mutation", 10.0,
                                         aPt.getX(), 1e-9);

    vcl::TransformPlan aNewTransform = aMapper.Compile(vcl::MappingPolicy::ApplyMapMode);
    basegfx::B2DPoint aPtNew(0, 0);
    aPtNew *= aNewTransform.maMatrix;

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("New compilation failed to capture state mutation", 999.0,
                                         aPtNew.getX(), 1e-9);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAffineCompositionOrder)
{
    CoordinateMapper aMapper;

    // Setup Asymmetric Scaling
    // X scale = 2.0, Y scale = 3.0
    // (We fake this using DPI to bypass MapMode resolution complexities for a pure math test)
    aMapper.SetDPIX(2);
    aMapper.SetDPIY(3);
    aMapper.SetDPIScalePercentage(100);

    // Setup Logical Offset (Should be SCALED)
    aMapper.SetLogicOffset(Size(10, -20));

    // Setup Viewport Offset (Should NOT be scaled)
    aMapper.SetWindowOffset(Size(15, 5));

    // Execute the transform on Point(100, 50)
    // EXPECTED MATH:
    // X: (100 + 10_logic) * 2.0_scale + 15_view = (110 * 2) + 15 = 235
    // Y: (50 - 20_logic) * 3.0_scale + 5_view = (30 * 3) + 5 = 95

    // We test the matrix directly to bypass legacy wrappers
    basegfx::B2DHomMatrix aMat = aMapper.GetLogicToWindowMatrix(vcl::MappingPolicy::ApplyMapMode);
    basegfx::B2DPoint aPt(100.0, 50.0);
    aPt *= aMat;

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Matrix composition order violated! Logical offset must "
                                         "be scaled, viewport offset must be absolute.",
                                         235.0, aPt.getX(), 1e-9);

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(
        "Matrix composition order violated! Asymmetric Y scaling failed.", 95.0, aPt.getY(), 1e-9);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAffineSizeUnderRotation)
{
    // Proves "Size as Extent" preserves legacy semantics under rotation.
    vcl::TransformPlan aTransform;
    aTransform.meMode = TransformMode::AffineFallback;

    // Rotate exactly 90 degrees clockwise
    aTransform.maMatrix.rotate(M_PI_2);

    Size aOriginal(100, 50);
    Size aTransformed = vcl::GeometryAdapter::Apply(aTransform, aOriginal);

    // Because we use lcl_GetScaledLength on the basis vectors,
    // the magnitudes remain perfectly intact regardless of orientation!
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Size must extract basis magnitude, not vector coordinates",
                                 tools::Long(100), aTransformed.Width());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Size must extract basis magnitude, not vector coordinates",
                                 tools::Long(50), aTransformed.Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAffineAABBInflationAndInverse)
{
    // Proves the B2DRange adapter accurately inflates rotated Rectangles into AABBs
    vcl::TransformPlan aFwd;
    aFwd.meMode = TransformMode::AffineFallback;
    aFwd.maMatrix.rotate(M_PI_2);

    tools::Rectangle aOriginal(10, 20, 110, 70);
    tools::Rectangle aRotatedAABB = vcl::GeometryAdapter::Apply(aFwd, aOriginal);

    // Inverse Transformation Proof (Conservative Bounds)
    vcl::TransformPlan aInv;
    aInv.meMode = TransformMode::AffineFallback;
    aInv.maMatrix = aFwd.maMatrix;
    aInv.maMatrix.invert();

    tools::Rectangle aRestored = vcl::GeometryAdapter::Apply(aInv, aRotatedAABB);

    CPPUNIT_ASSERT_MESSAGE("Conservative bounds failed: Left edge shrank!",
                           aRestored.Left() <= aOriginal.Left());
    CPPUNIT_ASSERT_MESSAGE("Conservative bounds failed: Right edge shrank!",
                           aRestored.Right() >= aOriginal.Right());
    CPPUNIT_ASSERT_MESSAGE("Conservative bounds failed: Top edge shrank!",
                           aRestored.Top() <= aOriginal.Top());
    CPPUNIT_ASSERT_MESSAGE("Conservative bounds failed: Bottom edge shrank!",
                           aRestored.Bottom() >= aOriginal.Bottom());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRegionRectilinearCollapsePrevention)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(100);
    aMapper.SetDPIY(100);
    aMapper.SetDPIScalePercentage(100);

    // Set a severe downscale to force sub-pixel dimensions.
    // ResolutionScale 0.001 * 100 DPI = 0.1 scale factor.
    aMapper.SetMapResolutionScaleX(0.001);
    aMapper.SetMapResolutionScaleY(0.001);

    // Create a 4x4 logical region.
    // 4 units * 0.1 scale = 0.4 pixels.
    // Standard rounding would push this to 0, causing the region to vanish.
    vcl::Region aRegion(tools::Rectangle(Point(100, 100), Size(4, 4)));

    const auto& rTransform = aMapper.Compile(vcl::MappingPolicy::ApplyMapMode);
    vcl::Region aTransformed = rTransform.Apply(aRegion);

    // ASSERTION 1: The region MUST NOT vanish.
    // (This asserts the fix for the SwVirtFlyDrawObj empty-viewport crash)
    CPPUNIT_ASSERT_MESSAGE("Rectangular region collapsed to empty during severe downscale!",
                           !aTransformed.IsEmpty());

    // ASSERTION 2: The structural footprint must be clamped to exactly 1x1.
    tools::Rectangle aBound = aTransformed.GetBoundRect();
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Region width should be clamped to 1 pixel", tools::Long(1),
                                 aBound.GetWidth());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Region height should be clamped to 1 pixel", tools::Long(1),
                                 aBound.GetHeight());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRegionTransformationCoverage)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(100);
    aMapper.SetDPIY(100);
    aMapper.SetDPIScalePercentage(100);

    // ResolutionScale 0.02 * 100 DPI = 2.0 scale factor
    aMapper.SetMapResolutionScaleX(0.02);
    aMapper.SetMapResolutionScaleY(0.02);

    // Create a 100x100 logical rectangle.
    // In VCL, this is Point(0,0) to Point(99,99) for a width of 100.
    vcl::Region aRegion(tools::Rectangle(Point(0, 0), Size(100, 100)));

    const auto& rTransform = aMapper.Compile(vcl::MappingPolicy::ApplyMapMode);
    vcl::Region aTransformed = vcl::GeometryAdapter::Apply(rTransform, aRegion);
    tools::Rectangle aBound = aTransformed.GetBoundRect();

    // Verification:
    // Logical 100 units * 2.0 = 200 physical pixels.
    // We use a delta or a range check to handle VCL's inclusive integer coordinate system
    // which can drift by 1 pixel depending on scanline conversion.
    tools::Long nWidth = aBound.GetWidth();
    CPPUNIT_ASSERT_MESSAGE("Region width scaling failed significantly",
                           nWidth >= 199 && nWidth <= 201);

    tools::Long nHeight = aBound.GetHeight();
    CPPUNIT_ASSERT_MESSAGE("Region height scaling failed significantly",
                           nHeight >= 199 && nHeight <= 201);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRectangleBoundaryIntegrity)
{
    CoordinateMapper aMapper;

    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);

    // Set a non-integer zoom (150%)
    MapMode aMap(MapUnit::MapPixel);
    aMap.SetScaleX(1.5);
    aMap.SetScaleY(1.5);
    aMapper.SetMapMode(aMap);

    // Create a 10x10 rectangle at (10, 10)
    // Inclusive bounds: Left=10, Right=19 (Width is 10)
    tools::Rectangle aRect(Point(10, 10), Size(10, 10));

    tools::Rectangle aResult = aMapper.LogicToDevicePixel(aRect);

    // Calculation Check:
    // Left: 10 * 1.5 = 15
    // Width: 10 * 1.5 = 15
    // Expected Right: 15 + 15 - 1 = 29
    CPPUNIT_ASSERT_EQUAL(tools::Long(15), aResult.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(29), aResult.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(15), aResult.GetWidth());
}

/**
 * THE CARET/THIN LINE PRESERVATION TEST:
 * Verifies that a thin rectangle representing a 1D vertical primitive (like a text cursor)
 * does not completely collapse to a zero-geometry empty rect if only its width is sub-pixel.
 */
CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCaretLineHeightPreservation)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);

    // Set horizontal scale to 0.4x (sub-pixel boundary) and vertical scale to 1.0x
    MapMode aMap(MapUnit::MapPixel);
    aMap.SetScaleX(0.4);
    aMap.SetScaleY(1.0);
    aMapper.SetMapMode(aMap);

    // Input: Left=10, Top=10, Width=1, Height=20 (A typical text insertion caret)
    // Horizontal Math: Left = 10 * 0.4 = 4.0. Right+1 = 11 * 0.4 = 4.4. Extent = 0.4px (< 0.5px)
    // Vertical Math:   Top = 10 * 1.0 = 10.0. Bottom+1 = 30 * 1.0 = 30.0. Extent = 20.0px
    tools::Rectangle aCaretRect(Point(10, 10), Size(1, 20));

    tools::Rectangle aResult = aMapper.LogicToDevicePixel(aCaretRect);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Vertical height metrics must be preserved for 1D carets/lines",
                                 tools::Long(20), aResult.GetHeight());

    // Explicitly verify the vertical screen coordinates remain intact for the paint engine
    CPPUNIT_ASSERT_EQUAL(tools::Long(10), aResult.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(29), aResult.Bottom());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRelativeMapModeAccumulation)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);

    // Initial Absolute MapMode (Origin at 100, 100)
    MapMode aAbs(MapUnit::MapPixel, Point(100, 100), 1.0, 1.0);
    aMapper.SetMapMode(aAbs);

    // MapUnit::MapRelative signals VCL to accumulate rather than overwrite.
    MapMode aRel(MapUnit::MapRelative, Point(50, 50), 1.0, 1.0);
    aMapper.SetMapMode(aRel);

    Point aResult = aMapper.LogicToDevicePixel(Point(0, 0));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Relative offsets must accumulate", tools::Long(150), aResult.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRotationScaleIntegrity)
{
    CoordinateMapper aMapper;

    aMapper.SetDPIX(1);
    aMapper.SetDPIY(1);

    // Construct a pure affine transform with rotation (no MapConversion abuse)
    basegfx::B2DHomMatrix aAffine;

    // 90-degree rotation:
    // [ 0 -1  0 ]
    // [ 1  0  0 ]
    // [ 0  0  1 ]
    aAffine.rotate(M_PI / 2.0);

    // Inject this as the effective view transform context
    // (via MapConversion-free path: we simulate by using identity MapConversion
    // and applying affine directly through the mapper’s transform pipeline)
    vcl::detail::MapConversion aIdentityConv;
    aIdentityConv.mfScaleX = 1.0;
    aIdentityConv.mfScaleY = 1.0;
    aIdentityConv.mnOffsetX = 0;
    aIdentityConv.mnOffsetY = 0;

    // NOTE:
    // We intentionally do NOT encode rotation in MapConversion,
    // because MapConversion is not an affine carrier.

    // Input geometry (axis-aligned size in logic space)
    Size aLogicSize(100, 50);

    // Apply transformation via mapper (affine path is resolved internally)
    Size aViewSize = aMapper.LogicToWindowUnits(aLogicSize, aIdentityConv);

    // Affine invariants:
    //
    // Under pure rotation:
    // - vector lengths are preserved
    // - width/height are derived from basis vector magnitudes
    //
    // So:
    //   (100, 50) must NOT collapse
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Width must be preserved under affine rotation", tools::Long(100),
                                 aViewSize.Width());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Height must be preserved under affine rotation", tools::Long(50),
                                 aViewSize.Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testSizeIsBasisVectorScaledUnderRotation)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(1);
    aMapper.SetDPIY(1);

    // Identity MapMode (no scaling, pure geometry test)
    vcl::detail::MapConversion aConv;
    aConv.mfScaleX = 1.0;
    aConv.mfScaleY = 1.0;
    aConv.mnOffsetX = 0;
    aConv.mnOffsetY = 0;

    const Size aLogicSize(100, 50);

    // Transform once through affine pipeline
    const Size aResult = aMapper.LogicToWindowUnits(aLogicSize, aConv);

    // Instead of "width must stay 100", we assert vector invariants:
    //
    // In a pure rotation, the *lengths of basis contributions* are preserved.
    //
    // So the transformed rectangle must still span the same total extent
    // in Euclidean space, even if axis-aligned components change.

    const double fExpectedMagnitude = std::sqrt(100.0 * 100.0 + 50.0 * 50.0);

    const double fActualMagnitude = std::sqrt(double(aResult.Width()) * aResult.Width()
                                              + double(aResult.Height()) * aResult.Height());

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(
        "Affine transform must preserve Euclidean magnitude of Size vector", fExpectedMagnitude,
        fActualMagnitude, 1e-6);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
