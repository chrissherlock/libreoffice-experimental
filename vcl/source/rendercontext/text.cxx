/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <config_vcl.h>
#include <config_fuzzers.h>

#include <rtl/ustrbuf.hxx>
#include <tools/debug.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/TextLayoutCache.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/settings.hxx>
#include <vcl/vcllayout.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <fontinstance.hxx>
#include <fontselect.hxx>
#include <impfontcache.hxx>
#include <salgdi.hxx>
#include <sallayout.hxx>

void RenderContext2::ImplInitTextColor()
{
    DBG_TESTSOLARMUTEX();

    if (mbInitTextColor)
    {
        mpGraphics->SetTextColor(GetTextColor());
        mbInitTextColor = false;
    }
}

tools::Long RenderContext2::GetTextWidth(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                         vcl::TextLayoutCache const* const pLayoutCache,
                                         SalLayoutGlyphs const* const pSalLayoutCache) const
{
    tools::Long nWidth = GetTextArray(rStr, nullptr, nIndex, nLen, pLayoutCache, pSalLayoutCache);

    return nWidth;
}

#if !ENABLE_FUZZERS
const SalLayoutFlags eDefaultLayout = SalLayoutFlags::NONE;
#else
// ofz#23573 skip detecting bidi directions
const SalLayoutFlags eDefaultLayout = SalLayoutFlags::BiDiStrong;
#endif

bool RenderContext2::GetTextBoundRect(tools::Rectangle& rRect, OUString const& rStr,
                                      sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                      sal_uLong nLayoutWidth, tools::Long const* pDXAry,
                                      SalLayoutGlyphs const* pGlyphs) const
{
    bool bRet = false;
    rRect.SetEmpty();

    std::unique_ptr<SalLayout> pSalLayout;
    const Point aPoint;
    // calculate offset when nBase!=nIndex
    tools::Long nXOffset = 0;

    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;
        pSalLayout = ImplLayout(rStr, nStart, nOfsLen, aPoint, nLayoutWidth, pDXAry);

        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            nXOffset /= pSalLayout->GetUnitsPerPixel();

            // TODO: fix offset calculation for Bidi case
            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = ImplLayout(rStr, nIndex, nLen, aPoint, nLayoutWidth, pDXAry, eDefaultLayout,
                            nullptr, pGlyphs);

    if (pSalLayout)
    {
        tools::Rectangle aPixelRect;
        bRet = pSalLayout->GetBoundRect(aPixelRect);

        if (bRet)
        {
            int nWidthFactor = pSalLayout->GetUnitsPerPixel();

            if (nWidthFactor > 1)
            {
                double fFactor = 1.0 / nWidthFactor;
                aPixelRect.SetLeft(static_cast<tools::Long>(aPixelRect.Left() * fFactor));
                aPixelRect.SetRight(static_cast<tools::Long>(aPixelRect.Right() * fFactor));
                aPixelRect.SetTop(static_cast<tools::Long>(aPixelRect.Top() * fFactor));
                aPixelRect.SetBottom(static_cast<tools::Long>(aPixelRect.Bottom() * fFactor));
            }

            Point aRotatedOfs(mnTextOffX, mnTextOffY);
            aRotatedOfs -= pSalLayout->GetDrawPosition(Point(nXOffset, 0));
            aPixelRect += aRotatedOfs;
            rRect = PixelToLogic(aPixelRect);

            if (mbMap)
                rRect += Point(maMapRes.mnMapOfsX, maMapRes.mnMapOfsY);
        }
    }

    return bRet;
}

