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

#include <vcl/virdev.hxx>
#include <vcl/BitmapReadAccess.hxx>
#include <vcl/font.hxx>
#include <tools/color.hxx>

namespace
{
class TextDecoratorTest : public CppUnit::TestFixture
{
};

tools::Rectangle getInkBounds(const Bitmap& rBmp)
{
    BitmapReadAccess aAccess(const_cast<Bitmap&>(rBmp));
    tools::Long minX = rBmp.GetSizePixel().Width(), maxX = 0;
    tools::Long minY = rBmp.GetSizePixel().Height(), maxY = 0;

    for (tools::Long y = 0; y < rBmp.GetSizePixel().Height(); ++y)
    {
        for (tools::Long x = 0; x < rBmp.GetSizePixel().Width(); ++x)
        {
            if (aAccess.GetPixel(y, x) != COL_WHITE)
            {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (minX > maxX)
        return tools::Rectangle(); // Empty

    return tools::Rectangle(minX, minY, maxX, maxY);
}

tools::Long countInkPixels(const Bitmap& rBmp)
{
    BitmapReadAccess aAccess(const_cast<Bitmap&>(rBmp));
    tools::Long nPixels = 0;

    for (tools::Long y = 0; y < rBmp.GetSizePixel().Height(); ++y)
    {
        for (tools::Long x = 0; x < rBmp.GetSizePixel().Width(); ++x)
        {
            if (aAccess.GetPixel(y, x) != COL_WHITE)
            {
                nPixels++;
            }
        }
    }
    return nPixels;
}

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testEmphasisMarkPositions)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(100, 100));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);
    pDev->EnableOutput(true);

    vcl::Font aFont(OUString("Noto Sans CJK SC"), Size(0, 24));
    Point aPos(20, 80);
    OUString aText(u"\u4E2D"_ustr); // "Zhong"

    // 1. Draw WITHOUT emphasis mark
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpNoMark = pDev->GetBitmap(Point(0, 0), Size(100, 100));
    tools::Rectangle aBoundsNoMark = getInkBounds(aBmpNoMark);

    // 2. Draw WITH emphasis mark (Dot)
    aFont.SetEmphasisMark(FontEmphasisMark::Dot);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpWithMark = pDev->GetBitmap(Point(0, 0), Size(100, 100));
    tools::Rectangle aBoundsWithMark = getInkBounds(aBmpWithMark);

    CPPUNIT_ASSERT_MESSAGE("Text should render ink", !aBoundsNoMark.IsEmpty());
    CPPUNIT_ASSERT_MESSAGE("Text with mark should render ink", !aBoundsWithMark.IsEmpty());

    CPPUNIT_ASSERT_MESSAGE("Emphasis mark must alter the visual bounding box",
                           aBoundsNoMark != aBoundsWithMark);
}

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testUnderline)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(200, 100));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);

    // Set the line color on the device!
    pDev->SetTextLineColor(COL_BLACK);
    pDev->EnableOutput(true);

    vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 40));
    Point aPos(20, 50);
    OUString aText(u"LibreOffice"_ustr);

    // Base Text
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpBase = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsBase = getInkBounds(aBmpBase);
    tools::Long nPixelsBase = countInkPixels(aBmpBase);

    // Underlined Text
    aFont.SetUnderline(LINESTYLE_SINGLE);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpUnderline = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsUnderline = getInkBounds(aBmpUnderline);
    tools::Long nPixelsUnderline = countInkPixels(aBmpUnderline);

    CPPUNIT_ASSERT_MESSAGE("Underline must add ink pixels to the image",
                           nPixelsUnderline > nPixelsBase);
    CPPUNIT_ASSERT_MESSAGE("Underline must extend the bottom visual bounds",
                           aBoundsUnderline.Bottom() > aBoundsBase.Bottom());
}

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testWaveUnderline)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(200, 100));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);
    pDev->SetTextLineColor(COL_BLACK);
    pDev->EnableOutput(true);

    vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 40));
    Point aPos(20, 50);
    OUString aText(u"Geometry"_ustr);

    // Single Underline
    aFont.SetUnderline(LINESTYLE_SINGLE);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    tools::Long nPixelsSingle = countInkPixels(pDev->GetBitmap(Point(0, 0), Size(200, 100)));

    // Wave Underline
    aFont.SetUnderline(LINESTYLE_WAVE);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    tools::Long nPixelsWave = countInkPixels(pDev->GetBitmap(Point(0, 0), Size(200, 100)));

    CPPUNIT_ASSERT_MESSAGE("Wave underline should have a different pixel density than single",
                           nPixelsWave != nPixelsSingle);
    CPPUNIT_ASSERT_MESSAGE("Wave underline must render pixels", nPixelsWave > 0);
}

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testStrikeout)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(200, 100));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);
    pDev->SetTextLineColor(COL_BLACK);
    pDev->EnableOutput(true);

    vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 40));
    Point aPos(20, 50);
    OUString aText(u"Strike"_ustr);

    // Base Text
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpBase = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsBase = getInkBounds(aBmpBase);
    tools::Long nPixelsBase = countInkPixels(aBmpBase);

    // Strikeout Text
    aFont.SetStrikeout(STRIKEOUT_SINGLE);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpStrike = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsStrike = getInkBounds(aBmpStrike);
    tools::Long nPixelsStrike = countInkPixels(aBmpStrike);

    CPPUNIT_ASSERT_MESSAGE("Strikeout must add ink pixels to the image",
                           nPixelsStrike > nPixelsBase);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Strikeout should not drastically alter top bound",
                                 aBoundsBase.Top(), aBoundsStrike.Top());
}

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testOverline)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(200, 100));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);

    // Set the Overline color on the device!
    pDev->SetOverlineColor(COL_BLACK);
    pDev->EnableOutput(true);

    vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 40));
    Point aPos(20, 50);

    // Use an underscore. It renders entirely on the baseline.
    // This guarantees the base text's Top() is mathematically far away from the ascender line.
    OUString aText(u"_"_ustr);

    // Base Text
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpBase = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsBase = getInkBounds(aBmpBase);
    tools::Long nPixelsBase = countInkPixels(aBmpBase);

    // Overlined Text
    aFont.SetOverline(LINESTYLE_SINGLE);
    pDev->SetFont(aFont);
    pDev->Erase();
    pDev->DrawText(aPos, aText);
    Bitmap aBmpOverline = pDev->GetBitmap(Point(0, 0), Size(200, 100));
    tools::Rectangle aBoundsOverline = getInkBounds(aBmpOverline);
    tools::Long nPixelsOverline = countInkPixels(aBmpOverline);

    CPPUNIT_ASSERT_MESSAGE("Overline must add ink pixels to the image",
                           nPixelsOverline > nPixelsBase);

    // Create a dynamic error message so we can see the exact pixel coordinates if it ever fails
    OString aMsg = "Overline top (" + OString::number(aBoundsOverline.Top())
                   + ") must be higher (less than) base top (" + OString::number(aBoundsBase.Top())
                   + ")";

    CPPUNIT_ASSERT_MESSAGE(aMsg.getStr(), aBoundsOverline.Top() < aBoundsBase.Top());
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
