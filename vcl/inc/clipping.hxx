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
#include <vcl/outdev.hxx>
#include <vcl/region.hxx>
#include <tools/gen.hxx>

namespace vcl::clipping
{
/**
 * Evaluates and synchronizes the active hardware/software clip region
 * for the given device.
 */
VCL_DLLPUBLIC void initDeviceClipRegion(OutputDevice& rDevice);

/**
 * Retrieves the currently active logic clip region from the device,
 * factoring in window paint bounds if applicable.
 */
VCL_DLLPUBLIC vcl::Region getActiveClipRegion(const OutputDevice& rDevice);

/**
 * Intersects the provided destination rectangle with the device's
 * active paint region bounds.
 */
VCL_DLLPUBLIC void clipToPaintRegion(OutputDevice& rDevice, tools::Rectangle& rDstRect);

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
