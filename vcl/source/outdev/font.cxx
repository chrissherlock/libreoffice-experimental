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
#include <PhysicalFontCollection.hxx>
#include <drawmode.hxx>
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

bool OutputDevice::GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const
{
    if (!ImplNewFont())
        return false;

    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    if (!pFontInstance)
        return false;

    hb_font_t* pHbFont = pFontInstance->GetHbFont();
    if (!pHbFont)
        return false;

    hb_face_t* pHbFace = hb_font_get_face(pHbFont);
    if (!pHbFace)
        return false;

    const LanguageType eOfficeLanguage = Application::GetSettings().GetLanguageTag().getLanguageType();

    vcl::font::FeatureCollector aFeatureCollector(pHbFace, rFontFeatures, eOfficeLanguage);
    aFeatureCollector.collect();

    return true;
}

FontMetric OutputDevice::GetFontMetric() const
{
    FontMetric aMetric;
    if (!ImplNewFont())
        return aMetric;

    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    ImplFontMetricDataRef xFontMetric = pFontInstance->mxFontMetric;

    // prepare metric
    aMetric = maFont;

    // set aMetric with info from font
    aMetric.SetFamilyName( maFont.GetFamilyName() );
    aMetric.SetStyleName( xFontMetric->GetStyleName() );
    aMetric.SetFontSize( PixelToLogic( Size( xFontMetric->GetWidth(), xFontMetric->GetAscent() + xFontMetric->GetDescent() - xFontMetric->GetInternalLeading() ) ) );
    aMetric.SetCharSet( xFontMetric->IsSymbolFont() ? RTL_TEXTENCODING_SYMBOL : RTL_TEXTENCODING_UNICODE );
    aMetric.SetFamily( xFontMetric->GetFamilyType() );
    aMetric.SetPitch( xFontMetric->GetPitch() );
    aMetric.SetWeight( xFontMetric->GetWeight() );
    aMetric.SetItalic( xFontMetric->GetItalic() );
    aMetric.SetAlignment( TextAlign::ALIGN_TOP );
    aMetric.SetWidthType( xFontMetric->GetWidthType() );
    if ( pFontInstance->mnOwnOrientation )
        aMetric.SetOrientation( pFontInstance->mnOwnOrientation );
    else
        aMetric.SetOrientation( xFontMetric->GetOrientation() );

    // set remaining metric fields
    aMetric.SetFullstopCenteredFlag( xFontMetric->IsFullstopCentered() );
    aMetric.SetBulletOffset( xFontMetric->GetBulletOffset() );
    aMetric.SetAscent( ImplDevicePixelToLogicHeight( xFontMetric->GetAscent() + mnEmphasisAscent ) );
    aMetric.SetDescent( ImplDevicePixelToLogicHeight( xFontMetric->GetDescent() + mnEmphasisDescent ) );
    aMetric.SetInternalLeading( ImplDevicePixelToLogicHeight( xFontMetric->GetInternalLeading() + mnEmphasisAscent ) );
    // OutputDevice has its own external leading function due to #i60945#
    aMetric.SetExternalLeading( ImplDevicePixelToLogicHeight( GetFontExtLeading() ) );
    aMetric.SetLineHeight( ImplDevicePixelToLogicHeight( xFontMetric->GetAscent() + xFontMetric->GetDescent() + mnEmphasisAscent + mnEmphasisDescent ) );
    aMetric.SetSlant( ImplDevicePixelToLogicHeight( xFontMetric->GetSlant() ) );

    // get miscellaneous data
    aMetric.SetQuality( xFontMetric->GetQuality() );

    SAL_INFO("vcl.gdi.fontmetric", "OutputDevice::GetFontMetric:" << aMetric);

    xFontMetric = nullptr;

    return aMetric;
}

FontMetric OutputDevice::GetFontMetric( const vcl::Font& rFont ) const
{
    // select font, query metrics, select original font again
    vcl::Font aOldFont = GetFont();
    const_cast<OutputDevice*>(this)->SetFont( rFont );
    FontMetric aMetric( GetFontMetric() );
    const_cast<OutputDevice*>(this)->SetFont( aOldFont );
    return aMetric;
}

