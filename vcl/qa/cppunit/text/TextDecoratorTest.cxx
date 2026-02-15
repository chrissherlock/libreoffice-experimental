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

CPPUNIT_TEST_FIXTURE(TextDecoratorTest, testEmphasisMarkPositions)
{
    // NOTE: We test GetEmphasisMarkPositions indirectly via VirtualDevice.
    // Applying an emphasis mark causes VCL to dynamically scale down the base font
    // to fit the mark within the standard line height.
    // We verify the decorator succeeds by ensuring the visual footprint changes.

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

    // The emphasis mark radically alters the visual footprint (adds the dot, scales the text)
    CPPUNIT_ASSERT_MESSAGE("Emphasis mark must alter the visual bounding box",
                           aBoundsNoMark != aBoundsWithMark);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
