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

#include <cassert>

#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>
#include <sal/log.hxx>

#include <vcl/salnativewidgets.hxx>
#include <vcl/pdfextoutdevdata.hxx>

#include <salgdi.hxx>

Size RenderContext2::GetButtonBorderSize() { return Size(1, 1); };
Color RenderContext2::GetMonochromeButtonColor() { return COL_WHITE; }

ImplControlValue::~ImplControlValue() {}

ImplControlValue* ImplControlValue::clone() const
{
    assert(typeid(const ImplControlValue) == typeid(*this));
    return new ImplControlValue(*this);
}

ScrollbarValue::~ScrollbarValue() {}

ScrollbarValue* ScrollbarValue::clone() const
{
    assert(typeid(const ScrollbarValue) == typeid(*this));
    return new ScrollbarValue(*this);
}

SliderValue::~SliderValue() {}

SliderValue* SliderValue::clone() const
{
    assert(typeid(const SliderValue) == typeid(*this));
    return new SliderValue(*this);
}

int TabPaneValue::m_nOverlap = 0;

TabPaneValue* TabPaneValue::clone() const
{
    assert(typeid(const TabPaneValue) == typeid(*this));
    return new TabPaneValue(*this);
}

TabitemValue::~TabitemValue() {}

TabitemValue* TabitemValue::clone() const
{
    assert(typeid(const TabitemValue) == typeid(*this));
    return new TabitemValue(*this);
}

SpinbuttonValue::~SpinbuttonValue() {}

SpinbuttonValue* SpinbuttonValue::clone() const
{
    assert(typeid(const SpinbuttonValue) == typeid(*this));
    return new SpinbuttonValue(*this);
}

ToolbarValue::~ToolbarValue() {}

ToolbarValue* ToolbarValue::clone() const
{
    assert(typeid(const ToolbarValue) == typeid(*this));
    return new ToolbarValue(*this);
}

MenubarValue::~MenubarValue() {}

MenubarValue* MenubarValue::clone() const
{
    assert(typeid(const MenubarValue) == typeid(*this));
    return new MenubarValue(*this);
}

MenupopupValue::~MenupopupValue() {}

MenupopupValue* MenupopupValue::clone() const
{
    assert(typeid(const MenupopupValue) == typeid(*this));
    return new MenupopupValue(*this);
}

PushButtonValue::~PushButtonValue() {}

PushButtonValue* PushButtonValue::clone() const
{
    assert(typeid(const PushButtonValue) == typeid(*this));
    return new PushButtonValue(*this);
}

// These functions are mainly passthrough functions that allow access to
// the SalFrame behind a Window object for native widget rendering purposes.

bool RenderContext2::IsNativeControlSupported(ControlType nType, ControlPart nPart) const
{
    if (!CanEnableNativeWidget())
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    return mpGraphics->IsNativeControlSupported(nType, nPart);
}

bool RenderContext2::HitTestNativeScrollbar(ControlPart nPart,
                                            tools::Rectangle const& rControlRegion,
                                            Point const& aPos, bool& rIsInside) const
{
    if (!CanEnableNativeWidget())
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    Point aWinOffs(GetFrameOffset());
    tools::Rectangle screenRegion(rControlRegion);
    screenRegion.Move(aWinOffs.X(), aWinOffs.Y());

    return mpGraphics->HitTestNativeScrollbar(
        nPart, screenRegion,
        Point(aPos.X() + GetFrameOffset().X(), aPos.Y() + GetFrameOffset().Y()), rIsInside, *this);
}

