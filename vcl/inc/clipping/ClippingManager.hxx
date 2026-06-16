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
#include <vcl/window.hxx>

#include <clipping/ClipCompiler.hxx>
#include <clipping/ClipState.hxx>

#include "ClippingObserver.hxx"

#include <map>
#include <memory>
#include <vector>

namespace vcl::clipping
{
/**
 * ClippingManager acts as the coordinator for the reactive clipping graph.
 * It is owned by the OutputDevice and provides cached, compiled ClipPlans
 * for windows within its hierarchy.
 */
class VCL_DLLPUBLIC ClippingManager : public ClippingObserver
{
public:
    explicit ClippingManager(vcl::Window& rRoot);
    virtual ~ClippingManager() override;

    void AttachToHierarchy(vcl::Window* pWindow);

    virtual void OnWindowGeometryChanged(vcl::Window& rWindow) override;
    virtual void OnWindowDestroyed(vcl::Window& rWindow) override;

    const ClipPlan& GetClipPlan(vcl::Window& rWindow);
    void InvalidateWindow(vcl::Window& rWindow);

private:
    struct CacheEntry
    {
        ClipPlan maPlan;
        sal_uInt64 mnLastCompiledVersion = 0;
    };

    vcl::Window& mrRoot;
    std::map<vcl::Window*, CacheEntry> maCache;

    DECL_LINK(WindowEventHdl, VclWindowEvent&, void);
};
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
