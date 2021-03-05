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

#pragma once

#include <tools/color.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/dllapi.h>
#include <vcl/flags/AntialiasingFlags.hxx>
#include <vcl/flags/ComplexTextLayoutFlags.hxx>
#include <vcl/flags/DrawModeFlags.hxx>
#include <vcl/font.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/vclreferencebase.hxx>
#include <vcl/wall.hxx>

class AllSettings;
class SalGraphics;
class VirtualDevice;

class VCL_DLLPUBLIC RenderContext2 : public virtual VclReferenceBase
{
public:
    RenderContext2();
    virtual ~RenderContext2();

    /** Get the graphic context that the output device uses to draw on.

     If no graphics device exists, then initialize it.

     @returns SalGraphics instance.
     */
    SalGraphics const* GetGraphics() const;
    SalGraphics* GetGraphics();

    void EnableOutput(bool bEnable = true);
    bool IsOutputEnabled() const;

    ComplexTextLayoutFlags GetLayoutMode() const;
    virtual void SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode);

    DrawModeFlags GetDrawMode() const;
    void SetDrawMode(DrawModeFlags nDrawMode);

    AntialiasingFlags GetAntialiasing() const;
    void SetAntialiasing(AntialiasingFlags nMode);

    LanguageType GetDigitLanguage() const;
    virtual void SetDigitLanguage(LanguageType);

    Color const& GetLineColor() const;
    bool IsLineColor() const;
    virtual void SetLineColor();
    virtual void SetLineColor(Color const& rColor);

    Color const& GetFillColor() const;
    bool IsFillColor() const;
    virtual void SetFillColor();
    virtual void SetFillColor(Color const& rColor);

    Color const& GetTextColor() const;
    virtual void SetTextColor(Color const& rColor);

    virtual void SetSettings(AllSettings const& rSettings);
    AllSettings const& GetSettings() const;

    bool IsBackground() const;
    Wallpaper const& GetBackground() const;
    virtual void SetBackground();
    virtual void SetBackground(Wallpaper const& rBackground);

protected:
    virtual void dispose() override;

    /** Acquire a graphics device that the output device uses to draw on.

     There is an LRU of OutputDevices that is used to get the graphics. The
     actual creation of a SalGraphics instance is done via the SalFrame
     implementation.

     However, the SalFrame instance will only return a valid SalGraphics
     instance if it is not in use or there wasn't one in the first place. When
     this happens, AcquireGraphics finds the least recently used OutputDevice
     in a different frame and "steals" it (releases it then starts using it).

     If there are no frames to steal an OutputDevice's SalGraphics instance from
     then it blocks until the graphics is released.

     Once it has acquired a graphics instance, then we add the OutputDevice to
     the LRU.

     @returns true if was able to initialize the graphics device, false otherwise.
     */
    virtual bool AcquireGraphics() const = 0;

    /** Release the graphics device, and remove it from the graphics device
     list.

     @param         bRelease    Determines whether to release the fonts of the
                                physically released graphics device.
     */
    virtual void ReleaseGraphics(bool bRelease = true) = 0;

    mutable SalGraphics* mpGraphics; ///< Graphics context to draw on
    VclPtr<VirtualDevice> mpAlphaVDev;

    ComplexTextLayoutFlags mnTextLayoutMode;
    DrawModeFlags mnDrawMode;
    AntialiasingFlags mnAntialiasing;
    std::unique_ptr<AllSettings> mxSettings;
    LanguageType meTextLanguage;
    Color maLineColor;
    Color maFillColor;
    vcl::Font maFont;
    Color maTextColor;
    Wallpaper maBackground;

    mutable bool mbInitFont : 1;
    mutable bool mbInitFillColor : 1;
    mutable bool mbInitTextColor : 1;
    mutable bool mbLineColor : 1;
    mutable bool mbInitLineColor : 1;
    mutable bool mbFillColor : 1;
    mutable bool mbNewFont : 1;
    mutable bool mbBackground : 1;

private:
    mutable bool mbOutput : 1;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