bool OutputDevice::GetFontCharMap( FontCharMapRef& rxFontCharMap ) const
{
    if (!InitFont())
        return false;

    FontCharMapRef xFontCharMap ( mpGraphics->GetFontCharMap() );
    if (!xFontCharMap.is())
    {
        FontCharMapRef xDefaultMap( new FontCharMap() );
        rxFontCharMap = xDefaultMap;
    }
    else
        rxFontCharMap = xFontCharMap;

    return !rxFontCharMap->IsDefaultMap();
}

bool OutputDevice::GetFontCapabilities( vcl::FontCapabilities& rFontCapabilities ) const
{
    if (!InitFont())
        return false;
    return mpGraphics->GetFontCapabilities(rFontCapabilities);
}

tools::Long OutputDevice::GetFontExtLeading() const
{
    return mpFontInstance->mxFontMetric->GetExternalLeading();
}

void OutputDevice::ImplClearFontData( const bool bNewFontLists )
{
    // the currently selected logical font is no longer needed
    mpFontInstance.clear();

    mbInitFont = true;
    mbNewFont = true;

    if ( bNewFontLists )
    {
        mpDeviceFontList.reset();
        mpDeviceFontSizeList.reset();

        // release all physically selected fonts on this device
        if( AcquireGraphics() )
            mpGraphics->ReleaseFonts();
    }

    ImplSVData* pSVData = ImplGetSVData();

    if (mxFontCache && mxFontCache != pSVData->maGDIData.mxScreenFontCache)
        mxFontCache->Invalidate();

    if (bNewFontLists && AcquireGraphics())
    {
        if (mxFontCollection && mxFontCollection != pSVData->maGDIData.mxScreenFontList)
            mxFontCollection->Clear();
    }
}

void OutputDevice::RefreshFontData( const bool bNewFontLists )
{
    ImplRefreshFontData( bNewFontLists );
}

void OutputDevice::ImplRefreshFontData( const bool bNewFontLists )
{
    if (bNewFontLists && AcquireGraphics())
            mpGraphics->GetDevFontList( mxFontCollection.get() );
}

void OutputDevice::ImplUpdateFontData()
{
    ImplClearFontData( true/*bNewFontLists*/ );
    ImplRefreshFontData( true/*bNewFontLists*/ );
}

void OutputDevice::ImplClearAllFontData(bool bNewFontLists)
{
    ImplSVData* pSVData = ImplGetSVData();

    ImplUpdateFontDataForAllFrames( &OutputDevice::ImplClearFontData, bNewFontLists );

    // clear global font lists to have them updated
    pSVData->maGDIData.mxScreenFontCache->Invalidate();
    if ( !bNewFontLists )
        return;

    pSVData->maGDIData.mxScreenFontList->Clear();
    vcl::Window * pFrame = pSVData->maFrameData.mpFirstFrame;
    if ( pFrame )
    {
        if ( pFrame->AcquireGraphics() )
        {
            OutputDevice *pDevice = pFrame;
            pDevice->mpGraphics->ClearDevFontCache();
            pDevice->mpGraphics->GetDevFontList(pFrame->mpWindowImpl->mpFrameData->mxFontCollection.get());
        }
    }
}

void OutputDevice::ImplRefreshAllFontData(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames( &OutputDevice::ImplRefreshFontData, bNewFontLists );
}

void OutputDevice::ImplUpdateAllFontData(bool bNewFontLists)
{
    OutputDevice::ImplClearAllFontData(bNewFontLists);
    OutputDevice::ImplRefreshAllFontData(bNewFontLists);
}

