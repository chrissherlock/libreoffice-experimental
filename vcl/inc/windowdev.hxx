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

#pragma once

#include <vcl/outdev.hxx>
#include <vcl/deviceconcepts.hxx>

namespace vcl
{
class WindowOutputDevice;

/**
 * Opt the WindowOutputDevice class into hardware acceleration paths.
 * This satisfies the HWAccelerated<T> concept, allowing generic algorithms
 * to compile Skia/OpenGL fast-paths that draw directly to the window surface.
 */
template <> struct supports_hw_acceleration<WindowOutputDevice> : std::true_type
{
};

class WindowOutputDevice final : public ::OutputDevice
{
public:
    static constexpr bool is_animatable_v = true;
    static constexpr bool is_paint_event_capable_v = true;
    static constexpr bool is_readable_raster_v = true;
    static constexpr bool is_framed_v = true;
    static constexpr bool is_double_buffered_v = true;

    WindowOutputDevice(vcl::Window& rOwnerWindow);
    virtual ~WindowOutputDevice() override;
    virtual void dispose() override;

    bool IsDoubleBufferedWindow() const;

    void Flush() override;

    void SaveBackground(VirtualDevice& rSaveDevice, const Point& rPos, const Size& rSize,
                        const Size&) const override;

    css::awt::DeviceInfo GetDeviceInfo() const override;

    virtual vcl::Region GetOutputBoundsClipRegion() const override;

    bool IsInPaint() const;
    vcl::Region GetPaintRegion() const;

    virtual bool AcquireGraphics() const override;
    virtual void ReleaseGraphics(bool bRelease = true) override;

    using ::OutputDevice::SetSettings;
    virtual void SetSettings(const AllSettings& rSettings) override;
    void SetSettings(const AllSettings& rSettings, bool bChild);

    bool CanEnableNativeWidget() const override;

    /** Get the vcl::Window that this OutputDevice belongs to, if any */
    virtual vcl::Window* GetOwnerWindow() const override { return mxOwnerWindow.get(); }

    virtual css::uno::Reference<css::rendering::XCanvas>
    ImplGetCanvas(bool bSpriteCanvas) const override;

    virtual bool HasAlpha() const override { return true; }
    void UpdateCursorOnMapModeChange();

private:
    virtual void InitClipRegion() override;

    void ImplClearFontData(bool bNewFontLists) override;
    void ImplRefreshFontData(bool bNewFontLists) override;

    virtual void CopyDeviceArea(SalTwoRect& aPosAry) override;
    virtual const OutputDevice* DrawOutDevDirectCheck(const OutputDevice& rSrcDev) const override;
    virtual void DrawOutDevDirectProcess(const OutputDevice& rSrcDev, SalTwoRect& rPosAry,
                                         SalGraphics* pSrcGraphics) override;

    VclPtr<vcl::Window> mxOwnerWindow;
};

}; // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
