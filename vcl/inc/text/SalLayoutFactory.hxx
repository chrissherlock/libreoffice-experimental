/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once
#include <text/ILayoutFactory.hxx>
#include <salgdi.hxx>
#include <functional>

namespace vcl::text
{
class SalLayoutFactory : public ILayoutFactory
{
    std::function<SalGraphics*()> m_fnGetGraphics;

public:
    explicit SalLayoutFactory(std::function<SalGraphics*()> fn)
        : m_fnGetGraphics(std::move(fn))
    {
    }

    std::unique_ptr<SalLayout> CreateLayout(LogicalFontInstance* pFont, int nFallbackLevel) override
    {
        SalGraphics* pGraphics = m_fnGetGraphics();
        if (pGraphics)
        {
            pGraphics->SetFont(pFont, nFallbackLevel);
            return pGraphics->GetTextLayout(nFallbackLevel);
        }
        return nullptr;
    }
};

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
