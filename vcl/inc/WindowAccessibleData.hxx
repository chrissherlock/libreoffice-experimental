/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <comphelper/OAccessible.hxx>

#include <vcl/vclptr.hxx>

#include <vector>
#include <memory>

#include <com/sun/star/awt/XVclWindowPeer.hpp>

class VCLXWindow;
class FixedText;
struct ImplAccessibleInfos;

struct WindowAccessibleData
{
    css::uno::Reference<css::awt::XVclWindowPeer> mxWindowPeer;
    rtl::Reference<comphelper::OAccessible> mpAccessible;
    std::unique_ptr<ImplAccessibleInfos> mpAccessibleInfos;
    std::vector<VclPtr<FixedText>> m_aMnemonicLabels;

    VCLXWindow* mpVCLXWindow = nullptr;

    bool mbSuppressAccessibilityEvents = false;

    WindowAccessibleData() = default;
    ~WindowAccessibleData() = default;

    void add_mnemonic_label(FixedText* pLabel, vcl::Window* pWidget);
    void remove_mnemonic_label(FixedText* pLabel);
    const std::vector<VclPtr<FixedText>>& list_mnemonic_labels() const { return m_aMnemonicLabels; }

    void suspendEvents() { mbSuppressAccessibilityEvents = true; }
    void resumeEvents() { mbSuppressAccessibilityEvents = false; }
    bool isEventsSuspended() const { return mbSuppressAccessibilityEvents; }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
