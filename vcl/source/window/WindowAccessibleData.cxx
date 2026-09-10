/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>
#include <vcl/toolkit/fixed.hxx>

#include <ImplAccessibleInfos.hxx>
#include <WindowAccessibleData.hxx>

#include <com/sun/star/accessibility/AccessibleRole.hpp>

#include <algorithm>

void WindowAccessibleData::add_mnemonic_label(FixedText* pLabel, vcl::Window* pWidget)
{
    if (std::find(m_aMnemonicLabels.begin(), m_aMnemonicLabels.end(), VclPtr<FixedText>(pLabel))
        != m_aMnemonicLabels.end())
    {
        return;
    }

    m_aMnemonicLabels.emplace_back(pLabel);
    pLabel->set_mnemonic_widget(pWidget);
}

void WindowAccessibleData::remove_mnemonic_label(FixedText* pLabel)
{
    auto aFind
        = std::find(m_aMnemonicLabels.begin(), m_aMnemonicLabels.end(), VclPtr<FixedText>(pLabel));
    if (aFind == m_aMnemonicLabels.end())
    {
        return;
    }

    m_aMnemonicLabels.erase(aFind);
    pLabel->set_mnemonic_widget(nullptr);
}

ImplAccessibleInfos& WindowAccessibleData::ensureAccessibleInfos()
{
    if (!mpAccessibleInfos)
        mpAccessibleInfos.reset(new ImplAccessibleInfos);

    return *mpAccessibleInfos;
}

void WindowAccessibleData::setAccessibleRole(sal_uInt16 nRole)
{
    ensureAccessibleInfos().nAccessibleRole = nRole;
}

sal_uInt16 WindowAccessibleData::getAccessibleRole() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->nAccessibleRole
                             : css::accessibility::AccessibleRole::UNKNOWN;
}

void WindowAccessibleData::setAccessibleName(const OUString& rName)
{
    ensureAccessibleInfos().pAccessibleName = rName;
}

std::optional<OUString> WindowAccessibleData::getAccessibleName() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleName : std::nullopt;
}

void WindowAccessibleData::setAccessibleDescription(const OUString& rDescription)
{
    ensureAccessibleInfos().pAccessibleDescription = rDescription;
}

std::optional<OUString> WindowAccessibleData::getAccessibleDescription() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleDescription : std::nullopt;
}

void WindowAccessibleData::setAccessibleRelationLabeledBy(vcl::Window* pWindow)
{
    ensureAccessibleInfos().pLabeledByWindow = pWindow;
}

vcl::Window* WindowAccessibleData::getAccessibleRelationLabeledBy() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pLabeledByWindow : nullptr;
}

void WindowAccessibleData::setAccessibleRelationLabelFor(vcl::Window* pWindow)
{
    ensureAccessibleInfos().pLabelForWindow = pWindow;
}

vcl::Window* WindowAccessibleData::getAccessibleRelationLabelFor() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pLabelForWindow : nullptr;
}

void WindowAccessibleData::setAccessibleParent(
    const rtl::Reference<comphelper::OAccessible>& rpParent)
{
    ensureAccessibleInfos().pAccessibleParent = rpParent;
}

rtl::Reference<comphelper::OAccessible> WindowAccessibleData::getAccessibleParent() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleParent : nullptr;
}

const OUString* WindowAccessibleData::getAccessibleDescriptionPtr() const
{
    if (mpAccessibleInfos && mpAccessibleInfos->pAccessibleDescription)
        return &(*mpAccessibleInfos->pAccessibleDescription);

    return nullptr;
}

void WindowAccessibleData::dispose()
{
    if (mpAccessible.is())
    {
        mpAccessible->dispose();
        mpAccessible.clear();
    }

    if (mpAccessibleInfos)
        mpAccessibleInfos->pAccessibleParent.clear();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