void OutputDevice::ImplUpdateFontDataForAllFrames( const FontUpdateHandler_t pHdl, const bool bNewFontLists )
{
    ImplSVData* const pSVData = ImplGetSVData();

    // update all windows
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    while ( pFrame )
    {
        ( pFrame->*pHdl )( bNewFontLists );

        vcl::Window* pSysWin = pFrame->mpWindowImpl->mpFrameData->mpFirstOverlap;
        while ( pSysWin )
        {
            ( pSysWin->*pHdl )( bNewFontLists );
            pSysWin = pSysWin->mpWindowImpl->mpNextOverlap;
        }

        pFrame = pFrame->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // update all virtual devices
    VirtualDevice* pVirDev = pSVData->maGDIData.mpFirstVirDev;
    while ( pVirDev )
    {
        ( pVirDev->*pHdl )( bNewFontLists );
        pVirDev = pVirDev->mpNext;
    }

    // update all printers
    Printer* pPrinter = pSVData->maGDIData.mpFirstPrinter;
    while ( pPrinter )
    {
        ( pPrinter->*pHdl )( bNewFontLists );
        pPrinter = pPrinter->mpNext;
    }
}

void OutputDevice::BeginFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maGDIData.mbFontSubChanged = false;
}

void OutputDevice::EndFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    if ( pSVData->maGDIData.mbFontSubChanged )
    {
        ImplUpdateAllFontData( false );

        DataChangedEvent aDCEvt( DataChangedEventType::FONTSUBSTITUTION );
        Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
        Application::NotifyAllWindows( aDCEvt );
        pSVData->maGDIData.mbFontSubChanged = false;
    }
}

void OutputDevice::AddFontSubstitute( const OUString& rFontName,
                                      const OUString& rReplaceFontName,
                                      AddFontSubstituteFlags nFlags )
{
    ImplDirectFontSubstitution*& rpSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if( !rpSubst )
        rpSubst = new ImplDirectFontSubstitution;
    rpSubst->AddFontSubstitute( rFontName, rReplaceFontName, nFlags );
    ImplGetSVData()->maGDIData.mbFontSubChanged = true;
}

void ImplDirectFontSubstitution::AddFontSubstitute( const OUString& rFontName,
    const OUString& rSubstFontName, AddFontSubstituteFlags nFlags )
{
    maFontSubstList.emplace_back( rFontName, rSubstFontName, nFlags );
}

ImplFontSubstEntry::ImplFontSubstEntry( const OUString& rFontName,
    const OUString& rSubstFontName, AddFontSubstituteFlags nSubstFlags )
:   mnFlags( nSubstFlags )
{
    maSearchName = GetEnglishSearchFontName( rFontName );
    maSearchReplaceName = GetEnglishSearchFontName( rSubstFontName );
}

void OutputDevice::RemoveFontsSubstitute()
{
    ImplDirectFontSubstitution* pSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if( pSubst )
        pSubst->RemoveFontsSubstitute();
}

void ImplDirectFontSubstitution::RemoveFontsSubstitute()
{
    maFontSubstList.clear();
}

bool ImplDirectFontSubstitution::FindFontSubstitute( OUString& rSubstName,
    std::u16string_view rSearchName ) const
{
    // TODO: get rid of O(N) searches
    std::vector<ImplFontSubstEntry>::const_iterator it = std::find_if (
                         maFontSubstList.begin(), maFontSubstList.end(),
                         [&] (const ImplFontSubstEntry& s) { return (s.mnFlags & AddFontSubstituteFlags::ALWAYS)
                                   && (s.maSearchName == rSearchName); } );
    if (it != maFontSubstList.end())
    {
        rSubstName = it->maSearchReplaceName;
        return true;
    }
    return false;
}

void ImplFontSubstitute( OUString& rFontName )
{
    // must be canonicalised
    assert( GetEnglishSearchFontName( rFontName ) == rFontName );

    OUString aSubstFontName;

    // apply user-configurable font replacement (eg, from the list in Tools->Options)
    const ImplDirectFontSubstitution* pSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if( pSubst && pSubst->FindFontSubstitute( aSubstFontName, rFontName ) )
    {
        rFontName = aSubstFontName;
        return;
    }
}

