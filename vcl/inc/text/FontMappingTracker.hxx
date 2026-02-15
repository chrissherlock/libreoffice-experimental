/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/outdev.hxx>
#include <vcl/font.hxx>

class SalLayout;

namespace vcl::text
{
class FontMappingTracker
{
public:
    static void StartTracking();
    static OutputDevice::FontMappingUseData FinishTracking();
    static bool IsTracking();
    static void TrackLayoutFonts(const vcl::Font& rFont, const SalLayout* pLayout);
};

} // namespace vcl::text
