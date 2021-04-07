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

#include <font/PhysicalFontFace.hxx>
#include <font/PhysicalFontFaceCollection.hxx>

PhysicalFontFaceCollection::PhysicalFontFaceCollection() { maDevFontVector.reserve(1024); }

void PhysicalFontFaceCollection::Add(PhysicalFontFace* pFace) { maDevFontVector.push_back(pFace); }

PhysicalFontFace* PhysicalFontFaceCollection::Get(int nIndex) const
{
    return maDevFontVector[nIndex].get();
}

int PhysicalFontFaceCollection::Count() const { return maDevFontVector.size(); }

FontMetric PhysicalFontFaceCollection::GetFontMetric(int nIndex) const
{
    PhysicalFontFace* pData = Get(nIndex);

    FontMetric aFontMetric;

    aFontMetric.SetFamilyName(pData->GetFamilyName());
    aFontMetric.SetStyleName(pData->GetStyleName());
    aFontMetric.SetCharSet(pData->GetCharSet());
    aFontMetric.SetFamily(pData->GetFamilyType());
    aFontMetric.SetPitch(pData->GetPitch());
    aFontMetric.SetWeight(pData->GetWeight());
    aFontMetric.SetItalic(pData->GetItalic());
    aFontMetric.SetAlignment(TextAlign::ALIGN_TOP);
    aFontMetric.SetWidthType(pData->GetWidthType());
    aFontMetric.SetQuality(pData->GetQuality());

    return aFontMetric;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
