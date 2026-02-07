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

#include <vcl/dllapi.h>
#include <vcl/text/IFontFallbackStrategy.hxx>

class ImplLayoutRuns;
class LogicalFontInstance;
class ImplFontCache;
class SalLayoutGlyphsImpl;
class MultiSalLayout;

namespace vcl::font
{
class PhysicalFontCollection;
}

namespace vcl::text
{
class ILayoutFactory;
struct FontLookupCriteria;

class VCL_DLLPUBLIC DefaultFallbackStrategy : public IFontFallbackStrategy
{
public:
    virtual std::unique_ptr<SalLayout> ResolveFallbacks(const LayoutResources& rRes,
                                                        std::unique_ptr<SalLayout> pLayout,
                                                        vcl::text::TextLayoutRequest& rArgs,
                                                        const SalLayoutGlyphs* pGlyphs) override;

private:
    // Helper methods moved from TextLayoutEngine
    static std::unique_ptr<SalLayout> ResolveMissingGlyphs(
        std::unique_ptr<SalLayout> pBaseLayout, vcl::text::TextLayoutRequest& rLayoutArgs,
        const SalLayoutGlyphs* pGlyphs, const FontLookupCriteria& rCriteria,
        ILayoutFactory* pFactory); // Note: Signature might need adjustment to match extracted code

    static void MergeFallback(std::unique_ptr<MultiSalLayout>& rMultiSalLayout,
                              std::unique_ptr<SalLayout>& rBaseLayout,
                              std::unique_ptr<SalLayout> pFallback, const ImplLayoutRuns& rRuns,
                              bool bIsLastLevel);

    static OUString IdentifyMissingChars(vcl::text::TextLayoutRequest& rArgs);

    static rtl::Reference<LogicalFontInstance> FindFallbackFont(const FontLookupCriteria& rCriteria,
                                                                int nFallbackLevel,
                                                                OUString& rMissingCodes,
                                                                bool& rHasUsedFallback,
                                                                SalLayoutGlyphsImpl* pGlyphsImpl);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
