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

#include <sal/types.h>
#include <comphelper/OAccessible.hxx>
#include <comphelper/diagnose_ex.hxx>

#include <vcl/CoordinateMapper.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <impfontcache.hxx>
#include <salframe.hxx>
#include <svdata.hxx>

#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/accessibility/XAccessibleContext.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>

// Forward declaration required due to mutual recursion
static css::uno::Reference<css::accessibility::XAccessibleEditableText> lcl_FindFocusedEditableText(
    css::uno::Reference<css::accessibility::XAccessibleContext> const& xContext);

static css::uno::Reference<css::accessibility::XAccessibleEditableText>
lcl_SearchChildrenForEditableText(
    const css::uno::Reference<css::accessibility::XAccessibleContext>& xContext, sal_Int64 nCount)
{
    for (sal_Int64 nChildIndex = 0; nChildIndex < nCount; ++nChildIndex)
    {
        css::uno::Reference<css::accessibility::XAccessible> xChild
            = xContext->getAccessibleChild(nChildIndex);

        if (!xChild.is())
            continue;

        css::uno::Reference<css::accessibility::XAccessibleContext> xChildContext
            = xChild->getAccessibleContext();

        if (!xChildContext.is())
            continue;

        css::uno::Reference<css::accessibility::XAccessibleEditableText> xText
            = lcl_FindFocusedEditableText(xChildContext);

        if (xText.is())
            return xText;
    }

    return css::uno::Reference<css::accessibility::XAccessibleEditableText>();
}

static css::uno::Reference<css::accessibility::XAccessibleEditableText> lcl_FindFocusedEditableText(
    css::uno::Reference<css::accessibility::XAccessibleContext> const& xContext)
{
    if (!xContext.is())
        return css::uno::Reference<css::accessibility::XAccessibleEditableText>();

    const sal_Int64 nState = xContext->getAccessibleStateSet();

    if (nState & css::accessibility::AccessibleStateType::FOCUSED)
    {
        css::uno::Reference<css::accessibility::XAccessibleEditableText> xText(xContext,
                                                                               css::uno::UNO_QUERY);
        if (xText.is())
            return xText;

        if (nState & css::accessibility::AccessibleStateType::MANAGES_DESCENDANTS)
            return css::uno::Reference<css::accessibility::XAccessibleEditableText>();
    }

    const sal_Int64 nCount = xContext->getAccessibleChildCount();

    if (nCount < 0 || nCount > SAL_MAX_UINT16 /* slow enough for anyone */)
        return css::uno::Reference<css::accessibility::XAccessibleEditableText>();

    return lcl_SearchChildrenForEditableText(xContext, nCount);
}

static css::uno::Reference<css::accessibility::XAccessibleEditableText>
lcl_GetxText(vcl::Window* pFocusWin)
{
    css::uno::Reference<css::accessibility::XAccessibleEditableText> xText;

    try
    {
        rtl::Reference<comphelper::OAccessible> pAccessible = pFocusWin->GetAccessible();

        if (pAccessible.is())
            xText = lcl_FindFocusedEditableText(pAccessible);
    }
    catch (const css::uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("vcl.gtk3", "Exception in getting input method surrounding text");
    }

    return xText;
}

namespace vcl
{
void Window::ImplNewInputContext()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pFocusWin = pSVData->mpWinData->mpFocusWin;

    if (!pFocusWin || !pFocusWin->mpWindowImpl || pFocusWin->isDisposed())
        return;

    // Is InputContext changed?
    const InputContext& rInputContext = pFocusWin->GetInputContext();

    if (rInputContext == pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext)
        return;

    pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext = rInputContext;

    SalInputContext aNewContext;
    const vcl::Font& rFont = rInputContext.GetFont();
    const OUString& rFontName = rFont.GetFamilyName();

    if (!rFontName.isEmpty())
    {
        OutputDevice* pFocusWinOutDev = pFocusWin->GetOutDev();
        Size aSize = pFocusWinOutDev->GetMapper().LogicToViewDistance(
            rFont.GetFontSize(), pFocusWinOutDev->GetMappingPolicy());

        if (!aSize.Height())
        {
            // only set default sizes if the font height in logical
            // coordinates equals 0
            if (rFont.GetFontSize().Height())
            {
                aSize.setHeight(1);
            }
            else
            {
                constexpr tools::Long nDefaultPointHeight = 12;
                constexpr tools::Long nPointsPerInch = 72;
                aSize.setHeight((nDefaultPointHeight * pFocusWin->GetOutDev()->GetDPIY())
                                / nPointsPerInch);
            }
        }

        aNewContext.mpFont = pFocusWin->GetOutDev()->mxFontCache->GetFontInstance(
            pFocusWin->GetOutDev()->mxFontCollection.get(), rFont, aSize,
            static_cast<float>(aSize.Height()));
    }

