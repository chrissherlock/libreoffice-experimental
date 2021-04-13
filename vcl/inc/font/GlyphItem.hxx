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

#pragma once

#include <tools/gen.hxx>

#include <vcl/dllapi.h>
#include <vcl/glyphitem.hxx>
#include <vcl/outdev.hxx>

#include <font/GlyphItemFlags.hxx>
#include <font/FontInstance.hxx>
#include <font/SalLayoutGlyphsImpl.hxx>

#include <vector>

class VCL_DLLPUBLIC GlyphItem
{
public:
    GlyphItem(int nCharPos, int nCharCount, sal_GlyphId aGlyphId, const Point& rLinearPos,
              GlyphItemFlags nFlags, int nOrigWidth, int nXOffset, FontInstance* pFontInstance)
        : m_nNewWidth(nOrigWidth)
        , m_aLinearPos(rLinearPos)
        , m_aGlyphId(aGlyphId)
        , m_nCharCount(nCharCount)
        , m_nOrigWidth(nOrigWidth)
        , m_pFontInstance(pFontInstance)
        , m_nCharPos(nCharPos)
        , m_nFlags(nFlags)
        , m_nXOffset(nXOffset)
    {
        assert(m_pFontInstance);
    }

    bool IsInCluster() const { return bool(m_nFlags & GlyphItemFlags::IS_IN_CLUSTER); }
    bool IsRTLGlyph() const { return bool(m_nFlags & GlyphItemFlags::IS_RTL_GLYPH); }
    bool IsDiacritic() const { return bool(m_nFlags & GlyphItemFlags::IS_DIACRITIC); }
    bool IsVertical() const { return bool(m_nFlags & GlyphItemFlags::IS_VERTICAL); }
    bool IsSpacing() const { return bool(m_nFlags & GlyphItemFlags::IS_SPACING); }
    bool AllowKashida() const { return bool(m_nFlags & GlyphItemFlags::ALLOW_KASHIDA); }
    bool IsDropped() const { return bool(m_nFlags & GlyphItemFlags::IS_DROPPED); }
    bool IsClusterStart() const { return bool(m_nFlags & GlyphItemFlags::IS_CLUSTER_START); }

    inline bool GetGlyphBoundRect(tools::Rectangle&) const;
    inline bool GetGlyphOutline(basegfx::B2DPolyPolygon&) const;
    inline void dropGlyph();

    sal_GlyphId glyphId() const { return m_aGlyphId; }
    int charCount() const { return m_nCharCount; }
    int origWidth() const { return m_nOrigWidth; }
    int charPos() const { return m_nCharPos; }
    int xOffset() const { return m_nXOffset; }

    int m_nNewWidth; // width after adjustments
    Point m_aLinearPos; // absolute position of non rotated string

private:
    sal_GlyphId m_aGlyphId;
    int m_nCharCount; ///< number of characters making up this glyph
    int m_nOrigWidth; ///< original glyph width
    FontInstance* m_pFontInstance;
    int m_nCharPos; ///< index in string
    GlyphItemFlags m_nFlags;
    int m_nXOffset;
};

VCL_DLLPUBLIC bool GlyphItem::GetGlyphBoundRect(tools::Rectangle& rRect) const
{
    return m_pFontInstance->GetGlyphBoundRect(m_aGlyphId, rRect, IsVertical());
}

VCL_DLLPUBLIC bool GlyphItem::GetGlyphOutline(basegfx::B2DPolyPolygon& rPoly) const
{
    return m_pFontInstance->GetGlyphOutline(m_aGlyphId, rPoly, IsVertical());
}

void GlyphItem::dropGlyph()
{
    m_nCharPos = -1;
    m_nFlags |= GlyphItemFlags::IS_DROPPED;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