tools::Long RenderContext2::GetTextArray(const OUString& rStr, tools::Long* pDXAry,
                                         sal_Int32 nIndex, sal_Int32 nLen,
                                         vcl::TextLayoutCache const* const pLayoutCache,
                                         SalLayoutGlyphs const* const pSalLayoutCache) const
{
    if (nIndex >= rStr.getLength())
        return 0; // TODO: this looks like a buggy caller?

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    // do layout
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(
        rStr, nIndex, nLen, Point(0, 0), 0, nullptr, eDefaultLayout, pLayoutCache, pSalLayoutCache);

    if (!pSalLayout)
    {
        // The caller expects this to init the elements of pDXAry.
        // Adapting all the callers to check that GetTextArray succeeded seems
        // too much work.
        // Init here to 0 only in the (rare) error case, so that any missing
        // element init in the happy case will still be found by tools,
        // and hope that is sufficient.
        if (pDXAry)
            memset(pDXAry, 0, nLen * sizeof(*pDXAry));

        return 0;
    }

#if VCL_FLOAT_DEVICE_PIXEL
    std::unique_ptr<DeviceCoordinate[]> pDXPixelArray;
    if (pDXAry)
        pDXPixelArray.reset(new DeviceCoordinate[nLen]);

    DeviceCoordinate nWidth = pSalLayout->FillDXArray(pDXPixelArray.get());
    int nWidthFactor = pSalLayout->GetUnitsPerPixel();

    // convert virtual char widths to virtual absolute positions
    if (pDXPixelArray)
    {
        for (int i = 1; i < nLen; ++i)
        {
            pDXPixelArray[i] += pDXPixelArray[i - 1];
        }
    }
    if (mbMap)
    {
        if (pDXPixelArray)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] = ImplDevicePixelToLogicWidth(pDXPixelArray[i]);
            }
        }
        nWidth = ImplDevicePixelToLogicWidth(nWidth);
    }
    if (nWidthFactor > 1)
    {
        if (pDXPixelArray)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] /= nWidthFactor;
            }
        }
        nWidth /= nWidthFactor;
    }
    if (pDXAry)
    {
        for (int i = 0; i < nLen; ++i)
        {
            pDXAry[i] = basegfx::fround(pDXPixelArray[i]);
        }
    }
    return basegfx::fround(nWidth);

#else /* ! VCL_FLOAT_DEVICE_PIXEL */

    tools::Long nWidth = pSalLayout->FillDXArray(pDXAry);
    int nWidthFactor = pSalLayout->GetUnitsPerPixel();

    // convert virtual char widths to virtual absolute positions
    if (pDXAry)
    {
        for (int i = 1; i < nLen; ++i)
        {
            pDXAry[i] += pDXAry[i - 1];
        }
    }

    // convert from font units to logical units
    if (mbMap)
    {
        if (pDXAry)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXAry[i] = ImplDevicePixelToLogicWidth(pDXAry[i]);
            }
        }

        nWidth = ImplDevicePixelToLogicWidth(nWidth);
    }

    if (nWidthFactor > 1)
    {
        if (pDXAry)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXAry[i] /= nWidthFactor;
            }
        }

        nWidth /= nWidthFactor;
    }

    return nWidth;
#endif /* VCL_FLOAT_DEVICE_PIXEL */
}
ComplexTextLayoutFlags RenderContext2::GetLayoutMode() const { return mnTextLayoutMode; }

void RenderContext2::SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode)
{
    mnTextLayoutMode = nTextLayoutMode;

    if (mpAlphaVDev)
        mpAlphaVDev->SetLayoutMode(nTextLayoutMode);
}

LanguageType RenderContext2::GetDigitLanguage() const { return meTextLanguage; }

void RenderContext2::SetDigitLanguage(LanguageType eTextLanguage)
{
    meTextLanguage = eTextLanguage;

    if (mpAlphaVDev)
        mpAlphaVDev->SetDigitLanguage(eTextLanguage);
}

Color const& RenderContext2::GetTextColor() const { return maTextColor; }

