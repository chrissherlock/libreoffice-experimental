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
#include <vcl/region.hxx>

#include "ClipCompiler.hxx"

class OutputDevice;

namespace vcl::clipping
{
template <typename T> struct BackendClipPolicy;

template <> struct VCL_DLLPUBLIC BackendClipPolicy<OutputDevice>
{
    static void Apply(OutputDevice& rDev, const ClipPlan& rPlan);
};

template <typename T> struct BackendClipPolicy
{
    static void Apply(T& rDev, const ClipPlan& rPlan)
    {
        // By default, implicitly cast specific devices (Printer, VirtualDevice, etc.)
        // to a generic OutputDevice. If a backend (like Skia) needs custom clipping
        // later, you simply add a specific template specialization for it.
        BackendClipPolicy<OutputDevice>::Apply(rDev, rPlan);
    }
};
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
