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

#include <sal/types.h>
#include <tools/fontenum.hxx>
#include <unotools/fontdefs.hxx>

#include <font/PhysicalFontFace.hxx>
#include <font/FontAttributes.hxx>
#include <font/FontSelectPattern.hxx>

PhysicalFontFace::PhysicalFontFace( const FontAttributes& rDFA )
    : FontAttributes( rDFA )
    , mnWidth(0)
    , mnHeight(0)
{
    // StarSymbol is a unicode font, but it still deserves the symbol flag
    if( !IsSymbolFont() )
        if ( IsStarSymbol( GetFamilyName() ) )
            SetSymbolFlag( true );
}

sal_Int32 PhysicalFontFace::CompareIgnoreSize( const PhysicalFontFace& rOther ) const
{
    // compare their width, weight, italic, style name and family name
    if( GetWidthTypeNoAsk() < rOther.GetWidthTypeNoAsk() )
        return -1;
    else if( GetWidthTypeNoAsk() > rOther.GetWidthTypeNoAsk() )
        return 1;

    if( GetWeightNoAsk() < rOther.GetWeightNoAsk() )
        return -1;
    else if( GetWeightNoAsk() > rOther.GetWeightNoAsk() )
        return 1;

    if( GetItalicNoAsk() < rOther.GetItalicNoAsk() )
        return -1;
    else if( GetItalicNoAsk() > rOther.GetItalicNoAsk() )
        return 1;

    sal_Int32 nRet = GetFamilyName().compareTo( rOther.GetFamilyName() );

    if (nRet == 0)
    {
        nRet = GetStyleName().compareTo( rOther.GetStyleName() );
    }

    return nRet;
}

sal_Int32 PhysicalFontFace::CompareWithSize( const PhysicalFontFace& rOther ) const
{
    sal_Int32 nCompare = CompareIgnoreSize( rOther );
    if (nCompare != 0)
        return nCompare;

    if( mnHeight < rOther.mnHeight )
        return -1;
    else if( mnHeight > rOther.mnHeight )
        return 1;

    if( mnWidth < rOther.mnWidth )
        return -1;
    else if( mnWidth > rOther.mnWidth )
        return 1;

    return 0;
}

bool PhysicalFontFace::IsBetterMatch( const FontSelectPattern& rFSD, FontMatchStatus& rStatus ) const
{
    int nMatch = 0;

    const OUString& rFontName = rFSD.maTargetName;
    if( rFontName.equalsIgnoreAsciiCase( GetFamilyName() ) )
        nMatch += 240000;

    if( rStatus.mpTargetStyleName
    &&  GetStyleName().equalsIgnoreAsciiCase( *rStatus.mpTargetStyleName ) )
        nMatch += 120000;

    if( (rFSD.GetPitchNoAsk() != PITCH_DONTKNOW) && (rFSD.GetPitchNoAsk() == GetPitchNoAsk()) )
        nMatch += 20000;

    // prefer NORMAL font width
    // TODO: change when the upper layers can tell their width preference
    if( GetWidthTypeNoAsk() == WIDTH_NORMAL )
        nMatch += 400;
    else if( (GetWidthTypeNoAsk() == WIDTH_SEMI_EXPANDED) || (GetWidthTypeNoAsk() == WIDTH_SEMI_CONDENSED) )
        nMatch += 300;

    if( rFSD.GetWeightNoAsk() != WEIGHT_DONTKNOW )
    {
        // if not bold or requiring emboldening prefer light fonts to bold fonts
        FontWeight ePatternWeight = rFSD.mbEmbolden ? WEIGHT_NORMAL : rFSD.GetWeightNoAsk();

        int nReqWeight = static_cast<int>(ePatternWeight);
        if ( ePatternWeight > WEIGHT_MEDIUM )
            nReqWeight += 100;

        int nGivenWeight = static_cast<int>(GetWeightNoAsk());
        if( GetWeightNoAsk() > WEIGHT_MEDIUM )
            nGivenWeight += 100;

        int nWeightDiff = nReqWeight - nGivenWeight;

        if ( nWeightDiff == 0 )
            nMatch += 1000;
        else if ( nWeightDiff == +1 || nWeightDiff == -1 )
            nMatch += 700;
        else if ( nWeightDiff < +50 && nWeightDiff > -50)
            nMatch += 200;
    }
    else // requested weight == WEIGHT_DONTKNOW
    {
        // prefer NORMAL font weight
        // TODO: change when the upper layers can tell their weight preference
        if( GetWeightNoAsk() == WEIGHT_NORMAL )
            nMatch += 450;
        else if( GetWeightNoAsk() == WEIGHT_MEDIUM )
            nMatch += 350;
        else if( (GetWeightNoAsk() == WEIGHT_SEMILIGHT) || (GetWeightNoAsk() == WEIGHT_SEMIBOLD) )
            nMatch += 200;
        else if( GetWeightNoAsk() == WEIGHT_LIGHT )
            nMatch += 150;
    }

    // if requiring custom matrix to fake italic, prefer upright font
    FontItalic ePatternItalic = rFSD.maItalicMatrix != ItalicMatrix() ? ITALIC_NONE : rFSD.GetItalicNoAsk();

    if ( ePatternItalic == ITALIC_NONE )
    {
        if( GetItalicNoAsk() == ITALIC_NONE )
            nMatch += 900;
    }
    else
    {
        if( ePatternItalic == GetItalicNoAsk() )
            nMatch += 900;
        else if( GetItalicNoAsk() != ITALIC_NONE )
            nMatch += 600;
    }

    int nHeightMatch = 0;
    int nWidthMatch = 0;

    if( rFSD.mnOrientation != 0_deg10 )
        nMatch += 80;
    else if( rFSD.mnWidth != 0 )
        nMatch += 25;
    else
        nMatch += 5;

    if( rStatus.mnFaceMatch > nMatch )
        return false;
    else if( rStatus.mnFaceMatch < nMatch )
    {
        rStatus.mnFaceMatch      = nMatch;
        rStatus.mnHeightMatch    = nHeightMatch;
        rStatus.mnWidthMatch     = nWidthMatch;
        return true;
    }

    // when two fonts are still competing prefer the
    // one with the best matching height
    if( rStatus.mnHeightMatch > nHeightMatch )
        return false;
    else if( rStatus.mnHeightMatch < nHeightMatch )
    {
        rStatus.mnHeightMatch    = nHeightMatch;
        rStatus.mnWidthMatch     = nWidthMatch;
        return true;
    }

    if( rStatus.mnWidthMatch > nWidthMatch )
        return false;

    rStatus.mnWidthMatch = nWidthMatch;
    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
