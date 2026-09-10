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
#include <vcl/toolkit/unowrap.hxx>

#include <ImplAccessibleInfos.hxx>
#include <WindowA11y.hxx>

#include <com/sun/star/accessibility/AccessibleRole.hpp>

#include <algorithm>

void WindowA11y::add_mnemonic_label(FixedText* pLabel, vcl::Window* pWidget)
{
    if (std::find(m_aMnemonicLabels.begin(), m_aMnemonicLabels.end(), VclPtr<FixedText>(pLabel))
        != m_aMnemonicLabels.end())
    {
        return;
    }

    m_aMnemonicLabels.emplace_back(pLabel);
    pLabel->set_mnemonic_widget(pWidget);
}

void WindowA11y::remove_mnemonic_label(FixedText* pLabel)
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

ImplAccessibleInfos& WindowA11y::ensureAccessibleInfos()
{
    if (!mpAccessibleInfos)
        mpAccessibleInfos.reset(new ImplAccessibleInfos);

    return *mpAccessibleInfos;
}

void WindowA11y::setAccessibleRole(sal_uInt16 nRole)
{
    ensureAccessibleInfos().nAccessibleRole = nRole;
}

sal_uInt16 WindowA11y::getAccessibleRole() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->nAccessibleRole
                             : css::accessibility::AccessibleRole::UNKNOWN;
}

void WindowA11y::setAccessibleName(const OUString& rName)
{
    ensureAccessibleInfos().pAccessibleName = rName;
}

std::optional<OUString> WindowA11y::getAccessibleName() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleName : std::nullopt;
}

void WindowA11y::setAccessibleDescription(const OUString& rDescription)
{
    ensureAccessibleInfos().pAccessibleDescription = rDescription;
}

std::optional<OUString> WindowA11y::getAccessibleDescription() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleDescription : std::nullopt;
}

void WindowA11y::setAccessibleRelationLabeledBy(vcl::Window* pWindow)
{
    ensureAccessibleInfos().pLabeledByWindow = pWindow;
}

vcl::Window* WindowA11y::getAccessibleRelationLabeledBy() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pLabeledByWindow : nullptr;
}

void WindowA11y::setAccessibleRelationLabelFor(vcl::Window* pWindow)
{
    ensureAccessibleInfos().pLabelForWindow = pWindow;
}

vcl::Window* WindowA11y::getAccessibleRelationLabelFor() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pLabelForWindow : nullptr;
}

void WindowA11y::setAccessibleParent(const rtl::Reference<comphelper::OAccessible>& rpParent)
{
    ensureAccessibleInfos().pAccessibleParent = rpParent;
}

rtl::Reference<comphelper::OAccessible> WindowA11y::getAccessibleParent() const
{
    return mpAccessibleInfos ? mpAccessibleInfos->pAccessibleParent : nullptr;
}

const OUString* WindowA11y::getAccessibleDescriptionPtr() const
{
    if (mpAccessibleInfos && mpAccessibleInfos->pAccessibleDescription)
        return &(*mpAccessibleInfos->pAccessibleDescription);

    return nullptr;
}

void WindowA11y::dispose()
{
    if (hasAccessible())
    {
        mpAccessible->dispose();
        clearAccessible();
    }

    if (mpAccessibleInfos)
        mpAccessibleInfos->pAccessibleParent.clear();
}

void WindowA11y::setWindowPeer(const css::uno::Reference<css::awt::XVclWindowPeer>& xPeer,
                               VCLXWindow* pVCLXWindow)
{
    // be safe against re-entrance: first clear the old ref, then assign the new one
    if (mxWindowPeer)
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        SAL_WARN_IF(!pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!");
        if (pWrapper)
            pWrapper->SetWindowInterface(nullptr, mxWindowPeer);
        mxWindowPeer->dispose();
        mxWindowPeer.clear();
    }

    mxWindowPeer = xPeer;
    mpVCLXWindow = pVCLXWindow;
}

css::uno::Reference<css::awt::XVclWindowPeer> WindowA11y::getWindowPeer(bool bCreate,
                                                                        vcl::Window* pWindow)
{
    if (!mxWindowPeer.is() && bCreate)
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        if (pWrapper)
            mxWindowPeer = pWrapper->GetWindowInterface(pWindow);
    }
    return mxWindowPeer;
}

void WindowA11y::disposeWindowPeer()
{
    if (mxWindowPeer)
    {
        mxWindowPeer->dispose();
        mxWindowPeer.clear();
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
