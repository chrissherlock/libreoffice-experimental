/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ref.hxx>
#include <tools/color.hxx>

#include <vcl/dllapi.h>
#include <vcl/fontcharmap.hxx>
#include <vcl/metric.hxx>
#include <vcl/vclenum.hxx>

#include <tuple>
#include <impfontcache.hxx>

class ImplFontCache;
class PhysicalFontCollection;
class LogicalFontInstance;
class SalGraphics;

namespace vcl::font
{
class Font;

struct FontRealization
{
    rtl::Reference<LogicalFontInstance> mxFont;

    tools::Long nXOffset = 0;
    tools::Long nYOffset = 0;

    tools::Long nEmphasisAscent = 0;
    tools::Long nEmphasisDescent = 0;

    bool bHasLineDecorations = false;
    bool bHasSpecialEffects = false;

    vcl::text::ComplexTextLayoutFlags eLayoutMode = vcl::text::ComplexTextLayoutFlags::Default;
};

class VCL_DLLPUBLIC FontController
{
public:
    void SetFontCollection(const std::shared_ptr<PhysicalFontCollection>& pPFC)
    {
        mxFontCollection = pPFC;
    }

    PhysicalFontCollection* GetFontCollection() const { return mxFontCollection.get(); }

    const std::shared_ptr<PhysicalFontCollection>& GetSharedFontCollection() const
    {
        return mxFontCollection;
    }

    sal_uInt32 GetFontFaceCollectionCount() const;

    bool IsFontAvailable(std::u16string_view rFontName) const;

    void AdoptCache(const std::shared_ptr<ImplFontCache>& pShared) { mxFontCache = pShared; }
    void ClearCache() { mxFontCache.reset(); }
    void InvalidateCache();

    void ResetCache() { mxFontCache = std::make_shared<ImplFontCache>(); }
    ImplFontCache& GetCache() const { return *mxFontCache; }

    rtl::Reference<LogicalFontInstance> mxFontInstance;
    std::shared_ptr<PhysicalFontCollection> mxFontCollection;
    std::shared_ptr<ImplFontCache> mxFontCache;

    TextAlign meTextAlign;

    FontController();

    void InitializeFonts(SalGraphics* pGraphics);

    bool NeedsUpdate(const vcl::Font& rRequestedFont, bool bDeviceDirty) const;

    rtl::Reference<LogicalFontInstance> RealizeFont(vcl::font::PhysicalFontCollection* pColl,
                                                    const vcl::Font& rFont,
                                                    const SalGraphics* pGraphics, const Size& rSize,
                                                    float fExactHeight, bool bNonAntialiased);

    void InitializeInstance(LogicalFontInstance* pFontInstance, SalGraphics* pGraphics);

    /**
     * Calculates vertical/horizontal offsets and emphasis metrics.
     * Returns a tuple of: <nHorzOffset, nVertOffset, nEmphasisAscent, nEmphasisDescent>
     */
    std::tuple<tools::Long, tools::Long, tools::Long, tools::Long>
    CalculateTextOffsets(const vcl::Font& rFont, const LogicalFontInstance* pFontInstance) const;

    std::pair<float, Size> CalculateDeviceSize(const vcl::Font& rFont,
                                               const CoordinateMapper& rMapper,
                                               tools::Long nDPIY) const;

    /**
     * Evaluates if the font requires special rendering for lines or effects.
     * Returns a tuple of: <bTextLines, bTextSpecial>
     */
    std::tuple<bool, bool> GetTextLayoutFlags(const vcl::Font& rFont) const;

    bool ShouldDisableAntialiasing(AntialiasingFlags eAntialisingFlags,
                                   const StyleSettings& rStyleSettings, tools::Long nHeight) const;

    /**
     * Calculates a stretched width for OLE font scaling fixes.
     * Returns 0 if no fix is required or possible.
     */
    int CalculateOLEStorageWidth(const LogicalFontInstance* pFontInstance, tools::Long nMapXNum,
                                 tools::Long nMapXDen, tools::Long nMapYNum,
                                 tools::Long nMapYDen) const;

    void PopulateFontMetric(FontMetric& rMetric, const vcl::Font& rLogicalFont,
                            const LogicalFontInstance* pFontInstance, tools::Long nEmphasisAscent,
                            tools::Long nEmphasisDescent) const;

    double GetMinKashidaWidth(const LogicalFontInstance* pFontInstance) const;

    bool GetFontCapabilities(SalGraphics* pGraphics,
                             vcl::FontCapabilities& rFontCapabilities) const;

    FontCharMapRef GetFontCharMap(SalGraphics* pGraphics) const;

    void GetFontFeatures(const LogicalFontInstance* pFontInstance,
                         std::vector<vcl::font::Feature>& rFontFeatures) const;

    bool GetFontSubstitution(const SalGraphics* pGraphics, const vcl::Font& rFont,
                             OUString& rMissingCodes, vcl::Font& rSubstFont) const;

    bool NeedsOLEFontScaleFix(const CoordinateMapper& rMapper, const Size& rSize) const;
    Size GetOLECorrectedSize(const CoordinateMapper& rMapper, const Size& rSize,
                             tools::Long nHeight) const;

    bool AddTempDevFont(SalGraphics* pGraphics, const OUString& rFileURL,
                        const OUString& rFontName);
    bool RemoveTempDevFont(SalGraphics* pGraphics, const OUString& rFileURL,
                           const OUString& rFontName);

    bool ActivateFontOnDevice(SalGraphics* pGraphics, LogicalFontInstance* pFontInstance);

    FontMetric GetFontMetricFromCollection(SalGraphics* pGraphics, size_t nDevFontIndex) const;
    void RefreshFromGraphics(SalGraphics* pGraphics);
    void ClearFontResources(SalGraphics* pGraphics, bool bNewFontLists);
    void UpdateFontData(SalGraphics* pGraphics, bool bNewFontLists);

    // Global Font Substitution
    static void BeginFontSubstitution();
    static void EndFontSubstitution();
    static void AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName,
                                  AddFontSubstituteFlags nFlags);
    static void RemoveFontsSubstitute();
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