//hidpi TODO: This routine has hard-coded font-sizes that break places such as DialControl
vcl::Font OutputDevice::GetDefaultFont( DefaultFontType nType, LanguageType eLang,
                                        GetDefaultFontFlags nFlags, const OutputDevice* pOutDev )
{
    if (!pOutDev && !utl::ConfigManager::IsFuzzing()) // default is NULL
        pOutDev = Application::GetDefaultDevice();

    OUString aSearch;
    if (!utl::ConfigManager::IsFuzzing())
    {
        LanguageTag aLanguageTag(
                ( eLang == LANGUAGE_NONE || eLang == LANGUAGE_SYSTEM || eLang == LANGUAGE_DONTKNOW ) ?
                Application::GetSettings().GetUILanguageTag() :
                LanguageTag( eLang ));

        utl::DefaultFontConfiguration& rDefaults = utl::DefaultFontConfiguration::get();
        OUString aDefault = rDefaults.getDefaultFont( aLanguageTag, nType );

        if( !aDefault.isEmpty() )
            aSearch = aDefault;
        else
            aSearch = rDefaults.getUserInterfaceFont( aLanguageTag ); // use the UI font as a fallback
    }
    else
        aSearch = "Liberation Serif";

    vcl::Font aFont;
    aFont.SetPitch( PITCH_VARIABLE );

    switch ( nType )
    {
        case DefaultFontType::SANS_UNICODE:
        case DefaultFontType::UI_SANS:
            aFont.SetFamily( FAMILY_SWISS );
            break;

        case DefaultFontType::SANS:
        case DefaultFontType::LATIN_HEADING:
        case DefaultFontType::LATIN_SPREADSHEET:
        case DefaultFontType::LATIN_DISPLAY:
            aFont.SetFamily( FAMILY_SWISS );
            break;

        case DefaultFontType::SERIF:
        case DefaultFontType::LATIN_TEXT:
        case DefaultFontType::LATIN_PRESENTATION:
            aFont.SetFamily( FAMILY_ROMAN );
            break;

        case DefaultFontType::FIXED:
        case DefaultFontType::LATIN_FIXED:
        case DefaultFontType::UI_FIXED:
            aFont.SetPitch( PITCH_FIXED );
            aFont.SetFamily( FAMILY_MODERN );
            break;

        case DefaultFontType::SYMBOL:
            aFont.SetCharSet( RTL_TEXTENCODING_SYMBOL );
            break;

        case DefaultFontType::CJK_TEXT:
        case DefaultFontType::CJK_PRESENTATION:
        case DefaultFontType::CJK_SPREADSHEET:
        case DefaultFontType::CJK_HEADING:
        case DefaultFontType::CJK_DISPLAY:
            aFont.SetFamily( FAMILY_SYSTEM ); // don't care, but don't use font subst config later...
            break;

        case DefaultFontType::CTL_TEXT:
        case DefaultFontType::CTL_PRESENTATION:
        case DefaultFontType::CTL_SPREADSHEET:
        case DefaultFontType::CTL_HEADING:
        case DefaultFontType::CTL_DISPLAY:
            aFont.SetFamily( FAMILY_SYSTEM ); // don't care, but don't use font subst config later...
            break;
    }

    if ( !aSearch.isEmpty() )
    {
        aFont.SetFontHeight( 12 ); // corresponds to nDefaultHeight
        aFont.SetWeight( WEIGHT_NORMAL );
        aFont.SetLanguage( eLang );

        if ( aFont.GetCharSet() == RTL_TEXTENCODING_DONTKNOW )
            aFont.SetCharSet( osl_getThreadTextEncoding() );

        // Should we only return available fonts on the given device
        if ( pOutDev )
        {
            pOutDev->ImplInitFontList();

            // Search Font in the FontList
            OUString      aName;
            sal_Int32     nIndex = 0;
            do
            {
                PhysicalFontFamily* pFontFamily = pOutDev->mxFontCollection->FindFontFamily( GetNextFontToken( aSearch, nIndex ) );
                if( pFontFamily )
                {
                    AddTokenFontName( aName, pFontFamily->GetFamilyName() );
                    if( nFlags & GetDefaultFontFlags::OnlyOne )
                        break;
                }
            }
            while ( nIndex != -1 );
            aFont.SetFamilyName( aName );
        }

        // No Name, then set all names
        if ( aFont.GetFamilyName().isEmpty() )
        {
            if ( nFlags & GetDefaultFontFlags::OnlyOne )
            {
                if( !pOutDev )
                {
                    SAL_WARN_IF(!utl::ConfigManager::IsFuzzing(), "vcl.gdi", "No default window has been set for the application - we really shouldn't be able to get here");
                    aFont.SetFamilyName( aSearch.getToken( 0, ';' ) );
                }
                else
                {
                    pOutDev->ImplInitFontList();

                    aFont.SetFamilyName( aSearch );

                    // convert to pixel height
                    Size aSize = pOutDev->ImplLogicToDevicePixel( aFont.GetFontSize() );
                    if ( !aSize.Height() )
                    {
                        // use default pixel height only when logical height is zero
                        if ( aFont.GetFontHeight() )
                            aSize.setHeight( 1 );
                        else
                            aSize.setHeight( (12*pOutDev->GetDPIY())/72 );
                    }

                    // use default width only when logical width is zero
                    if( (0 == aSize.Width()) && (0 != aFont.GetFontSize().Width()) )
                        aSize.setWidth( 1 );

                    // get the name of the first available font
                    float fExactHeight = static_cast<float>(aSize.Height());
                    rtl::Reference<LogicalFontInstance> pFontInstance = pOutDev->mxFontCache->GetFontInstance( pOutDev->mxFontCollection.get(), aFont, aSize, fExactHeight );
                    if (pFontInstance)
                    {
                        assert(pFontInstance->GetFontFace());
                        aFont.SetFamilyName(pFontInstance->GetFontFace()->GetFamilyName());
                    }
                }
            }
            else
                aFont.SetFamilyName( aSearch );
        }
    }

#if OSL_DEBUG_LEVEL > 2
    const char* s = "SANS_UNKNOWN";
    switch ( nType )
    {
    case DefaultFontType::SANS_UNICODE: s = "SANS_UNICODE"; break;
    case DefaultFontType::UI_SANS: s = "UI_SANS"; break;

    case DefaultFontType::SANS: s = "SANS"; break;
    case DefaultFontType::LATIN_HEADING: s = "LATIN_HEADING"; break;
    case DefaultFontType::LATIN_SPREADSHEET: s = "LATIN_SPREADSHEET"; break;
    case DefaultFontType::LATIN_DISPLAY: s = "LATIN_DISPLAY"; break;

    case DefaultFontType::SERIF: s = "SERIF"; break;
    case DefaultFontType::LATIN_TEXT: s = "LATIN_TEXT"; break;
    case DefaultFontType::LATIN_PRESENTATION: s = "LATIN_PRESENTATION"; break;

    case DefaultFontType::FIXED: s = "FIXED"; break;
    case DefaultFontType::LATIN_FIXED: s = "LATIN_FIXED"; break;
    case DefaultFontType::UI_FIXED: s = "UI_FIXED"; break;

    case DefaultFontType::SYMBOL: s = "SYMBOL"; break;

    case DefaultFontType::CJK_TEXT: s = "CJK_TEXT"; break;
    case DefaultFontType::CJK_PRESENTATION: s = "CJK_PRESENTATION"; break;
    case DefaultFontType::CJK_SPREADSHEET: s = "CJK_SPREADSHEET"; break;
    case DefaultFontType::CJK_HEADING: s = "CJK_HEADING"; break;
    case DefaultFontType::CJK_DISPLAY: s = "CJK_DISPLAY"; break;

    case DefaultFontType::CTL_TEXT: s = "CTL_TEXT"; break;
    case DefaultFontType::CTL_PRESENTATION: s = "CTL_PRESENTATION"; break;
    case DefaultFontType::CTL_SPREADSHEET: s = "CTL_SPREADSHEET"; break;
    case DefaultFontType::CTL_HEADING: s = "CTL_HEADING"; break;
    case DefaultFontType::CTL_DISPLAY: s = "CTL_DISPLAY"; break;
    }
    SAL_INFO("vcl.gdi",
             "OutputDevice::GetDefaultFont() Type=" << s
             << " lang=" << eLang
             << " flags=" << static_cast<int>(nFlags)
             << " family=\"" << aFont.GetFamilyName() << "\"");
#endif

    return aFont;
}

