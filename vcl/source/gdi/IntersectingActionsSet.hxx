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

#ifndef INCLUDED_VCL_SOURCE_GDI_CONNECTEDACTIONSSET_HXX
#define INCLUDED_VCL_SOURCE_GDI_CONNECTEDACTIONSSET_HXX

#include <tools/gen.hxx>

#include <vector>

struct IntersectingActions;
class GDIMetaFile;
class MetaAction;
class VirtualDevice;

class IntersectingActionsSet
{
public:
    bool ProcessIntersections(IntersectingActions& rTotalActions, tools::Rectangle& rTotalBounds,
                              bool bTreatSpecial);

    std::tuple<bool, IntersectingActions, tools::Rectangle>
    SearchForIntersectingEntries(MetaAction* pCurrAct, VirtualDevice* pMapModeVDev,
                                 IntersectingActions rBackgroundAction);

    void GenerateIntersectingActions(IntersectingActions& rBackgroundAction,
                                     MetaAction* pInitialAction, int nActionNum,
                                     GDIMetaFile const& rInMtf, VirtualDevice* pMapModeVDev);

    void UnmarkIntersectingActions(tools::Rectangle aOutputRect, bool bReduceTransparency,
                                   bool bTransparencyAutoMode);

    void PopulateIntersectingActionsMap(
        ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap, size_t nSize);

    void
    CreateBitmapActions(GDIMetaFile& rOutMtf, OutputDevice* pOutDev, GDIMetaFile const& rInMtf,
                        ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap,
                        tools::Rectangle aOutputRect, IntersectingActions const& rBackgroundAction,
                        long nMaxBmpDPIX, long nMaxBmpDPIY, bool bDownsampleBitmaps,
                        bool bReduceTransparency, bool bTransparencyAutoMode);

private:
    ::std::vector<IntersectingActions> maIntersectingActions;
};

bool DoesActionHandleTransparency(const MetaAction& rAct);

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
