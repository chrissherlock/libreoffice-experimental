/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <sal/config.h>
#include <tools/color.hxx>

#include <vcl/settings.hxx>
#include <vcl/outdev.hxx>

#include <GraphicsState.hxx>

#include <cassert>

namespace vcl
{
Color GraphicsState::GetSingleColorGradientFill(const StyleSettings& rStyleSettings) const
{
    // We should never call this function if neither flag is set!
    assert(mnDrawMode & (DrawModeFlags::WhiteGradient | DrawModeFlags::SettingsGradient));

    if (mnDrawMode & DrawModeFlags::WhiteGradient)
    {
        return COL_WHITE;
    }

    if (mnDrawMode & DrawModeFlags::SettingsGradient)
    {
        if (mnDrawMode & DrawModeFlags::SettingsForSelection)
        {
            return rStyleSettings.GetHighlightColor();
        }
        else
        {
            return rStyleSettings.GetWindowColor();
        }
    }

    // Fallback (should be unreachable due to the assert above)
    return COL_WHITE;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