const LogicalFontInstance* OutputDevice::GetFontInstance() const
{
    if (!InitFont())
        return nullptr;
    return mpFontInstance.get();
}

void OutputDevice::ImplDrawEmphasisMark( tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                         const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                         const tools::Rectangle& rRect1, const tools::Rectangle& rRect2 )
{
    if( IsRTLEnabled() )
        nX = nBaseX - (nX - nBaseX - 1);

    nX -= mnOutOffX;
    nY -= mnOutOffY;

    if ( rPolyPoly.Count() )
    {
        if ( bPolyLine )
        {
            tools::Polygon aPoly = rPolyPoly.GetObject( 0 );
            aPoly.Move( nX, nY );
            DrawPolyLine( aPoly );
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move( nX, nY );
            DrawPolyPolygon( aPolyPoly );
        }
    }

    if ( !rRect1.IsEmpty() )
    {
        tools::Rectangle aRect( Point( nX+rRect1.Left(),
                                nY+rRect1.Top() ), rRect1.GetSize() );
        DrawRect( aRect );
    }

    if ( !rRect2.IsEmpty() )
    {
        tools::Rectangle aRect( Point( nX+rRect2.Left(),
                                nY+rRect2.Top() ), rRect2.GetSize() );

        DrawRect( aRect );
    }
}