static std::unique_ptr<ImplControlValue> TransformControlValue(ImplControlValue const& rVal,
                                                               RenderContext2 const& rDev)
{
    std::unique_ptr<ImplControlValue> aResult;
    switch (rVal.getType())
    {
        case ControlType::Slider:
        {
            SliderValue const* pSlVal = static_cast<SliderValue const*>(&rVal);
            SliderValue* pNew = new SliderValue(*pSlVal);
            aResult.reset(pNew);
            pNew->maThumbRect = rDev.ImplLogicToDevicePixel(pSlVal->maThumbRect);
        }
        break;
        case ControlType::Scrollbar:
        {
            ScrollbarValue const* pScVal = static_cast<ScrollbarValue const*>(&rVal);
            ScrollbarValue* pNew = new ScrollbarValue(*pScVal);
            aResult.reset(pNew);
            pNew->maThumbRect = rDev.ImplLogicToDevicePixel(pScVal->maThumbRect);
            pNew->maButton1Rect = rDev.ImplLogicToDevicePixel(pScVal->maButton1Rect);
            pNew->maButton2Rect = rDev.ImplLogicToDevicePixel(pScVal->maButton2Rect);
        }
        break;
        case ControlType::SpinButtons:
        {
            SpinbuttonValue const* pSpVal = static_cast<SpinbuttonValue const*>(&rVal);
            SpinbuttonValue* pNew = new SpinbuttonValue(*pSpVal);
            aResult.reset(pNew);
            pNew->maUpperRect = rDev.ImplLogicToDevicePixel(pSpVal->maUpperRect);
            pNew->maLowerRect = rDev.ImplLogicToDevicePixel(pSpVal->maLowerRect);
        }
        break;
        case ControlType::Toolbar:
        {
            ToolbarValue const* pTVal = static_cast<ToolbarValue const*>(&rVal);
            ToolbarValue* pNew = new ToolbarValue(*pTVal);
            aResult.reset(pNew);
            pNew->maGripRect = rDev.ImplLogicToDevicePixel(pTVal->maGripRect);
        }
        break;
        case ControlType::TabPane:
        {
            TabPaneValue const* pTIVal = static_cast<TabPaneValue const*>(&rVal);
            TabPaneValue* pNew = new TabPaneValue(*pTIVal);
            pNew->m_aTabHeaderRect = rDev.ImplLogicToDevicePixel(pTIVal->m_aTabHeaderRect);
            pNew->m_aSelectedTabRect = rDev.ImplLogicToDevicePixel(pTIVal->m_aSelectedTabRect);
            aResult.reset(pNew);
        }
        break;
        case ControlType::TabItem:
        {
            TabitemValue const* pTIVal = static_cast<TabitemValue const*>(&rVal);
            TabitemValue* pNew = new TabitemValue(*pTIVal);
            pNew->maContentRect = rDev.ImplLogicToDevicePixel(pTIVal->maContentRect);
            aResult.reset(pNew);
        }
        break;
        case ControlType::Menubar:
        {
            MenubarValue const* pMVal = static_cast<MenubarValue const*>(&rVal);
            MenubarValue* pNew = new MenubarValue(*pMVal);
            aResult.reset(pNew);
        }
        break;
        case ControlType::Pushbutton:
        {
            PushButtonValue const* pBVal = static_cast<PushButtonValue const*>(&rVal);
            PushButtonValue* pNew = new PushButtonValue(*pBVal);
            aResult.reset(pNew);
        }
        break;
        case ControlType::Generic:
            aResult = std::make_unique<ImplControlValue>(rVal);
            break;
        case ControlType::MenuPopup:
        {
            MenupopupValue const* pMVal = static_cast<MenupopupValue const*>(&rVal);
            MenupopupValue* pNew = new MenupopupValue(*pMVal);
            pNew->maItemRect = rDev.ImplLogicToDevicePixel(pMVal->maItemRect);
            aResult.reset(pNew);
        }
        break;
        default:
            std::abort();
            break;
    }
    return aResult;
}

bool RenderContext2::DrawNativeControl(ControlType nType, ControlPart nPart,
                                       tools::Rectangle const& rControlRegion, ControlState nState,
                                       ImplControlValue const& aValue, OUString const& aCaption,
                                       Color const& rBackgroundColor)
{
    assert(!is_double_buffered_window());

    if (!CanEnableNativeWidget())
        return false;

    // make sure the current clip region is initialized correctly
    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return true;

    if (mbInitLineColor)
        InitLineColor();

    if (mbInitFillColor)
        InitFillColor();

    // Convert the coordinates from relative to Window-absolute, so we draw
    // in the correct place in platform code
    std::unique_ptr<ImplControlValue> aScreenCtrlValue(TransformControlValue(aValue, *this));
    tools::Rectangle screenRegion(ImplLogicToDevicePixel(rControlRegion));

    bool bRet = mpGraphics->DrawNativeControl(nType, nPart, screenRegion, nState, *aScreenCtrlValue,
                                              aCaption, *this, rBackgroundColor);

    return bRet;
}

bool RenderContext2::GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                            tools::Rectangle const& rControlRegion,
                                            ControlState nState, ImplControlValue const& aValue,
                                            tools::Rectangle& rNativeBoundingRegion,
                                            tools::Rectangle& rNativeContentRegion) const
{
    if (!CanEnableNativeWidget())
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    // Convert the coordinates from relative to Window-absolute, so we draw
    // in the correct place in platform code
    std::unique_ptr<ImplControlValue> aScreenCtrlValue(TransformControlValue(aValue, *this));
    tools::Rectangle screenRegion(ImplLogicToDevicePixel(rControlRegion));

    bool bRet
        = mpGraphics->GetNativeControlRegion(nType, nPart, screenRegion, nState, *aScreenCtrlValue,
                                             rNativeBoundingRegion, rNativeContentRegion, *this);
    if (bRet)
    {
        // transform back native regions
        rNativeBoundingRegion = ImplDevicePixelToLogic(rNativeBoundingRegion);
        rNativeContentRegion = ImplDevicePixelToLogic(rNativeContentRegion);
    }

    return bRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
