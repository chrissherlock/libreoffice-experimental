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

#include <rtl/instance.hxx>
#include <tools/gen.hxx>
#include <tools/stream.hxx>
#include <tools/vcompat.hxx>
#include <unotools/fontcfg.hxx>
#include <unotools/fontdefs.hxx>
#include <i18nlangtag/mslangid.hxx>

#include <vcl/TypeSerializer.hxx>
#include <vcl/font.hxx>

#ifdef _WIN32
#include <vcl/metric.hxx>
#include <vcl/outdev.hxx>
#include <vcl/svapp.hxx>
#endif

#include <font/FontAttributes.hxx>
#include <font/sft.hxx>

#include <algorithm>
#include <string_view>

using namespace vcl;

namespace
{
    struct theGlobalDefault :
        public rtl::Static< Font::ImplType, theGlobalDefault > {};
}

Font::Font() : mpFontAttributes(theGlobalDefault::get())
{
}

Font::Font( const vcl::Font& rFont ) : mpFontAttributes( rFont.mpFontAttributes )
{
}

Font::Font( vcl::Font&& rFont ) noexcept : mpFontAttributes( std::move(rFont.mpFontAttributes) )
{
}

Font::Font( const OUString& rFamilyName, const Size& rSize ) : mpFontAttributes()
{
    mpFontAttributes->SetFamilyName( rFamilyName );
    mpFontAttributes->SetFontSize( rSize );
}

Font::Font( const OUString& rFamilyName, const OUString& rStyleName, const Size& rSize ) : mpFontAttributes()
{
    mpFontAttributes->SetFamilyName( rFamilyName );
    mpFontAttributes->SetStyleName( rStyleName );
    mpFontAttributes->SetFontSize( rSize );
}

Font::Font( FontFamily eFamily, const Size& rSize ) : mpFontAttributes()
{
    mpFontAttributes->SetFamilyType( eFamily );
    mpFontAttributes->SetFontSize( rSize );
}

Font::~Font()
{
}

void Font::SetColor( const Color& rColor )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->maColor != rColor)
    {
        mpFontAttributes->maColor = rColor;
    }
}

void Font::SetFillColor( const Color& rColor )
{
    mpFontAttributes->maFillColor = rColor;
    if ( rColor.IsTransparent() )
        mpFontAttributes->mbTransparent = true;
}

void Font::SetTransparent( bool bTransparent )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mbTransparent != bTransparent)
        mpFontAttributes->mbTransparent = bTransparent;
}

void Font::SetAlignment( FontAlign eAlign )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meAlign != eAlign)
        mpFontAttributes->SetAlignment(eAlign);
}

void Font::SetFamilyName( const OUString& rFamilyName )
{
    mpFontAttributes->SetFamilyName( rFamilyName );
}

void Font::SetStyleName( const OUString& rStyleName )
{
    mpFontAttributes->maStyleName = rStyleName;
}

void Font::SetFontSize( const Size& rSize )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetFontSize() != rSize)
        mpFontAttributes->SetFontSize( rSize );
}

void Font::SetFamily( FontFamily eFamily )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetFamilyTypeNoAsk() != eFamily)
        mpFontAttributes->SetFamilyType( eFamily );
}

void Font::SetCharSet( rtl_TextEncoding eCharSet )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetCharSet() != eCharSet)
    {
        mpFontAttributes->SetCharSet( eCharSet );

        if ( eCharSet == RTL_TEXTENCODING_SYMBOL )
            mpFontAttributes->SetSymbolFlag( true );
        else
            mpFontAttributes->SetSymbolFlag( false );
    }
}

bool Font::IsSymbolFont() const
{
    return mpFontAttributes->IsSymbolFont();
}

void Font::SetSymbolFlag( bool bSymbol )
{
    mpFontAttributes->SetSymbolFlag( bSymbol );

    if ( IsSymbolFont() )
    {
        mpFontAttributes->SetCharSet( RTL_TEXTENCODING_SYMBOL );
    }
    else
    {
        if ( mpFontAttributes->GetCharSet() == RTL_TEXTENCODING_SYMBOL )
            mpFontAttributes->SetCharSet( RTL_TEXTENCODING_DONTKNOW );
    }
}

void Font::SetLanguageTag( const LanguageTag& rLanguageTag )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->maLanguageTag != rLanguageTag)
        mpFontAttributes->maLanguageTag = rLanguageTag;
}

