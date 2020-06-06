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

#include <vcl/vcllayout.hxx>
#include <vcl/outdev.hxx>
#include <vcl/MetaStretchTextAction.hxx>

tools::Rectangle MetaStretchTextAction::GetBoundsRect(const OutputDevice* pOutDev) const
{
    const OUString aString(GetText().copy(GetIndex(), GetLen()));

    tools::Rectangle aActionBounds;

    // #i16195# Literate copy from TextArray action, the
    // semantics for the ImplLayout call are copied from the
    // OutDev::DrawStretchText() code. Unfortunately, also in
    // this case, public outdev methods such as GetTextWidth()
    // don't provide enough info.
    if (!aString.isEmpty())
    {
        // #105987# ImplLayout takes everything in logical coordinates
        std::unique_ptr<SalLayout> pSalLayout
            = pOutDev->ImplLayout(GetText(), GetIndex(), GetLen(), GetPoint(), GetWidth());
        if (pSalLayout)
        {
            tools::Rectangle aBoundRect(
                const_cast<OutputDevice*>(pOutDev)->ImplGetTextBoundRect(*pSalLayout));
            aActionBounds = pOutDev->PixelToLogic(aBoundRect);
        }
    }

    return ClipBounds(aActionBounds, pOutDev);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
