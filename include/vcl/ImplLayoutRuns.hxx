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

#pragma once

#include <vcl/dllapi.h>
#include <vcl/vclenum.hxx>

#include <vector>

sal_UCS4 VCL_DLLPUBLIC GetLocalizedChar(sal_UCS4 nChar, LanguageType eLang);
sal_UCS4 VCL_DLLPUBLIC GetMirroredChar(sal_UCS4 nChar);

// used for managing runs e.g. for BiDi, glyph and script fallback
class ImplLayoutRuns
{
public:
    ImplLayoutRuns();

    void Clear();
    void AddPos(int nCharPos, bool bRTL);
    void AddRun(int nMinRunPos, int nEndRunPos, bool bRTL);

    bool IsEmpty() const;
    void ResetPos();
    void NextRun();
    bool GetRun(int* nMinRunPos, int* nEndRunPos, bool* bRTL) const;
    bool GetNextPos(int* nCharPos, bool* bRTL);
    bool PosIsInRun(int nCharPos) const;
    bool PosIsInAnyRun(int nCharPos) const;

private:
    int mnRunIndex;
    std::vector<int> maRuns;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
