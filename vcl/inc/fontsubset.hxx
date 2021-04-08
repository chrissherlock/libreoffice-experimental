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

#ifndef INCLUDED_VCL_INC_FONTSUBSET_HXX
#define INCLUDED_VCL_INC_FONTSUBSET_HXX

#include <rtl/ustring.hxx>
#include <tools/gen.hxx>

#include <vcl/dllapi.h>
#include <vcl/glyphitem.hxx>

#include <font/FontType.hxx>

namespace vcl { class TrueTypeFont; } ///< SFT's idea of a TTF font

class VCL_DLLPUBLIC FontSubsetInfo final
{
public:
    explicit    FontSubsetInfo();
                ~FontSubsetInfo();

    void        LoadFont( FontType eInFontType,
                    const unsigned char* pFontBytes, int nByteLength );
    void        LoadFont( vcl::TrueTypeFont* pSftTrueTypeFont );

    bool        CreateFontSubset( FontType nOutFontTypeMask,
                    FILE* pOutFile, const char* pOutFontName,
                    const sal_GlyphId* pGlyphIds, const sal_uInt8* pEncodedIds,
                    int nReqGlyphCount, sal_Int32* pOutGlyphWidths = nullptr );

public: // TODO: make subsetter results private and provide accessor methods instead
        // subsetter-provided subset details needed by e.g. Postscript or PDF
    OUString                m_aPSName;
    int                     m_nAscent;          ///< all metrics in PS font units
    int                     m_nDescent;
    int                     m_nCapHeight;
    tools::Rectangle               m_aFontBBox;
    FontType                m_nFontType;        ///< font-type of subset result

private:
    // input-font-specific details
    unsigned const char*    mpInFontBytes;
    int                     mnInByteLength;
    FontType                meInFontType;       ///< allowed mask of input font-types
    vcl::TrueTypeFont*      mpSftTTFont;

    // subset-request details
    FontType                mnReqFontTypeMask;  ///< allowed subset-target font types
    FILE*                   mpOutFile;
    const char*             mpReqFontName;
    const sal_GlyphId*      mpReqGlyphIds;
    const sal_uInt8*        mpReqEncodedIds;
    int                     mnReqGlyphCount;

    bool    CreateFontSubsetFromCff( sal_Int32* pOutGlyphWidths );
    bool    CreateFontSubsetFromSfnt( sal_Int32* pOutGlyphWidths );
    static bool CreateFontSubsetFromType1( const sal_Int32* pOutGlyphWidths );
};

#endif // INCLUDED_VCL_INC_FONTSUBSET_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