void RenderContext2::SetTextColor(Color const& rColor)
{
    Color aColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (maTextColor != aColor)
    {
        maTextColor = aColor;
        mbInitTextColor = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextColor(COL_BLACK);
}

bool RenderContext2::IsTextFillColor() const { return !maFont.IsTransparent(); }

Color RenderContext2::GetTextFillColor() const
{
    if (maFont.IsTransparent())
        return COL_TRANSPARENT;
    else
        return maFont.GetFillColor();
}

void RenderContext2::SetTextFillColor()
{
    if (maFont.GetColor() != COL_TRANSPARENT)
        maFont.SetFillColor(COL_TRANSPARENT);

    if (!maFont.IsTransparent())
        maFont.SetTransparent(true);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor();
}

void RenderContext2::SetTextFillColor(Color const& rColor)
{
    Color aColor(rColor);
    bool bTransFill = aColor.IsTransparent();

    if (!bTransFill)
    {
        if (mnDrawMode
            & (DrawModeFlags::BlackFill | DrawModeFlags::WhiteFill | DrawModeFlags::GrayFill
               | DrawModeFlags::NoFill | DrawModeFlags::SettingsFill))
        {
            if (mnDrawMode & DrawModeFlags::BlackFill)
            {
                aColor = COL_BLACK;
            }
            else if (mnDrawMode & DrawModeFlags::WhiteFill)
            {
                aColor = COL_WHITE;
            }
            else if (mnDrawMode & DrawModeFlags::GrayFill)
            {
                const sal_uInt8 cLum = aColor.GetLuminance();
                aColor = Color(cLum, cLum, cLum);
            }
            else if (mnDrawMode & DrawModeFlags::SettingsFill)
            {
                aColor = GetSettings().GetStyleSettings().GetWindowColor();
            }
            else if (mnDrawMode & DrawModeFlags::NoFill)
            {
                aColor = COL_TRANSPARENT;
                bTransFill = true;
            }
        }
    }

    if (maFont.GetFillColor() != aColor)
        maFont.SetFillColor(aColor);

    if (maFont.IsTransparent() != bTransFill)
        maFont.SetTransparent(bTransFill);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor(COL_BLACK);
}

bool RenderContext2::IsTextLineColor() const { return !maTextLineColor.IsTransparent(); }

Color const& RenderContext2::GetTextLineColor() const { return maTextLineColor; }

void RenderContext2::SetTextLineColor()
{
    maTextLineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor();
}

void RenderContext2::SetTextLineColor(Color const& rColor)
{
    maTextLineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor(COL_BLACK);
}

bool RenderContext2::IsOverlineColor() const { return !maOverlineColor.IsTransparent(); }

Color const& RenderContext2::GetOverlineColor() const { return maOverlineColor; }

void RenderContext2::SetOverlineColor()
{
    maOverlineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor();
}

void RenderContext2::SetOverlineColor(Color const& rColor)
{
    maOverlineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor(COL_BLACK);
}

TextAlign RenderContext2::GetTextAlign() const { return maFont.GetAlignment(); }

void RenderContext2::SetTextAlign(TextAlign eAlign)
{
    if (maFont.GetAlignment() != eAlign)
    {
        maFont.SetAlignment(eAlign);
        mbNewFont = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextAlign(eAlign);
}

std::unique_ptr<SalLayout> RenderContext2::ImplLayout(
    OUString const& rOrigStr, sal_Int32 nMinIndex, sal_Int32 nLen, Point const& rLogicalPos,
    tools::Long nLogicalWidth, tools::Long const* pDXArray, SalLayoutFlags flags,
    vcl::TextLayoutCache const* pLayoutCache, SalLayoutGlyphs const* pGlyphs) const
{
    if (pGlyphs && !pGlyphs->IsValid())
    {
        SAL_WARN("vcl", "Trying to setup invalid cached glyphs - falling back to relayout!");
        pGlyphs = nullptr;
    }

    if (!InitFont())
        return nullptr;

    // check string index and length
    if (nLen == -1 || nMinIndex + nLen > rOrigStr.getLength())
    {
        const sal_Int32 nNewLen = rOrigStr.getLength() - nMinIndex;
        if (nNewLen <= 0)
            return nullptr;
        nLen = nNewLen;
    }

    OUString aStr = rOrigStr;

    // convert from logical units to physical units
    // recode string if needed
    if (mpFontInstance->mpConversion)
    {
        mpFontInstance->mpConversion->RecodeString(aStr, 0, aStr.getLength());
        pLayoutCache = nullptr; // don't use cache with modified string!
        pGlyphs = nullptr;
    }

    DeviceCoordinate nPixelWidth = static_cast<DeviceCoordinate>(nLogicalWidth);
    if (nLogicalWidth && mbMap)
        nPixelWidth = LogicWidthToDeviceCoordinate(nLogicalWidth);

    std::unique_ptr<DeviceCoordinate[]> xDXPixelArray;
    DeviceCoordinate* pDXPixelArray(nullptr);
    if (pDXArray)
    {
        if (mbMap)
        {
            // convert from logical units to font units using a temporary array
            xDXPixelArray.reset(new DeviceCoordinate[nLen]);
            pDXPixelArray = xDXPixelArray.get();
            // using base position for better rounding a.k.a. "dancing characters"
            DeviceCoordinate nPixelXOfs = LogicWidthToDeviceCoordinate(rLogicalPos.X());
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i]
                    = LogicWidthToDeviceCoordinate(rLogicalPos.X() + pDXArray[i]) - nPixelXOfs;
            }
        }
        else
        {
#if VCL_FLOAT_DEVICE_PIXEL
            xDXPixelArray.reset(new DeviceCoordinate[nLen]);
            pDXPixelArray = xDXPixelArray.get();
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] = pDXArray[i];
            }
#else /* !VCL_FLOAT_DEVICE_PIXEL */
            pDXPixelArray = const_cast<DeviceCoordinate*>(pDXArray);
#endif /* !VCL_FLOAT_DEVICE_PIXEL */
        }
    }

    ImplLayoutArgs aLayoutArgs = ImplPrepareLayoutArgs(aStr, nMinIndex, nLen, nPixelWidth,
                                                       pDXPixelArray, flags, pLayoutCache);

    // get matching layout object for base font
    std::unique_ptr<SalLayout> pSalLayout = mpGraphics->GetTextLayout(0);

    // layout text
    if (pSalLayout && !pSalLayout->LayoutText(aLayoutArgs, pGlyphs ? pGlyphs->Impl(0) : nullptr))
        pSalLayout.reset();

    if (!pSalLayout)
        return nullptr;

    // do glyph fallback if needed
    // #105768# avoid fallback for very small font sizes
    if (aLayoutArgs.NeedFallback() && mpFontInstance->GetFontSelectPattern().mnHeight >= 3)
        pSalLayout = ImplGlyphFallbackLayout(std::move(pSalLayout), aLayoutArgs, pGlyphs);

    // Return glyph items only after fallback handling. Otherwise they may contain invalid
    // glyph IDs.
    if (flags & SalLayoutFlags::GlyphItemsOnly)
        return pSalLayout;

    // position, justify, etc. the layout
    pSalLayout->AdjustLayout(aLayoutArgs);
    pSalLayout->DrawBase() = ImplLogicToDevicePixel(rLogicalPos);
    // adjust to right alignment if necessary
    if (aLayoutArgs.mnFlags & SalLayoutFlags::RightAlign)
    {
        DeviceCoordinate nRTLOffset;

        if (pDXPixelArray)
            nRTLOffset = pDXPixelArray[nLen - 1];
        else if (nPixelWidth)
            nRTLOffset = nPixelWidth;
        else
            nRTLOffset = pSalLayout->GetTextWidth() / pSalLayout->GetUnitsPerPixel();

        pSalLayout->DrawOffset().setX(1 - nRTLOffset);
    }

    return pSalLayout;
}

