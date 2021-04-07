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

#include <rtl/ustrbuf.hxx>
#include <tools/debug.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <unotools/configmgr.hxx>

#include <vcl/event.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/metric.hxx>
#include <vcl/print.hxx>
#include <vcl/virdev.hxx>

#include <outdev.h>
#include <window.h>
#include <drawmode.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/FeatureCollector.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

void OutputDevice::SetFont(vcl::Font const& rNewFont)
{
    vcl::Font aFont = GetDrawModeFont(rNewFont, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaFontAction(aFont));
        // the color and alignment actions don't belong here
        // TODO: get rid of them without breaking anything...
        mpMetaFile->AddAction(new MetaTextAlignAction(aFont.GetAlignment()));
        mpMetaFile->AddAction(
            new MetaTextFillColorAction(aFont.GetFillColor(), !aFont.IsTransparent()));

        // Optimization MT/HDU: COL_TRANSPARENT means SetFont should ignore the font color,
        // because SetTextColor() is used for this.
        // #i28759# maTextColor might have been changed behind our back, commit then, too.
        if (aFont.GetColor() != COL_TRANSPARENT
            && (aFont.GetColor() != maFont.GetColor() || aFont.GetColor() != maTextColor))
        {
            mpMetaFile->AddAction(new MetaTextColorAction(aFont.GetColor()));
        }
    }

    RenderContext2::SetFont(rNewFont);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
