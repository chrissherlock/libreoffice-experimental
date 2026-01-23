/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <text/TextLayoutEngine.hxx>
#include <salgdi.hxx>
#include <functional>
#include <memory>

namespace vcl::text
{
class GraphicLayoutFactory : public ILayoutFactory
{
    std::function<SalGraphics*()> m_fnGetGraphics;

public:
    explicit GraphicLayoutFactory(std::function<SalGraphics*()> fnGetGraphics)
        : m_fnGetGraphics(std::move(fnGetGraphics))
    {
    }

    std::unique_ptr<SalLayout> CreateLayout(int nFallbackLevel) override
    {
        SalGraphics* pGraphics = m_fnGetGraphics();
        if (!pGraphics)
            return nullptr;
        return pGraphics->GetTextLayout(nFallbackLevel);
    }

    void SetFont(LogicalFontInstance* pFont, int nFallbackLevel) override
    {
        SalGraphics* pGraphics = m_fnGetGraphics();
        if (pGraphics)
            pGraphics->SetFont(pFont, nFallbackLevel);
    }
};

} // namespace vcl::text