void Font::SetCJKContextLanguageTag( const LanguageTag& rLanguageTag )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->maCJKLanguageTag != rLanguageTag)
        mpFontAttributes->maCJKLanguageTag = rLanguageTag;
}

void Font::SetLanguage( LanguageType eLanguage )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->maLanguageTag.getLanguageType(false) != eLanguage)
        mpFontAttributes->maLanguageTag.reset( eLanguage);
}

void Font::SetCJKContextLanguage( LanguageType eLanguage )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->maCJKLanguageTag.getLanguageType(false) != eLanguage)
        mpFontAttributes->maCJKLanguageTag.reset( eLanguage);
}

void Font::SetPitch( FontPitch ePitch )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetPitchNoAsk() != ePitch)
        mpFontAttributes->SetPitch( ePitch );
}

void Font::SetOrientation( Degree10 nOrientation )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mnOrientation != nOrientation)
        mpFontAttributes->mnOrientation = nOrientation;
}

void Font::SetVertical( bool bVertical )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mbVertical != bVertical)
        mpFontAttributes->mbVertical = bVertical;
}

void Font::SetKerning( FontKerning eKerning )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meKerning != eKerning)
        mpFontAttributes->meKerning = eKerning;
}

bool Font::IsKerning() const
{
    return mpFontAttributes->meKerning != FontKerning::NONE;
}

void Font::SetWeight( FontWeight eWeight )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetWeightNoAsk() != eWeight)
        mpFontAttributes->SetWeight( eWeight );
}

void Font::SetWidthType( FontWidth eWidth )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetWidthTypeNoAsk() != eWidth)
        mpFontAttributes->SetWidthType( eWidth );
}

void Font::SetItalic( FontItalic eItalic )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->GetItalicNoAsk() != eItalic)
        mpFontAttributes->SetItalic( eItalic );
}

void Font::SetOutline( bool bOutline )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mbOutline != bOutline)
        mpFontAttributes->mbOutline = bOutline;
}

void Font::SetShadow( bool bShadow )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mbShadow != bShadow)
        mpFontAttributes->mbShadow = bShadow;
}

void Font::SetUnderline( FontLineStyle eUnderline )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meUnderline != eUnderline)
        mpFontAttributes->meUnderline = eUnderline;
}

void Font::SetOverline( FontLineStyle eOverline )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meOverline != eOverline)
        mpFontAttributes->meOverline = eOverline;
}

void Font::SetStrikeout( FontStrikeout eStrikeout )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meStrikeout != eStrikeout)
        mpFontAttributes->meStrikeout = eStrikeout;
}

void Font::SetRelief( FontRelief eRelief )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meRelief != eRelief)
        mpFontAttributes->meRelief = eRelief;
}

void Font::SetEmphasisMark( FontEmphasisMark eEmphasisMark )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->meEmphasisMark != eEmphasisMark )
        mpFontAttributes->meEmphasisMark = eEmphasisMark;
}

FontEmphasisMark Font::GetEmphasisMarkStyle() const
{
    FontEmphasisMark nEmphasisMark = GetEmphasisMark();

    // If no Position is set, then calculate the default position, which
    // depends on the language
    if (!(nEmphasisMark & (FontEmphasisMark::PosAbove | FontEmphasisMark::PosBelow)))
    {
        LanguageType eLang = GetLanguage();
        // In Chinese Simplified the EmphasisMarks are below/left
        if (MsLangId::isSimplifiedChinese(eLang))
        {
            nEmphasisMark |= FontEmphasisMark::PosBelow;
        }
        else
        {
            eLang = GetCJKContextLanguage();
            // In Chinese Simplified the EmphasisMarks are below/left
            if (MsLangId::isSimplifiedChinese(eLang))
                nEmphasisMark |= FontEmphasisMark::PosBelow;
            else
                nEmphasisMark |= FontEmphasisMark::PosAbove;
        }
    }

    return nEmphasisMark;
}

void Font::SetWordLineMode( bool bWordLine )
{
    if (const_cast<const ImplType&>(mpFontAttributes)->mbWordLine != bWordLine)
        mpFontAttributes->mbWordLine = bWordLine;
}

Font& Font::operator=( const vcl::Font& rFont )
{
    mpFontAttributes = rFont.mpFontAttributes;
    return *this;
}

Font& Font::operator=( vcl::Font&& rFont ) noexcept
{
    mpFontAttributes = std::move(rFont.mpFontAttributes);
    return *this;
}