void OutputDevice::ImplDrawEmphasisMarks( SalLayout& rSalLayout )
{
    Color               aOldLineColor   = GetLineColor();
    Color               aOldFillColor   = GetFillColor();
    bool                bOldMap         = mbMap;
    GDIMetaFile*        pOldMetaFile    = mpMetaFile;
    mpMetaFile = nullptr;
    EnableMapMode( false );

    FontEmphasisMark    nEmphasisMark = ImplGetEmphasisMarkStyle( maFont );
    tools::PolyPolygon  aPolyPoly;
    tools::Rectangle           aRect1;
    tools::Rectangle           aRect2;
    tools::Long                nEmphasisYOff;
    tools::Long                nEmphasisWidth;
    tools::Long                nEmphasisHeight;
    bool                bPolyLine;

    if ( nEmphasisMark & FontEmphasisMark::PosBelow )
        nEmphasisHeight = mnEmphasisDescent;
    else
        nEmphasisHeight = mnEmphasisAscent;

    ImplGetEmphasisMark( aPolyPoly, bPolyLine,
                         aRect1, aRect2,
                         nEmphasisYOff, nEmphasisWidth,
                         nEmphasisMark,
                         nEmphasisHeight );

    if ( bPolyLine )
    {
        SetLineColor( GetTextColor() );
        SetFillColor();
    }
    else
    {
        SetLineColor();
        SetFillColor( GetTextColor() );
    }

    Point aOffset(0,0);

    if ( nEmphasisMark & FontEmphasisMark::PosBelow )
        aOffset.AdjustY(mpFontInstance->mxFontMetric->GetDescent() + nEmphasisYOff );
    else
        aOffset.AdjustY( -(mpFontInstance->mxFontMetric->GetAscent() + nEmphasisYOff) );

    tools::Long nEmphasisWidth2  = nEmphasisWidth / 2;
    tools::Long nEmphasisHeight2 = nEmphasisHeight / 2;
    aOffset += Point( nEmphasisWidth2, nEmphasisHeight2 );

    Point aOutPoint;
    tools::Rectangle aRectangle;
    const GlyphItem* pGlyph;
    int nStart = 0;
    while (rSalLayout.GetNextGlyph(&pGlyph, aOutPoint, nStart))
    {
        if (!pGlyph->GetGlyphBoundRect(aRectangle))
            continue;

        if (!pGlyph->IsSpacing())
        {
            Point aAdjPoint = aOffset;
            aAdjPoint.AdjustX(aRectangle.Left() + (aRectangle.GetWidth() - nEmphasisWidth) / 2 );
            if ( mpFontInstance->mnOrientation )
            {
                Point aOriginPt(0, 0);
                aOriginPt.RotateAround( aAdjPoint, mpFontInstance->mnOrientation );
            }
            aOutPoint += aAdjPoint;
            aOutPoint -= Point( nEmphasisWidth2, nEmphasisHeight2 );
            ImplDrawEmphasisMark( rSalLayout.DrawBase().X(),
                                  aOutPoint.X(), aOutPoint.Y(),
                                  aPolyPoly, bPolyLine, aRect1, aRect2 );
        }
    }

    SetLineColor( aOldLineColor );
    SetFillColor( aOldFillColor );
    EnableMapMode( bOldMap );
    mpMetaFile = pOldMetaFile;
}