ImplLayoutArgs
RenderContext2::ImplPrepareLayoutArgs(OUString& rStr, const sal_Int32 nMinIndex,
                                      const sal_Int32 nLen, DeviceCoordinate nPixelWidth,
                                      DeviceCoordinate const* pDXArray, SalLayoutFlags nLayoutFlags,
                                      vcl::TextLayoutCache const* const pLayoutCache) const
{
    assert(nMinIndex >= 0);
    assert(nLen >= 0);

    // get string length for calculating extents
    sal_Int32 nEndIndex = rStr.getLength();

    if (nMinIndex + nLen < nEndIndex)
        nEndIndex = nMinIndex + nLen;

    // don't bother if there is nothing to do
    if (nEndIndex < nMinIndex)
        nEndIndex = nMinIndex;

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl)
        nLayoutFlags |= SalLayoutFlags::BiDiRtl;

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiStrong)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }
    else if (!(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl))
    {
        // Disable Bidi if no RTL hint and only known LTR codes used.
        bool bAllLtr = true;
        for (sal_Int32 i = nMinIndex; i < nEndIndex; i++)
        {
            // [0x0000, 0x052F] are Latin, Greek and Cyrillic.
            // [0x0370, 0x03FF] has a few holes as if Unicode 10.0.0, but
            //                  hopefully no RTL character will be encoded there.
            if (rStr[i] > 0x052F)
            {
                bAllLtr = false;
                break;
            }
        }

        if (bAllLtr)
            nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }

    if (!maFont.IsKerning())
        nLayoutFlags |= SalLayoutFlags::DisableKerning;

    if (maFont.GetKerning() & FontKerning::Asian)
        nLayoutFlags |= SalLayoutFlags::KerningAsian;

    if (maFont.IsVertical())
        nLayoutFlags |= SalLayoutFlags::Vertical;

    if (meTextLanguage) //TODO: (mnTextLayoutMode & ComplexTextLayoutFlags::SubstituteDigits)
    {
        // disable character localization when no digits used
        const sal_Unicode* pBase = rStr.getStr();
        const sal_Unicode* pStr = pBase + nMinIndex;
        const sal_Unicode* pEnd = pBase + nEndIndex;
        std::optional<OUStringBuffer> xTmpStr;
        for (; pStr < pEnd; ++pStr)
        {
            // TODO: are there non-digit localizations?
            if ((*pStr >= '0') && (*pStr <= '9'))
            {
                // translate characters to local preference
                sal_UCS4 cChar = GetLocalizedChar(*pStr, meTextLanguage);
                if (cChar != *pStr)
                {
                    if (!xTmpStr)
                        xTmpStr = OUStringBuffer(rStr);

                    // TODO: are the localized digit surrogates?
                    (*xTmpStr)[pStr - pBase] = cChar;
                }
            }
        }
        if (xTmpStr)
            rStr = (*xTmpStr).makeStringAndClear();
    }

    // right align for RTL text, DRAWPOS_REVERSED, RTL window style
    bool bRightAlign = bool(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl);

    if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginLeft)
        bRightAlign = false;
    else if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginRight)
        bRightAlign = true;

    // SSA: hack for western office, ie text get right aligned
    //      for debugging purposes of mirrored UI
    bool bRTLWindow = IsRTLEnabled();
    bRightAlign ^= bRTLWindow;

    if (bRightAlign)
        nLayoutFlags |= SalLayoutFlags::RightAlign;

    // set layout options
    ImplLayoutArgs aLayoutArgs(rStr, nMinIndex, nEndIndex, nLayoutFlags, maFont.GetLanguageTag(),
                               pLayoutCache);

    Degree10 nOrientation = mpFontInstance ? mpFontInstance->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);
    aLayoutArgs.SetDXArray(pDXArray);

    return aLayoutArgs;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
