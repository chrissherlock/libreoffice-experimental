/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>
#include <vcl/BitmapReadAccess.hxx>
#include <vcl/graph.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/font.hxx>
#include <tools/color.hxx>
#include <tools/urlobj.hxx>
#include <osl/file.hxx>
#include <iostream>

// UNO Bootstrap headers
#include <cppuhelper/bootstrap.hxx>
#include <comphelper/processfactory.hxx>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>

using namespace ::com::sun::star;

/**
 * Renders a Bitmap to the terminal using ASCII characters based on luminance.
 */
void PrintBitmap(const Bitmap& rBmp, int nMaxWidth = 120)
{
    Bitmap aScaledBmp = rBmp;
    Size aSize = aScaledBmp.GetSizePixel();

    // Calculate horizontal scaling
    double fScale = 1.0;
    if (aSize.Width() > nMaxWidth)
        fScale = static_cast<double>(nMaxWidth) / aSize.Width();

    // Adjust height by 0.45 to compensate for terminal character aspect ratio
    tools::Long nNewWidth = std::max<tools::Long>(1, aSize.Width() * fScale);
    tools::Long nNewHeight = std::max<tools::Long>(1, aSize.Height() * fScale * 0.45);

    // Use Fast scaling (nearest neighbor) to keep text edges sharp for ASCII
    aScaledBmp.Scale(Size(nNewWidth, nNewHeight), BmpScaleFlag::Fast);

    BitmapReadAccess aAccess(aScaledBmp);
    if (!aAccess)
        return;

    // Palette ordered from darkest (@) to lightest (space)
    const char* pPalette = "@@%%##**++==--::..  ";
    int nPalLen = 20;

    std::cout << "\n";
    for (tools::Long y = 0; y < aScaledBmp.GetSizePixel().Height(); ++y)
    {
        for (tools::Long x = 0; x < aScaledBmp.GetSizePixel().Width(); ++x)
        {
            sal_uInt16 nLum = aAccess.GetColor(y, x).GetLuminance();
            // Map 0-255 luminance to 0-19 palette index
            int nIndex = (nLum * (nPalLen - 1)) / 255;
            std::cout << pPalette[nIndex];
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

int main(int argc, char** argv)
{
    Application::EnableConsoleOnly();

    uno::Reference<uno::XComponentContext> xContext;
    try
    {
        xContext = cppu::defaultBootstrap_InitialComponentContext();

        uno::Reference<lang::XMultiServiceFactory> xFactory(xContext->getServiceManager(),
                                                            uno::UNO_QUERY_THROW);

        comphelper::setProcessServiceFactory(xFactory);
    }
    catch (const uno::Exception& e)
    {
        std::cerr << "Fatal Error: UNO Bootstrap failed: " << e.Message.toUtf8().getStr()
                  << std::endl;
        return 1;
    }

    if (!InitVCL())
    {
        std::cerr << "Fatal Error: InitVCL() failed." << std::endl;
        return 1;
    }

    {
        if (argc > 1)
        {
            OUString aPath = OUString::createFromAscii(argv[1]);
            OUString aFileUrl;
            osl::FileBase::getFileURLFromSystemPath(aPath, aFileUrl);
            INetURLObject aURL(aFileUrl);

            GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
            Graphic aGraphic;
            if (rFilter.ImportGraphic(aGraphic, aURL) == ERRCODE_NONE)
                PrintBitmap(aGraphic.GetBitmap());
        }
        else
        {
            ScopedVclPtrInstance<VirtualDevice> pDev;
            pDev->SetOutputSizePixel(Size(1200, 300));
            pDev->SetBackground(Wallpaper(COL_WHITE));
            pDev->SetTextColor(COL_BLACK);

            vcl::Font aFont(u"Arial"_ustr, Size(0, 100));
            aFont.SetWeight(WEIGHT_BOLD);
            pDev->SetFont(aFont);

            OUString aFullStr(u"LibreOffice Refactoring"_ustr);

            // We want to break the text exactly after "LibreOffice"
            tools::Long nTargetWidth = pDev->GetTextWidth(u"LibreOffice"_ustr);

            sal_Int32 nBreakPos = pDev->GetTextBreak(aFullStr, nTargetWidth, 0);

            OUString aBrokenStr = aFullStr.copy(0, nBreakPos);

            // Draw only the "broken" portion
            Point aStartPt(20, 20);
            pDev->Erase();
            pDev->DrawText(aStartPt, aBrokenStr);

            // Calculate visual bounds to crop the bitmap
            tools::Long nW = pDev->GetTextWidth(aBrokenStr);
            tools::Long nH = pDev->GetTextHeight();
            tools::Rectangle aCropRect(aStartPt, Size(nW, nH));

            // Add slight padding for visibility
            aCropRect.AdjustLeft(-5);
            aCropRect.AdjustTop(-5);
            aCropRect.AdjustRight(5);
            aCropRect.AdjustBottom(5);

            Bitmap aBmp = pDev->GetBitmap(aCropRect.TopLeft(), aCropRect.GetSize());

            std::cout << "[VERIFICATION] Breaking string at width: " << nTargetWidth << "\n";
            std::cout << "[VERIFICATION] TextGeometry::GetTextBreak returned index: " << nBreakPos
                      << "\n";

            PrintBitmap(aBmp, 100);
        }
    }

    comphelper::setProcessServiceFactory(nullptr);
    DeInitVCL();
    return 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
