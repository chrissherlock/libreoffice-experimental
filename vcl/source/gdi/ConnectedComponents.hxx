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

#ifndef INCLUDED_VCL_SOURCE_GDI_CONNECTEDCOMPONENTS_HXX
#define INCLUDED_VCL_SOURCE_GDI_CONNECTEDCOMPONENTS_HXX

#include <utility>
#include <list>
#include <vector>
#include <tuple>

#include <basegfx/polygon/b2dpolygontools.hxx>
#include <sal/log.hxx>
#include <officecfg/Office/Common.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/svapp.hxx>
#include <vcl/bitmapaccess.hxx>

#include "pdfwriter_impl.hxx"

class MetaTransparentAction;

template <typename T> tools::Rectangle GetBoundsRect(T);

typedef ::std::pair<MetaAction*, int> Component; // MetaAction plus index in metafile

// List of (intersecting) actions, plus overall bounds
struct ConnectedComponents
{
    ConnectedComponents()
        : aComponentList()
        , aBounds()
        , aBgColor(COL_WHITE)
        , bIsSpecial(false)
        , bIsFullyTransparent(false)
    {
    }

    void SetBackground(Color const& rBgColor, tools::Rectangle const& rBounds);

    template <typename T> bool IsValidShape(T);

    template <> bool IsValidShape<MetaRectAction*>(MetaRectAction*);

    template <> bool IsValidShape<MetaPolygonAction*>(MetaPolygonAction* pAction);

    template <> bool IsValidShape<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction);

    template <> bool IsValidShape<MetaWallpaperAction*>(MetaWallpaperAction*);

    bool IsBackgroundNotCovered(MetaAction* pAction, OutputDevice const& rMapModeVDev);

    template <typename T> struct is_valid_background_region
    {
        static const bool value = false;
    };

    template <> struct is_valid_background_region<MetaRectAction*>
    {
        static const bool value = true;
    };

    template <> struct is_valid_background_region<MetaPolygonAction*>
    {
        static const bool value = true;
    };

    template <> struct is_valid_background_region<MetaPolyPolygonAction*>
    {
        static const bool value = true;
    };

    template <> struct is_valid_background_region<MetaWallpaperAction*>
    {
        static const bool value = true;
    };

    template <bool b> struct set_component_selector
    {
        static void implementation(ConnectedComponents* pBackgroundComponent,
                                   MetaAction* const pAction, VirtualDevice& rMapModeVDev)
        {
            const tools::Rectangle& rCurrRect = GetBoundsRect(pAction);

            if (pBackgroundComponent->IsBackgroundNotCovered(pAction, rMapModeVDev))
                pBackgroundComponent->SetBackground(rMapModeVDev.GetFillColor(), rCurrRect);
        }
    };

    template <> struct set_component_selector<true>
    {
        static void implementation(ConnectedComponents* pBackgroundComponent,
                                   MetaAction* const pAction, VirtualDevice* pMapModeVDev)
        {
            if (pAction->IsTransparent(pMapModeVDev))
            {
                // extend current bounds (next uniform action needs to fully cover this area)
                pBackgroundComponent->aBounds.Union(pAction->GetBoundsRect(pMapModeVDev));
            }
        }
    };

    template <typename T> void SetBackgroundComponent(T* const action, VirtualDevice* pMapModeVDev)
    {
        set_component_selector<is_valid_background_region<T>::value>::implementation(this, action,
                                                                                     *pMapModeVDev);
    }

    std::tuple<int, MetaAction*> ExtendAllBounds(GDIMetaFile const& rMtf, VirtualDevice* pMapModeVDev);

    template <typename T>
    std::tuple<int, bool> ExtendCurrentBounds(T* pCurrAct, VirtualDevice* pMapModeVDev, int,
                                              int nLastBgAction);

    std::tuple<int, bool> ExtendCurrentBounds_implementation(MetaAction* pCurrAct,
                                                             VirtualDevice* pMapModeVDev,
                                                             int nActionNum, int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaAction* pCurrAct, VirtualDevice* pMapModeVDev,
                                              int nActionNum, int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaTransparentAction* pCurrAct,
                                              VirtualDevice* pMapModeVDev, int nActionNum,
                                              int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaRectAction* pCurrAct, VirtualDevice* pMapModeVDev,
                                              int nActionNum, int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaRectAction* pCurrAct, VirtualDevice* pMapModeVDev,
                                              int nActionNum, int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaPolygonAction* pCurrAct,
                                              VirtualDevice* pMapModeVDev, int nActionNum,
                                              int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaPolyPolygonAction* pCurrAct,
                                              VirtualDevice* pMapModeVDev, int nActionNum,
                                              int nLastBgAction);

    template <>
    std::tuple<int, bool> ExtendCurrentBounds(MetaWallpaperAction* pCurrAct,
                                              VirtualDevice* pMapModeVDev, int nActionNum,
                                              int nLastBgAction);

    ::std::list<Component> aComponentList;
    tools::Rectangle aBounds;
    Color aBgColor;
    bool bIsSpecial;
    bool bIsFullyTransparent;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
