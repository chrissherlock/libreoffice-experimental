/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <test/outputdevice.hxx>

#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/BitmapReadAccess.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>

#include <list>

namespace vcl::test {

namespace
{

void drawLineOffset(OutputDevice& rDevice, tools::Rectangle const & rRect, int nOffset)
{

    int nMidOffset = rRect.GetWidth()/2;
    Point aLeftTop     (rRect.Left()  + nOffset - (nOffset+1)/2, rRect.Top()    + nOffset - 1);
    Point aRightTop    (rRect.Right() - nMidOffset - nOffset/3, rRect.Top()    + nOffset - 1);
    Point aLeftBottom  (rRect.Left()  + nOffset - (nOffset+1)/2, rRect.Bottom() - nOffset + 1);
    Point aRightBottom (rRect.Right() - nMidOffset - nOffset/3, rRect.Bottom() - nOffset + 1);

    rDevice.DrawLine(aLeftTop,     aRightTop);
    rDevice.DrawLine(aRightTop,    aRightBottom);
    rDevice.DrawLine(aRightBottom, aLeftBottom);
    rDevice.DrawLine(aLeftBottom,  aLeftTop);
}

} // end anonymous namespace

Bitmap OutputDeviceTestLine::setupRectangle(bool bEnableAA)
{
    initialSetup(13, 13, constBackgroundColor, bEnableAA);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    drawLineOffset(*mpVirtualDevice, maVDRectangle, 2);
    drawLineOffset(*mpVirtualDevice, maVDRectangle, 5);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupRectangleOnSize1028()
{
    initialSetup(1028, 1028, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    drawLineOffset(*mpVirtualDevice, maVDRectangle, 2);
    drawLineOffset(*mpVirtualDevice, maVDRectangle, 5);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupRectangleOnSize4096()
{
    initialSetup(4096, 4096, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    drawLineOffset(*mpVirtualDevice, maVDRectangle, 2);
    drawLineOffset(*mpVirtualDevice, maVDRectangle, 5);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupDiamond()
{
    initialSetup(11, 11, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    Point aPoint1, aPoint2, aPoint3, aPoint4;
    OutputDeviceTestCommon::createDiamondPoints(maVDRectangle, 4, aPoint1, aPoint2, aPoint3, aPoint4);

    mpVirtualDevice->DrawLine(aPoint1, aPoint2);
    mpVirtualDevice->DrawLine(aPoint2, aPoint3);
    mpVirtualDevice->DrawLine(aPoint3, aPoint4);
    mpVirtualDevice->DrawLine(aPoint4, aPoint1);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupLines()
{
    initialSetup(13, 13, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    Point aHorizontalLinePoint1, aHorizontalLinePoint2;
    Point aVerticalLinePoint1, aVerticalLinePoint2;
    Point aDiagonalLinePoint1, aDiagonalLinePoint2;

    OutputDeviceTestCommon::createHorizontalVerticalDiagonalLinePoints(
                          maVDRectangle, aHorizontalLinePoint1, aHorizontalLinePoint2,
                                         aVerticalLinePoint1,   aVerticalLinePoint2,
                                         aDiagonalLinePoint1,   aDiagonalLinePoint2);

    mpVirtualDevice->DrawLine(aHorizontalLinePoint1, aHorizontalLinePoint2);
    mpVirtualDevice->DrawLine(aVerticalLinePoint1,   aVerticalLinePoint2);
    mpVirtualDevice->DrawLine(aDiagonalLinePoint1,   aDiagonalLinePoint2);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupAALines()
{
    initialSetup(13, 13, constBackgroundColor);

    mpVirtualDevice->SetAntialiasing(AntialiasingFlags::Enable);
    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    Point aHorizontalLinePoint1, aHorizontalLinePoint2;
    Point aVerticalLinePoint1, aVerticalLinePoint2;
    Point aDiagonalLinePoint1, aDiagonalLinePoint2;

    OutputDeviceTestCommon::createHorizontalVerticalDiagonalLinePoints(
                          maVDRectangle, aHorizontalLinePoint1, aHorizontalLinePoint2,
                                         aVerticalLinePoint1,   aVerticalLinePoint2,
                                         aDiagonalLinePoint1,   aDiagonalLinePoint2);

    mpVirtualDevice->DrawLine(aHorizontalLinePoint1, aHorizontalLinePoint2);
    mpVirtualDevice->DrawLine(aVerticalLinePoint1,   aVerticalLinePoint2);
    mpVirtualDevice->DrawLine(aDiagonalLinePoint1,   aDiagonalLinePoint2);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupDashedLine()
{
    initialSetup(13, 13, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    tools::Rectangle rectangle = maVDRectangle;
    rectangle.shrink(2);

    std::vector<double> stroke({ 2.0, 1.0 });

    // Pack the geometry-generation attributes into our semantic struct
    vcl::rendercontext::StrokeAttributes aStroke;
    aStroke.fWidth = 1.0;
    aStroke.eJoin = basegfx::B2DLineJoin::NONE;
    aStroke.eCap = css::drawing::LineCap_BUTT;
    aStroke.fMiterMinimumAngle = basegfx::deg2rad(15.0);
    aStroke.pDashArray = &stroke;

    mpVirtualDevice->DrawPolyLine(
        basegfx::B2DPolygon{
            basegfx::B2DPoint(rectangle.Left(), rectangle.Top()),
            basegfx::B2DPoint(rectangle.Left(), rectangle.Bottom()),
            basegfx::B2DPoint(rectangle.Right(), rectangle.Bottom()),
            basegfx::B2DPoint(rectangle.Right(), rectangle.Top()),
            basegfx::B2DPoint(rectangle.Left(), rectangle.Top())},
        aStroke);

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

TestResult OutputDeviceTestLine::checkDashedLine(Bitmap& rBitmap)
{
    TestResult returnValue = TestResult::Passed;
    for (int i = 0; i < 7; i++)
    {
        TestResult eResult = TestResult::Passed;
        if( i == 2 )
        {
            // Build a sequence of pixels for the drawn rectangle border,
            // check that they alternate appropriately (there should be
            // normally 2 line, 1 background).
            std::list< bool > dash; // true - line color, false - background
            const int width = rBitmap.GetSizePixel().Width();
            const int height = rBitmap.GetSizePixel().Height();
            BitmapReadAccess access(rBitmap);
            for( int x = 2; x < width - 2; ++x )
                dash.push_back( access.GetPixel( 2, x ) == constLineColor );
            for( int y = 3; y < height - 3; ++y )
                dash.push_back( access.GetPixel( y, width - 3 ) == constLineColor );
            for( int x = width - 3; x >= 2; --x )
                dash.push_back( access.GetPixel( height - 3, x ) == constLineColor );
            for( int y = height - 4; y >= 3; --y )
                dash.push_back( access.GetPixel( y, 2 ) == constLineColor );
            for( int x = 2; x < width - 2; ++x ) // repeat, to check also the corner
                dash.push_back( access.GetPixel( 2, x ) == constLineColor );
            bool last = false;
            int lastCount = 0;
            while( !dash.empty())
            {
                if( dash.front() == last )
                {
                    ++lastCount;
                    if( lastCount > ( last ? 4 : 3 ))
                        eResult = TestResult::Failed;
                    else if( lastCount > ( last ? 3 : 2 ) && eResult != TestResult::Failed)
                        eResult = TestResult::PassedWithQuirks;
                }
                else
                {
                    last = dash.front();
                    lastCount = 1;
                }
                dash.pop_front();
            }
        }
        else
        {
            eResult = OutputDeviceTestCommon::checkRectangle(rBitmap, i, constBackgroundColor);
        }

        if (eResult == TestResult::Failed)
            returnValue = TestResult::Failed;
        if (eResult == TestResult::PassedWithQuirks && returnValue != TestResult::Failed)
            returnValue = TestResult::PassedWithQuirks;
    }
    return returnValue;
}

constexpr int CAPSHRINK = 25;
constexpr int CAPWIDTH = 20;
Bitmap OutputDeviceTestLine::setupLineCap( css::drawing::LineCap lineCap )
{
    initialSetup(101, 101, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    tools::Rectangle rectangle = maVDRectangle;
    rectangle.shrink(CAPSHRINK);

    const basegfx::B2DPolygon poly{
        basegfx::B2DPoint(rectangle.LeftCenter().getX(), rectangle.LeftCenter().getY()),
        basegfx::B2DPoint(rectangle.RightCenter().getX(), rectangle.RightCenter().getY())};

    {
        vcl::rendercontext::StrokeAttributes aStroke; aStroke.fWidth = CAPWIDTH; aStroke.eCap = lineCap; aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        mpVirtualDevice->DrawPolyLine(poly, aStroke);
    }

    mpVirtualDevice->SetLineColor(constFillColor);
    {
        vcl::rendercontext::StrokeAttributes aStroke; aStroke.fWidth = 0.0; aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        mpVirtualDevice->DrawPolyLine(poly, aStroke);
    }

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

Bitmap OutputDeviceTestLine::setupLineJoin( basegfx::B2DLineJoin lineJoin )
{
    initialSetup(101, 101, constBackgroundColor);

    mpVirtualDevice->SetLineColor(constLineColor);
    mpVirtualDevice->SetFillColor();

    tools::Rectangle rectangle = maVDRectangle;
    rectangle.shrink(CAPSHRINK);

    const basegfx::B2DPolygon poly{
        basegfx::B2DPoint(rectangle.TopLeft().getX(), rectangle.TopLeft().getY()),
        basegfx::B2DPoint(rectangle.TopRight().getX(), rectangle.TopRight().getY()),
        basegfx::B2DPoint(rectangle.BottomRight().getX(), rectangle.BottomRight().getY())};

    {
        vcl::rendercontext::StrokeAttributes aStroke; aStroke.fWidth = CAPWIDTH; aStroke.eJoin = lineJoin;
        mpVirtualDevice->DrawPolyLine(poly, aStroke);
    }

    mpVirtualDevice->SetLineColor(constFillColor);
    {
        vcl::rendercontext::StrokeAttributes aStroke; aStroke.fWidth = 0; aStroke.eJoin = lineJoin;
        mpVirtualDevice->DrawPolyLine(poly, aStroke);
    }

    return mpVirtualDevice->GetBitmap(maVDRectangle.TopLeft(), maVDRectangle.GetSize());
}

} // end namespace vcl::test

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
