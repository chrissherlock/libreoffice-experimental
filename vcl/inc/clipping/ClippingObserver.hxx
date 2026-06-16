/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>

namespace vcl
{
class Window;
}

namespace vcl::clipping
{
/**
 * ClippingObserver provides a decoupled mechanism for the ClippingManager
 * to listen to layout/geometry changes in the Window hierarchy without
 * Window class knowing anything about clipping internals.
 */
class VCL_DLLPUBLIC ClippingObserver
{
public:
    virtual ~ClippingObserver() = default;

    /**
     * Called when a window's spatial state changes (geometry, visibility,
     * or z-order). This acts as a signal to the ClippingManager to
     * mark the corresponding ClipPlan as dirty.
     */
    virtual void OnWindowGeometryChanged(vcl::Window& rWindow) = 0;

    /**
     * Called when a window is being destroyed to allow the manager
     * to clean up its cache and event listeners.
     */
    virtual void OnWindowDestroyed(vcl::Window& rWindow) = 0;
};

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