bool Font::operator==( const vcl::Font& rFont ) const
{
    return mpFontAttributes == rFont.mpFontAttributes;
}

void Font::Merge( const vcl::Font& rFont )
{
    if ( !rFont.GetFamilyName().isEmpty() )
    {
        SetFamilyName( rFont.GetFamilyName() );
        SetStyleName( rFont.GetStyleName() );
        SetCharSet( GetCharSet() );
        SetLanguageTag( rFont.GetLanguageTag() );
        SetCJKContextLanguageTag( rFont.GetCJKContextLanguageTag() );
        // don't use access methods here, might lead to AskConfig(), if DONTKNOW
        SetFamily( rFont.mpFontAttributes->GetFamilyTypeNoAsk() );
        SetPitch( rFont.mpFontAttributes->GetPitchNoAsk() );
    }

    // don't use access methods here, might lead to AskConfig(), if DONTKNOW
    if ( rFont.mpFontAttributes->GetWeightNoAsk() != WEIGHT_DONTKNOW )
        SetWeight( rFont.GetWeight() );
    if ( rFont.mpFontAttributes->GetItalicNoAsk() != ITALIC_DONTKNOW )
        SetItalic( rFont.GetItalic() );
    if ( rFont.mpFontAttributes->GetWidthTypeNoAsk() != WIDTH_DONTKNOW )
        SetWidthType( rFont.GetWidthType() );

    if ( rFont.GetFontSize().Height() )
        SetFontSize( rFont.GetFontSize() );
    if ( rFont.GetUnderline() != LINESTYLE_DONTKNOW )
    {
        SetUnderline( rFont.GetUnderline() );
        SetWordLineMode( rFont.IsWordLineMode() );
    }
    if ( rFont.GetOverline() != LINESTYLE_DONTKNOW )
    {
        SetOverline( rFont.GetOverline() );
        SetWordLineMode( rFont.IsWordLineMode() );
    }
    if ( rFont.GetStrikeout() != STRIKEOUT_DONTKNOW )
    {
        SetStrikeout( rFont.GetStrikeout() );
        SetWordLineMode( rFont.IsWordLineMode() );
    }

    // Defaults?
    SetOrientation( rFont.GetOrientation() );
    SetVertical( rFont.IsVertical() );
    SetEmphasisMark( rFont.GetEmphasisMark() );
    SetKerning( rFont.IsKerning() ? FontKerning::FontSpecific : FontKerning::NONE );
    SetOutline( rFont.IsOutline() );
    SetShadow( rFont.IsShadow() );
    SetRelief( rFont.GetRelief() );
}

void Font::GetFontAttributes( FontAttributes& rAttrs ) const
{
    rAttrs.SetFamilyName( mpFontAttributes->GetFamilyName() );
    rAttrs.SetStyleName( mpFontAttributes->maStyleName );
    rAttrs.SetFamilyType( mpFontAttributes->GetFamilyTypeNoAsk() );
    rAttrs.SetPitch( mpFontAttributes->GetPitchNoAsk() );
    rAttrs.SetItalic( mpFontAttributes->GetItalicNoAsk() );
    rAttrs.SetWeight( mpFontAttributes->GetWeightNoAsk() );
    rAttrs.SetWidthType( WIDTH_DONTKNOW );
    rAttrs.SetSymbolFlag( mpFontAttributes->GetCharSet() == RTL_TEXTENCODING_SYMBOL );
}