tools::Long OutputDevice::GetMinKashida() const
{
    if (!ImplNewFont())
        return 0;

    return ImplDevicePixelToLogicWidth( mpFontInstance->mxFontMetric->GetMinKashida() );
}

sal_Int32 OutputDevice::ValidateKashidas ( const OUString& rTxt,
                                            sal_Int32 nIdx, sal_Int32 nLen,
                                            sal_Int32 nKashCount,
                                            const sal_Int32* pKashidaPos,
                                            sal_Int32* pKashidaPosDropped ) const
{
   // do layout
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout( rTxt, nIdx, nLen );
    if( !pSalLayout )
        return 0;
    sal_Int32 nDropped = 0;
    for( int i = 0; i < nKashCount; ++i )
    {
        if( !pSalLayout->IsKashidaPosValid( pKashidaPos[ i ] ))
        {
            pKashidaPosDropped[ nDropped ] = pKashidaPos [ i ];
            ++nDropped;
        }
    }
    return nDropped;
}

bool OutputDevice::GetGlyphBoundRects( const Point& rOrigin, const OUString& rStr,
                                           int nIndex, int nLen, MetricVector& rVector )
{
    rVector.clear();

    if( nIndex >= rStr.getLength() )
        return false;

    if( nLen < 0 || nIndex + nLen >= rStr.getLength() )
    {
        nLen = rStr.getLength() - nIndex;
    }

    tools::Rectangle aRect;
    for( int i = 0; i < nLen; i++ )
    {
        if( !GetTextBoundRect( aRect, rStr, nIndex, nIndex + i, 1 ) )
            break;
        aRect.Move( rOrigin.X(), rOrigin.Y() );
        rVector.push_back( aRect );
    }

    return (nLen == static_cast<int>(rVector.size()));
}

sal_Int32 OutputDevice::HasGlyphs( const vcl::Font& rTempFont, const OUString& rStr,
    sal_Int32 nIndex, sal_Int32 nLen ) const
{
    if( nIndex >= rStr.getLength() )
        return nIndex;
    sal_Int32 nEnd;
    if( nLen == -1 )
        nEnd = rStr.getLength();
    else
        nEnd = std::min( rStr.getLength(), nIndex + nLen );

    SAL_WARN_IF( nIndex >= nEnd, "vcl.gdi", "StartPos >= EndPos?" );
    SAL_WARN_IF( nEnd > rStr.getLength(), "vcl.gdi", "String too short" );

    // to get the map temporarily set font
    const vcl::Font aOrigFont = GetFont();
    const_cast<OutputDevice&>(*this).SetFont( rTempFont );
    FontCharMapRef xFontCharMap;
    bool bRet = GetFontCharMap( xFontCharMap );
    const_cast<OutputDevice&>(*this).SetFont( aOrigFont );

    // if fontmap is unknown assume it doesn't have the glyphs
    if( !bRet )
        return nIndex;

    for( sal_Int32 i = nIndex; nIndex < nEnd; ++i, ++nIndex )
        if( ! xFontCharMap->HasChar( rStr[i] ) )
            return nIndex;

    return -1;
}

void OutputDevice::ReleaseFontCache() { mxFontCache.reset(); }

void OutputDevice::ReleaseFontCollection() { mxFontCollection.reset(); }

void OutputDevice::SetFontCollectionFromSVData()
{
    mxFontCollection = ImplGetSVData()->maGDIData.mxScreenFontList->Clone();
}

void OutputDevice::ResetNewFontCache()
{
    mxFontCache = std::make_shared<ImplFontCache>();
}

void OutputDevice::ImplReleaseFonts()
{
    mpGraphics->ReleaseFonts();

    mbNewFont = true;
    mbInitFont = true;

    mpFontInstance.clear();
    mpDeviceFontList.reset();
    mpDeviceFontSizeList.reset();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