    aNewContext.mnOptions = rInputContext.GetOptions();
    pFocusWin->ImplGetFrame()->SetInputContext(&aNewContext);
}

void Window::SetInputContext(const InputContext& rInputContext)
{
    mpWindowImpl->maInputContext = rInputContext;

    if (!mpWindowImpl->mbInFocusHdl && HasFocus())
        ImplNewInputContext();
}

void Window::PostExtTextInputEvent(VclEventId nType, const OUString& rText)
{
    switch (nType)
    {
        case VclEventId::ExtTextInput:
        {
            std::unique_ptr<ExtTextInputAttr[]> pAttr(new ExtTextInputAttr[rText.getLength()]);
            for (int i = 0; i < rText.getLength(); ++i)
            {
                pAttr[i] = ExtTextInputAttr::Underline;
            }
            SalExtTextInputEvent aEvent{ rText, pAttr.get(), rText.getLength(),
                                         EXTTEXTINPUT_CURSOR_OVERWRITE };
            ImplWindowFrameProc(this, SalEvent::ExtTextInput, &aEvent);
        }
        break;

        case VclEventId::EndExtTextInput:
            ImplWindowFrameProc(this, SalEvent::EndExtTextInput, nullptr);
            break;

        default:
            assert(false);
    }
}

void Window::EndExtTextInput()
{
    if (mpWindowImpl->mbExtTextInput)
        ImplGetFrame()->EndExtTextInput(EndExtTextInputFlags::Complete);
}

void Window::SetCursorRect(const tools::Rectangle* pRect, tools::Long nExtTextInputWidth)
{
    ImplWinData* pWinData = ImplGetWinData();

    if (pRect)
        pWinData->mpCursorRect = *pRect;
    else
        pWinData->mpCursorRect.reset();

    pWinData->mnCursorExtWidth = nExtTextInputWidth;
}

const tools::Rectangle* Window::GetCursorRect() const
{
    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mpCursorRect ? &*pWinData->mpCursorRect : nullptr;
}

tools::Long Window::GetCursorExtTextInputWidth() const
{
    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mnCursorExtWidth;
}

void Window::SetCompositionCharRect(const tools::Rectangle* pRect, tools::Long nCompositionLength,
                                    bool bVertical)
{
    ImplWinData* pWinData = ImplGetWinData();
    pWinData->mpCompositionCharRects.reset();
    pWinData->mbVertical = bVertical;
    pWinData->mnCompositionCharRects = nCompositionLength;

    if (!pRect || (nCompositionLength <= 0))
        return;

    pWinData->mpCompositionCharRects.reset(new tools::Rectangle[nCompositionLength]);

    for (tools::Long i = 0; i < nCompositionLength; ++i)
    {
        pWinData->mpCompositionCharRects[i] = pRect[i];
    }
}

LanguageType Window::GetInputLanguage() const { return mpWindowImpl->mpFrame->GetInputLanguage(); }

OUString Window::GetSurroundingText() const { return OUString(); }

Selection Window::GetSurroundingTextSelection() const { return Selection(0, 0); }

// this is a rubbish implementation using a11y, ideally all subclasses implementing
// GetSurroundingText/GetSurroundingTextSelection should implement this and then this
// should be removed in favor of a stub that returns false
bool Window::DeleteSurroundingText(const Selection& rSelection)
{
    css::uno::Reference<css::accessibility::XAccessibleEditableText> xText = lcl_GetxText(this);

    if (!xText.is())
        return false;

    sal_Int32 nPosition = xText->getCaretPosition();
    // #i111768# range checking
    sal_Int32 nDeletePos = rSelection.Min();
    sal_Int32 nDeleteEnd = rSelection.Max();

    if (nDeletePos < 0)
        nDeletePos = 0;

    if (nDeleteEnd < 0)
        nDeleteEnd = 0;

    if (nDeleteEnd > xText->getCharacterCount())
        nDeleteEnd = xText->getCharacterCount();

    xText->deleteText(nDeletePos, nDeleteEnd);

    // tdf91641 adjust cursor if deleted chars shift it forward (normal case)
    if (nDeletePos >= nPosition)
        return true;

    if (nDeleteEnd <= nPosition)
        nPosition = nPosition - (nDeleteEnd - nDeletePos);
    else
        nPosition = nDeletePos;

    if (xText->getCharacterCount() >= nPosition)
        xText->setCaretPosition(nPosition);

    return true;
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