SvStream& ReadFontAttributes( SvStream& rIStm, FontAttributes& rFontAttributes, tools::Long& rnNormedFontScaling )
{
    VersionCompatRead aCompat( rIStm );
    sal_uInt16      nTmp16(0);
    sal_Int16       nTmps16(0);
    bool            bTmp(false);
    sal_uInt8       nTmp8(0);

    rFontAttributes.SetFamilyName( rIStm.ReadUniOrByteString(rIStm.GetStreamCharSet()) );
    rFontAttributes.maStyleName = rIStm.ReadUniOrByteString(rIStm.GetStreamCharSet());
    TypeSerializer aSerializer(rIStm);
    aSerializer.readSize(rFontAttributes.maAverageFontSize);

    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.SetCharSet( static_cast<rtl_TextEncoding>(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.SetFamilyType( static_cast<FontFamily>(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.SetPitch( static_cast<FontPitch>(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.SetWeight( static_cast<FontWeight>(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.meUnderline = static_cast<FontLineStyle>(nTmp16);
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.meStrikeout = static_cast<FontStrikeout>(nTmp16);
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.SetItalic( static_cast<FontItalic>(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.maLanguageTag.reset( LanguageType(nTmp16) );
    rIStm.ReadUInt16( nTmp16 ); rFontAttributes.meWidthType = static_cast<FontWidth>(nTmp16);

    rIStm.ReadInt16( nTmps16 ); rFontAttributes.mnOrientation = Degree10(nTmps16);

    rIStm.ReadCharAsBool( bTmp ); rFontAttributes.mbWordLine = bTmp;
    rIStm.ReadCharAsBool( bTmp ); rFontAttributes.mbOutline = bTmp;
    rIStm.ReadCharAsBool( bTmp ); rFontAttributes.mbShadow = bTmp;
    rIStm.ReadUChar( nTmp8 ); rFontAttributes.meKerning = static_cast<FontKerning>(nTmp8);

    if( aCompat.GetVersion() >= 2 )
    {
        rIStm.ReadUChar( nTmp8 );     rFontAttributes.meRelief = static_cast<FontRelief>(nTmp8);
        rIStm.ReadUInt16( nTmp16 );   rFontAttributes.maCJKLanguageTag.reset( LanguageType(nTmp16) );
        rIStm.ReadCharAsBool( bTmp ); rFontAttributes.mbVertical = bTmp;
        rIStm.ReadUInt16( nTmp16 );   rFontAttributes.meEmphasisMark = static_cast<FontEmphasisMark>(nTmp16);
    }

    if( aCompat.GetVersion() >= 3 )
    {
        rIStm.ReadUInt16( nTmp16 ); rFontAttributes.meOverline = static_cast<FontLineStyle>(nTmp16);
    }

    // tdf#127471 read NormedFontScaling
    if( aCompat.GetVersion() >= 4 )
    {
        sal_Int32 nNormedFontScaling(0);
        rIStm.ReadInt32(nNormedFontScaling);
        rnNormedFontScaling = nNormedFontScaling;
    }

    // Relief
    // CJKContextLanguage

    return rIStm;
}

SvStream& WriteFontAttributes( SvStream& rOStm, const FontAttributes& rFontAttributes, const tools::Long& rnNormedFontScaling )
{
    // tdf#127471 increase to version 4
    VersionCompatWrite aCompat( rOStm, 4 );

    TypeSerializer aSerializer(rOStm);
    rOStm.WriteUniOrByteString( rFontAttributes.GetFamilyName(), rOStm.GetStreamCharSet() );
    rOStm.WriteUniOrByteString( rFontAttributes.GetStyleName(), rOStm.GetStreamCharSet() );
    aSerializer.writeSize(rFontAttributes.maAverageFontSize);

    rOStm.WriteUInt16( GetStoreCharSet( rFontAttributes.GetCharSet() ) );
    rOStm.WriteUInt16( rFontAttributes.GetFamilyTypeNoAsk() );
    rOStm.WriteUInt16( rFontAttributes.GetPitchNoAsk() );
    rOStm.WriteUInt16( rFontAttributes.GetWeightNoAsk() );
    rOStm.WriteUInt16( rFontAttributes.meUnderline );
    rOStm.WriteUInt16( rFontAttributes.meStrikeout );
    rOStm.WriteUInt16( rFontAttributes.GetItalicNoAsk() );
    rOStm.WriteUInt16( static_cast<sal_uInt16>(rFontAttributes.maLanguageTag.getLanguageType( false)) );
    rOStm.WriteUInt16( rFontAttributes.GetWidthTypeNoAsk() );

    rOStm.WriteInt16( rFontAttributes.mnOrientation.get() );

    rOStm.WriteBool( rFontAttributes.mbWordLine );
    rOStm.WriteBool( rFontAttributes.mbOutline );
    rOStm.WriteBool( rFontAttributes.mbShadow );
    rOStm.WriteUChar( static_cast<sal_uInt8>(rFontAttributes.meKerning) );

    // new in version 2
    rOStm.WriteUChar( static_cast<unsigned char>(rFontAttributes.meRelief) );
    rOStm.WriteUInt16( static_cast<sal_uInt16>(rFontAttributes.maCJKLanguageTag.getLanguageType( false)) );
    rOStm.WriteBool( rFontAttributes.mbVertical );
    rOStm.WriteUInt16( static_cast<sal_uInt16>(rFontAttributes.meEmphasisMark) );

    // new in version 3
    rOStm.WriteUInt16( rFontAttributes.meOverline );

    // new in version 4, NormedFontScaling
    rOStm.WriteInt32(rnNormedFontScaling);

    return rOStm;
}

SvStream& ReadFont( SvStream& rIStm, vcl::Font& rFont )
{
    // tdf#127471 try to read NormedFontScaling
    tools::Long nNormedFontScaling(0);
    SvStream& rRetval(ReadFontAttributes( rIStm, *rFont.mpFontAttributes, nNormedFontScaling ));

    if (nNormedFontScaling > 0)
    {
#ifdef _WIN32
        // we run on windows and a NormedFontScaling was written
        if(rFont.GetFontSize().getWidth() == nNormedFontScaling)
        {
            // the writing producer was running on a non-windows system, adapt to needed windows
            // system-specific pre-multiply
            const tools::Long nHeight(std::max<tools::Long>(rFont.GetFontSize().getHeight(), 0));
            sal_uInt32 nScaledWidth(0);

            if(nHeight > 0)
            {
                vcl::Font aUnscaledFont(rFont);
                aUnscaledFont.SetAverageFontWidth(0);
                const FontMetric aUnscaledFontMetric(Application::GetDefaultDevice()->GetFontMetric(aUnscaledFont));

                if (aUnscaledFontMetric.GetAverageFontWidth() > 0)
                {
                    const double fScaleFactor(static_cast<double>(nNormedFontScaling) / static_cast<double>(nHeight));
                    nScaledWidth = basegfx::fround(static_cast<double>(aUnscaledFontMetric.GetAverageFontWidth()) * fScaleFactor);
                }
            }

            rFont.SetAverageFontWidth(nScaledWidth);
        }
        else
        {
            // the writing producer was on a windows system, correct pre-multiplied value
            // is already set, nothing to do. Ignore 2nd value. Here a check
            // could be done if adapting the 2nd, NormedFontScaling value would be similar to
            // the set value for plausibility reasons
        }
#else
        // we do not run on windows and a NormedFontScaling was written
        if(rFont.GetFontSize().getWidth() == nNormedFontScaling)
        {
            // the writing producer was not on a windows system, correct value
            // already set, nothing to do
        }
        else
        {
            // the writing producer was on a windows system, correct FontScaling.
            // The correct non-pre-multiplied value is the 2nd one, use it
            rFont.SetAverageFontWidth(nNormedFontScaling);
        }
#endif
    }

    return rRetval;
}

SvStream& WriteFont( SvStream& rOStm, const vcl::Font& rFont )
{
    // tdf#127471 prepare NormedFontScaling for additional export
    tools::Long nNormedFontScaling(rFont.GetFontSize().getWidth());

    // FontScaling usage at vcl-Font is detected by checking that FontWidth != 0
    if (nNormedFontScaling > 0)
    {
        const tools::Long nHeight(std::max<tools::Long>(rFont.GetFontSize().getHeight(), 0));

        // check for negative height
        if(0 == nHeight)
        {
            nNormedFontScaling = 0;
        }
        else
        {
#ifdef _WIN32
            // for WIN32 the value is pre-multiplied with AverageFontWidth
            // which makes it system-dependent. Turn that back to have the
            // normed non-windows form of it for export as 2nd value
            vcl::Font aUnscaledFont(rFont);
            aUnscaledFont.SetAverageFontWidth(0);
            const FontMetric aUnscaledFontMetric(
                Application::GetDefaultDevice()->GetFontMetric(aUnscaledFont));

            if (aUnscaledFontMetric.GetAverageFontWidth() > 0)
            {
                const double fScaleFactor(
                    static_cast<double>(nNormedFontScaling)
                    / static_cast<double>(aUnscaledFontMetric.GetAverageFontWidth()));
                nNormedFontScaling = static_cast<tools::Long>(fScaleFactor * nHeight);
            }
#endif
        }
    }

    return WriteFontAttributes( rOStm, *rFont.mpFontAttributes, nNormedFontScaling );
}

namespace
{
    bool identifyTrueTypeFont( const void* i_pBuffer, sal_uInt32 i_nSize, Font& o_rResult )
    {
        bool bResult = false;
        TrueTypeFont* pTTF = nullptr;
        if( OpenTTFontBuffer( i_pBuffer, i_nSize, 0, &pTTF ) == SFErrCodes::Ok )
        {
            TTGlobalFontInfo aInfo;
            GetTTGlobalFontInfo( pTTF, &aInfo );
            // most importantly: the family name
            if( aInfo.ufamily )
                o_rResult.SetFamilyName( OUString(aInfo.ufamily) );
            else if( aInfo.family )
                o_rResult.SetFamilyName( OStringToOUString( aInfo.family, RTL_TEXTENCODING_ASCII_US ) );
            // set weight
            if( aInfo.weight )
            {
                if( aInfo.weight < FW_EXTRALIGHT )
                    o_rResult.SetWeight( WEIGHT_THIN );
                else if( aInfo.weight < FW_LIGHT )
                    o_rResult.SetWeight( WEIGHT_ULTRALIGHT );
                else if( aInfo.weight < FW_NORMAL )
                    o_rResult.SetWeight( WEIGHT_LIGHT );
                else if( aInfo.weight < FW_MEDIUM )
                    o_rResult.SetWeight( WEIGHT_NORMAL );
                else if( aInfo.weight < FW_SEMIBOLD )
                    o_rResult.SetWeight( WEIGHT_MEDIUM );
                else if( aInfo.weight < FW_BOLD )
                    o_rResult.SetWeight( WEIGHT_SEMIBOLD );
                else if( aInfo.weight < FW_EXTRABOLD )
                    o_rResult.SetWeight( WEIGHT_BOLD );
                else if( aInfo.weight < FW_BLACK )
                    o_rResult.SetWeight( WEIGHT_ULTRABOLD );
                else
                    o_rResult.SetWeight( WEIGHT_BLACK );
            }
            else
                o_rResult.SetWeight( (aInfo.macStyle & 1) ? WEIGHT_BOLD : WEIGHT_NORMAL );
            // set width
            if( aInfo.width )
            {
                if( aInfo.width == FWIDTH_ULTRA_CONDENSED )
                    o_rResult.SetAverageFontWidth( WIDTH_ULTRA_CONDENSED );
                else if( aInfo.width == FWIDTH_EXTRA_CONDENSED )
                    o_rResult.SetAverageFontWidth( WIDTH_EXTRA_CONDENSED );
                else if( aInfo.width == FWIDTH_CONDENSED )
                    o_rResult.SetAverageFontWidth( WIDTH_CONDENSED );
                else if( aInfo.width == FWIDTH_SEMI_CONDENSED )
                    o_rResult.SetAverageFontWidth( WIDTH_SEMI_CONDENSED );
                else if( aInfo.width == FWIDTH_NORMAL )
                    o_rResult.SetAverageFontWidth( WIDTH_NORMAL );
                else if( aInfo.width == FWIDTH_SEMI_EXPANDED )
                    o_rResult.SetAverageFontWidth( WIDTH_SEMI_EXPANDED );
                else if( aInfo.width == FWIDTH_EXPANDED )
                    o_rResult.SetAverageFontWidth( WIDTH_EXPANDED );
                else if( aInfo.width == FWIDTH_EXTRA_EXPANDED )
                    o_rResult.SetAverageFontWidth( WIDTH_EXTRA_EXPANDED );
                else if( aInfo.width >= FWIDTH_ULTRA_EXPANDED )
                    o_rResult.SetAverageFontWidth( WIDTH_ULTRA_EXPANDED );
            }
            // set italic
            o_rResult.SetItalic( (aInfo.italicAngle != 0) ? ITALIC_NORMAL : ITALIC_NONE );

            // set pitch
            o_rResult.SetPitch( (aInfo.pitch == 0) ? PITCH_VARIABLE : PITCH_FIXED );

            // set style name
            if( aInfo.usubfamily )
                o_rResult.SetStyleName( OUString( aInfo.usubfamily ) );
            else if( aInfo.subfamily )
                o_rResult.SetStyleName( OUString::createFromAscii( aInfo.subfamily ) );

            // cleanup
            CloseTTFont( pTTF );
            // success
            bResult = true;
        }
        return bResult;
    }

    struct WeightSearchEntry
    {
        const char* string;
        int         string_len;
        FontWeight  weight;

        bool operator<( const WeightSearchEntry& rRight ) const
        {
            return rtl_str_compareIgnoreAsciiCase_WithLength( string, string_len, rRight.string, rRight.string_len ) < 0;
        }
    }
    const weight_table[] =
    {
        { "black", 5, WEIGHT_BLACK },
        { "bold", 4, WEIGHT_BOLD },
        { "book", 4, WEIGHT_LIGHT },
        { "demi", 4, WEIGHT_SEMIBOLD },
        { "heavy", 5, WEIGHT_BLACK },
        { "light", 5, WEIGHT_LIGHT },
        { "medium", 6, WEIGHT_MEDIUM },
        { "regular", 7, WEIGHT_NORMAL },
        { "super", 5, WEIGHT_ULTRABOLD },
        { "thin", 4, WEIGHT_THIN }
    };

    bool identifyType1Font( const char* i_pBuffer, sal_uInt32 i_nSize, Font& o_rResult )
    {
        // might be a type1, find eexec
        const char* pStream = i_pBuffer;
        const char* const pExec = "eexec";
        const char* pExecPos = std::search( pStream, pStream+i_nSize, pExec, pExec+5 );
        if( pExecPos != pStream+i_nSize)
        {
            // find /FamilyName entry
            static const char* const pFam = "/FamilyName";
            const char* pFamPos = std::search( pStream, pExecPos, pFam, pFam+11 );
            if( pFamPos != pExecPos )
            {
                // extract the string value behind /FamilyName
                const char* pOpen = pFamPos+11;
                while( pOpen < pExecPos && *pOpen != '(' )
                    pOpen++;
                const char* pClose = pOpen;
                while( pClose < pExecPos && *pClose != ')' )
                    pClose++;
                if( pClose - pOpen > 1 )
                {
                    o_rResult.SetFamilyName( OStringToOUString( std::string_view( pOpen+1, pClose-pOpen-1 ), RTL_TEXTENCODING_ASCII_US ) );
                }
            }

            // parse /ItalicAngle
            static const char* const pItalic = "/ItalicAngle";
            const char* pItalicPos = std::search( pStream, pExecPos, pItalic, pItalic+12 );
            if( pItalicPos != pExecPos )
            {
                const char* pItalicEnd = pItalicPos + 12;
                auto nItalic = rtl_str_toInt64_WithLength(pItalicEnd, 10, pExecPos - pItalicEnd);
                o_rResult.SetItalic( (nItalic != 0) ? ITALIC_NORMAL : ITALIC_NONE );
            }

            // parse /Weight
            static const char* const pWeight = "/Weight";
            const char* pWeightPos = std::search( pStream, pExecPos, pWeight, pWeight+7 );
            if( pWeightPos != pExecPos )
            {
                // extract the string value behind /Weight
                const char* pOpen = pWeightPos+7;
                while( pOpen < pExecPos && *pOpen != '(' )
                    pOpen++;
                const char* pClose = pOpen;
                while( pClose < pExecPos && *pClose != ')' )
                    pClose++;
                if( pClose - pOpen > 1 )
                {
                    WeightSearchEntry aEnt;
                    aEnt.string = pOpen+1;
                    aEnt.string_len = (pClose-pOpen)-1;
                    aEnt.weight = WEIGHT_NORMAL;
                    WeightSearchEntry const * pFound = std::lower_bound( std::begin(weight_table), std::end(weight_table), aEnt );
                    if( pFound != std::end(weight_table) &&
                        rtl_str_compareIgnoreAsciiCase_WithLength( pFound->string, pFound->string_len, aEnt.string, aEnt.string_len) == 0 )
                        o_rResult.SetWeight( pFound->weight );
                }
            }

            // parse isFixedPitch
            static const char* const pFixed = "/isFixedPitch";
            const char* pFixedPos = std::search( pStream, pExecPos, pFixed, pFixed+13 );
            if( pFixedPos != pExecPos )
            {
                // skip whitespace
                while( pFixedPos < pExecPos-4 &&
                       ( *pFixedPos == ' '  ||
                         *pFixedPos == '\t' ||
                         *pFixedPos == '\r' ||
                         *pFixedPos == '\n' ) )
                {
                    pFixedPos++;
                }
                // find "true" value
                if( rtl_str_compareIgnoreAsciiCase_WithLength( pFixedPos, 4, "true", 4 ) == 0 )
                    o_rResult.SetPitch( PITCH_FIXED );
                else
                    o_rResult.SetPitch( PITCH_VARIABLE );
            }
        }
        return false;
    }
}

Font Font::identifyFont( const void* i_pBuffer, sal_uInt32 i_nSize )
{
    Font aResult;
    if( ! identifyTrueTypeFont( i_pBuffer, i_nSize, aResult ) )
    {
        const char* pStream = static_cast<const char*>(i_pBuffer);
        if( pStream && i_nSize > 100 &&
             *pStream == '%' && pStream[1] == '!' )
        {
            identifyType1Font( pStream, i_nSize, aResult );
        }
    }

    return aResult;
}

// The inlines from the font.hxx header are now instantiated for pImpl-ification
const Color& Font::GetColor() const { return mpFontAttributes->maColor; }
const Color& Font::GetFillColor() const { return mpFontAttributes->maFillColor; }
bool Font::IsTransparent() const { return mpFontAttributes->mbTransparent; }

FontAlign Font::GetAlignment() const { return mpFontAttributes->GetAlignment(); }

const OUString& Font::GetFamilyName() const { return mpFontAttributes->GetFamilyName(); }
const OUString& Font::GetStyleName() const { return mpFontAttributes->maStyleName; }

const Size& Font::GetFontSize() const { return mpFontAttributes->GetFontSize(); }
void Font::SetFontHeight( tools::Long nHeight ) { SetFontSize( Size( mpFontAttributes->GetFontSize().Width(), nHeight ) ); }
tools::Long Font::GetFontHeight() const { return mpFontAttributes->GetFontSize().Height(); }
void Font::SetAverageFontWidth( tools::Long nWidth ) { SetFontSize( Size( nWidth, mpFontAttributes->GetFontSize().Height() ) ); }
tools::Long Font::GetAverageFontWidth() const { return mpFontAttributes->GetFontSize().Width(); }

rtl_TextEncoding Font::GetCharSet() const { return mpFontAttributes->GetCharSet(); }

const LanguageTag& Font::GetLanguageTag() const { return mpFontAttributes->maLanguageTag; }
const LanguageTag& Font::GetCJKContextLanguageTag() const { return mpFontAttributes->maCJKLanguageTag; }
LanguageType Font::GetLanguage() const { return mpFontAttributes->maLanguageTag.getLanguageType( false); }
LanguageType Font::GetCJKContextLanguage() const { return mpFontAttributes->maCJKLanguageTag.getLanguageType( false); }

Degree10 Font::GetOrientation() const { return mpFontAttributes->mnOrientation; }
bool Font::IsVertical() const { return mpFontAttributes->mbVertical; }
FontKerning Font::GetKerning() const { return mpFontAttributes->meKerning; }

FontPitch Font::GetPitch() { return mpFontAttributes->GetPitch(); }
FontWeight Font::GetWeight() { return mpFontAttributes->GetWeight(); }
FontWidth Font::GetWidthType() { return mpFontAttributes->GetWidthType(); }
FontItalic Font::GetItalic() { return mpFontAttributes->GetItalic(); }
FontFamily Font::GetFamilyType() { return mpFontAttributes->GetFamilyType(); }

FontPitch Font::GetPitch() const { return mpFontAttributes->GetPitchNoAsk(); }
FontWeight Font::GetWeight() const { return mpFontAttributes->GetWeightNoAsk(); }
FontWidth Font::GetWidthType() const { return mpFontAttributes->GetWidthTypeNoAsk(); }
FontItalic Font::GetItalic() const { return mpFontAttributes->GetItalicNoAsk(); }
FontFamily Font::GetFamilyType() const { return mpFontAttributes->GetFamilyTypeNoAsk(); }

int Font::GetQuality() const { return mpFontAttributes->GetQuality(); }
void Font::SetQuality( int nQuality ) { mpFontAttributes->SetQuality( nQuality ); }
void Font::IncreaseQualityBy( int nQualityAmount ) { mpFontAttributes->IncreaseQualityBy( nQualityAmount ); }
void Font::DecreaseQualityBy( int nQualityAmount ) { mpFontAttributes->DecreaseQualityBy( nQualityAmount ); }

bool Font::IsOutline() const { return mpFontAttributes->mbOutline; }
bool Font::IsShadow() const { return mpFontAttributes->mbShadow; }
FontRelief Font::GetRelief() const { return mpFontAttributes->meRelief; }
FontLineStyle Font::GetUnderline() const { return mpFontAttributes->meUnderline; }
FontLineStyle Font::GetOverline()  const { return mpFontAttributes->meOverline; }
FontStrikeout Font::GetStrikeout() const { return mpFontAttributes->meStrikeout; }
FontEmphasisMark Font::GetEmphasisMark() const { return mpFontAttributes->meEmphasisMark; }
bool Font::IsWordLineMode() const { return mpFontAttributes->mbWordLine; }
bool Font::IsSameInstance( const vcl::Font& rFont ) const { return (mpFontAttributes == rFont.mpFontAttributes); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
