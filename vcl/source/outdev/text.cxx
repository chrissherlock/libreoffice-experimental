/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/settings.hxx>

#include <drawmode.hxx>
#include <textlayout.hxx>

void OutputDevice::DrawText(Point const& rStartPt, OUString const& rStr, sal_Int32 nIndex,
                            sal_Int32 nLen, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, SalLayoutGlyphs const* pLayoutCache)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
        nLen = rStr.getLength() - nIndex;

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextAction(rStartPt, rStr, nIndex, nLen));

    RenderContext2::DrawText(rStartPt, rStr, nIndex, nLen, pVector, pDisplayText, pLayoutCache);
}

void OutputDevice::DrawText(tools::Rectangle const& rRect, OUString const& rOrigStr,
                            DrawTextFlags nStyle, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, vcl::ITextLayout* _pTextLayout)
{
    assert(!is_double_buffered_window());

    bool bDecomposeTextRectAction
        = (_pTextLayout != nullptr) && _pTextLayout->DecomposeTextRectAction();

    if (mpMetaFile && !bDecomposeTextRectAction)
        mpMetaFile->AddAction(new MetaTextRectAction(rRect, rOrigStr, nStyle));

    RenderContext2::DrawText(rRect, rOrigStr, nStyle, pVector, pDisplayText, _pTextLayout);
}

void OutputDevice::DrawTextArray(Point const& rStartPt, OUString const& rStr,
                                 tools::Long const* pDXAry, sal_Int32 nIndex, sal_Int32 nLen,
                                 SalLayoutFlags flags, SalLayoutGlyphs const* pSalLayoutCache)
{
    assert(!is_double_buffered_window());

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextArrayAction(rStartPt, rStr, pDXAry, nIndex, nLen));

    RenderContext2::DrawTextArray(rStartPt, rStr, pDXAry, nIndex, nLen, flags, pSalLayoutCache);
}

void OutputDevice::DrawStretchText(Point const& rStartPt, sal_uLong nWidth, OUString const& rStr,
                                   sal_Int32 nIndex, sal_Int32 nLen)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
        nLen = rStr.getLength() - nIndex;

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaStretchTextAction(rStartPt, nWidth, rStr, nIndex, nLen));

    RenderContext2::DrawStretchText(rStartPt, nWidth, rStr, nIndex, nLen);
}

void OutputDevice::AddTextRectActions(const tools::Rectangle& rRect, const OUString& rOrigStr,
                                      DrawTextFlags nStyle, GDIMetaFile& rMtf)
{
    if (rOrigStr.isEmpty() || rRect.IsEmpty())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    // temporarily swap in passed mtf for action generation, and
    // disable output generation.
    const bool bOutputEnabled(IsOutputEnabled());
    GDIMetaFile* pMtf = mpMetaFile;

    mpMetaFile = &rMtf;
    EnableOutput(false);

    // #i47157# Factored out to ImplDrawTextRect(), to be shared
    // between us and DrawText()
    vcl::DefaultTextLayout aLayout(*this);
    ImplDrawText(*this, rRect, rOrigStr, nStyle, nullptr, nullptr, aLayout);

    // and restore again
    EnableOutput(bOutputEnabled);
    mpMetaFile = pMtf;
}

void OutputDevice::SetDigitLanguage(LanguageType eTextLanguage)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLanguageAction(eTextLanguage));

    RenderContext2::SetDigitLanguage(eTextLanguage);
}

void OutputDevice::SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaLayoutModeAction(nTextLayoutMode));

    RenderContext2::SetLayoutMode(nTextLayoutMode);
}

void OutputDevice::SetTextColor(Color const& rColor)
{
    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaTextColorAction(
            GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings())));
    }

    RenderContext2::SetTextColor(rColor);
}

void OutputDevice::SetTextFillColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextFillColorAction(Color(), false));

    RenderContext2::SetTextFillColor();
}

void OutputDevice::SetTextFillColor(const Color& rColor)
{
    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaTextFillColorAction(
            GetDrawModeFillColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()), true));
    }

    RenderContext2::SetTextFillColor(rColor);
}

void OutputDevice::SetTextAlign(TextAlign eAlign)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextAlignAction(eAlign));

    RenderContext2::SetTextAlign(eAlign);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
