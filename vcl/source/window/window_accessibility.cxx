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

#include <vcl/layout.hxx>
#include <vcl/menu.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/wrkwin.hxx>

#include <ImplAccessibleInfos.hxx>
#include <WindowA11y.hxx>
#include <WindowClassification.hxx>
#include <WindowHierarchy.hxx>
#include <accessibility/floatingwindowaccessible.hxx>
#include <accessibility/vclxaccessiblefixedtext.hxx>
#include <accessibility/vclxaccessiblestatusbar.hxx>
#include <accessibility/vclxaccessibletabcontrol.hxx>
#include <accessibility/vclxaccessibletabpagewindow.hxx>
#include <brdwin.hxx>

#include <com/sun/star/accessibility/AccessibleRole.hpp>

namespace vcl
{
rtl::Reference<comphelper::OAccessible> Window::GetAccessible(bool bCreate)
{
    if (!mpAccessibleData)
        return {};

    if (!mpAccessibleData->hasAccessible() && !mpClassification->mbInDispose && bCreate)
        mpAccessibleData->setAccessible(CreateAccessible());

    return mpAccessibleData->getAccessible();
}

static bool lcl_hasFloatingChild(vcl::Window* pWindow)
{
    vcl::Window* pChild = pWindow->GetAccessibleChildWindow(0);
    return pChild && pChild->GetType() == WindowType::FLOATINGWINDOW;
}

rtl::Reference<comphelper::OAccessible> Window::CreateAccessible()
{
    const WindowType eType = GetType();

    if (eType == WindowType::STATUSBAR)
        return new VCLXAccessibleStatusBar(this);

    if (eType == WindowType::TABCONTROL)
        return new VCLXAccessibleTabControl(this);

    if (eType == WindowType::TABPAGE && GetAccessibleParentWindow()
        && GetAccessibleParentWindow()->GetType() == WindowType::TABCONTROL)
        return new VCLXAccessibleTabPageWindow(this);

    if (eType == WindowType::FLOATINGWINDOW)
        return new FloatingWindowAccessible(this);

    if (eType == WindowType::BORDERWINDOW && lcl_hasFloatingChild(this))
        return new FloatingWindowAccessible(this);

    if ((eType == WindowType::HELPTEXTWINDOW) || (eType == WindowType::FIXEDLINE))
        return new VCLXAccessibleFixedText(this);

    return new VCLXAccessibleComponent(this);
}

void Window::SetAccessible(const rtl::Reference<comphelper::OAccessible>& rpAccessible)
{
    if (mpAccessibleData)
        mpAccessibleData->setAccessible(rpAccessible);
}

// skip all border windows that are not top level frames
bool Window::ImplIsAccessibleCandidate() const
{
    if (!mpClassification->mbBorderWin)
        return true;

    return IsNativeFrame();
}

vcl::Window* Window::GetAccessibleParentWindow() const
{
    if (!mpHierarchy || IsNativeFrame())
        return nullptr;

    if (IsTopWindow())
    {
        // if "top-level" has native border window parent, report it;
        // but don't report parent otherwise (which could e.g. be
        // a dialog's parent window that's otherwise a separate window and
        // doesn't consider the top level its a11y child either)
        if (mpHierarchy->mpBorderWindow && mpHierarchy->mpBorderWindow->IsNativeFrame())
            return mpHierarchy->mpBorderWindow;

        return nullptr;
    }

    if (GetType() == WindowType::MENUBARWINDOW)
    {
        vcl::Window* pParent = mpHierarchy->mpParent;

        // report the menubar as a child of THE workwindow
        if (vcl::Window* pRealParent = GetParent())
        {
            vcl::Window* pWorkWin = pRealParent->GetWindow(GetWindowType::FirstChild);

            while (pWorkWin && (pWorkWin == this))
            {
                pWorkWin = pWorkWin->GetWindow(GetWindowType::Next);
            }

            if (pWorkWin)
                pParent = pWorkWin;
        }

        return pParent;
    }

    // If this is a floating window which has a native border window, then that border should be reported as
    // the accessible parent
    if (GetType() == WindowType::FLOATINGWINDOW && mpHierarchy->mpBorderWindow
        && mpHierarchy->mpBorderWindow->ImplGetWindowClassification()->mbFrame)
    {
        return mpHierarchy->mpBorderWindow;
    }

    if (vcl::Window* pParent = mpHierarchy->mpParent;
        pParent && !pParent->ImplIsAccessibleCandidate())
        return pParent->ImplGetParent();

    return mpHierarchy->mpParent;
}

sal_uInt16 Window::GetAccessibleChildWindowCount()
{
    if (!mpClassification)
        return 0;

    sal_uInt16 nChildren = 0;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        if (pChild->IsVisible())
            nChildren++;

        pChild = pChild->mpHierarchy->mpNext;
    }

    // report the menubarwindow as a child of THE workwindow
    if (GetType() == WindowType::BORDERWINDOW)
    {
        ImplBorderWindow* pBorderWindow = static_cast<ImplBorderWindow*>(this);
        if (pBorderWindow->mpMenuBarWindow && pBorderWindow->mpMenuBarWindow->IsVisible())
            --nChildren;

        return nChildren;
    }

    if (GetType() != WindowType::WORKWINDOW)
        return nChildren;

    WorkWindow* pWorkWindow = static_cast<WorkWindow*>(this);

    if (pWorkWindow->GetMenuBar() && pWorkWindow->GetMenuBar()->GetWindow()
        && pWorkWindow->GetMenuBar()->GetWindow()->IsVisible())
    {
        ++nChildren;
    }

    return nChildren;
}

vcl::Window* Window::GetAccessibleChildWindow(sal_uInt16 n)
{
    // report the menubarwindow as the first child of THE workwindow
    if (GetType() == WindowType::WORKWINDOW && static_cast<WorkWindow*>(this)->GetMenuBar())
    {
        if (n == 0)
        {
            MenuBar* pMenuBar = static_cast<WorkWindow*>(this)->GetMenuBar();

            if (pMenuBar->GetWindow() && pMenuBar->GetWindow()->IsVisible())
                return pMenuBar->GetWindow();
        }
        else
        {
            --n;
        }
    }

    // transform n to child number including invisible children
    sal_uInt16 nChildren = n;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        if (pChild->IsVisible())
        {
            if (!nChildren)
                break;

            nChildren--;
        }

        pChild = pChild->mpHierarchy->mpNext;
    }

    if (GetType() == WindowType::BORDERWINDOW && pChild
        && pChild->GetType() == WindowType::MENUBARWINDOW)
    {
        do
        {
            pChild = pChild->mpHierarchy->mpNext;
        } while (pChild && !pChild->IsVisible());

        SAL_WARN_IF(!pChild, "vcl", "GetAccessibleChildWindow(): wrong index in border window");
    }

    if (pChild && (pChild->GetType() == WindowType::BORDERWINDOW) && (pChild->GetChildCount() == 1))
        pChild = pChild->GetChild(0);

    return pChild;
}

void Window::SetAccessibleParent(const rtl::Reference<comphelper::OAccessible>& rpParent)
{
    if (mpAccessibleData)
        mpAccessibleData->setAccessibleParent(rpParent);
}

rtl::Reference<comphelper::OAccessible> Window::GetAccessibleParent() const
{
    if (!mpAccessibleData)
        return nullptr;

    if (auto pParent = mpAccessibleData->getAccessibleParent())
        return pParent;

    if (vcl::Window* pAccessibleParentWin = GetAccessibleParentWindow())
        return pAccessibleParentWin->GetAccessible();

    return nullptr;
}

void Window::SetAccessibleRole(sal_uInt16 nRole)
{
    if (!mpAccessibleData)
        return;

    SAL_WARN_IF(mpAccessibleData->getAccessibleRole()
                    != css::accessibility::AccessibleRole::UNKNOWN,
                "vcl", "AccessibleRole already set!");

    mpAccessibleData->setAccessibleRole(nRole);
}

static bool lcl_actsAsAccessibleFrame(const vcl::Window* pWindow)
{
    const vcl::Window* pBorder = pWindow->ImplGetWindowHierarchy()->mpBorderWindow;
    bool bBorderIsFrame = pBorder && pBorder->ImplGetWindowClassification()->mbFrame;

    return pWindow->ImplGetWindowClassification()->mbFrame || bBorderIsFrame
           || (pWindow->GetStyle() & WB_OWNERDRAWDECORATION);
}

sal_uInt16 Window::getDefaultAccessibleRole() const
{
    switch (GetType())
    {
        case WindowType::MESSBOX:
        case WindowType::INFOBOX:
        case WindowType::WARNINGBOX:
        case WindowType::ERRORBOX:
        case WindowType::QUERYBOX:
            return css::accessibility::AccessibleRole::ALERT;

        case WindowType::MODELESSDIALOG:
        case WindowType::TABDIALOG:
        case WindowType::BUTTONDIALOG:
        case WindowType::DIALOG:
            return css::accessibility::AccessibleRole::DIALOG;

        case WindowType::PUSHBUTTON:
        case WindowType::OKBUTTON:
        case WindowType::CANCELBUTTON:
        case WindowType::HELPBUTTON:
        case WindowType::IMAGEBUTTON:
        case WindowType::MOREBUTTON:
            return css::accessibility::AccessibleRole::PUSH_BUTTON;

        case WindowType::MENUBUTTON:
            return css::accessibility::AccessibleRole::BUTTON_MENU;

        case WindowType::RADIOBUTTON:
            return css::accessibility::AccessibleRole::RADIO_BUTTON;

        case WindowType::TRISTATEBOX:
        case WindowType::CHECKBOX:
            return css::accessibility::AccessibleRole::CHECK_BOX;

        case WindowType::MULTILINEEDIT:
            return css::accessibility::AccessibleRole::SCROLL_PANE;

        case WindowType::PATTERNFIELD:
        case WindowType::EDIT:
            return static_cast<Edit const*>(this)->IsPassword()
                       ? css::accessibility::AccessibleRole::PASSWORD_TEXT
                       : css::accessibility::AccessibleRole::TEXT;

        case WindowType::PATTERNBOX:
        case WindowType::NUMERICBOX:
        case WindowType::METRICBOX:
        case WindowType::CURRENCYBOX:
        case WindowType::LONGCURRENCYBOX:
        case WindowType::COMBOBOX:
            return css::accessibility::AccessibleRole::COMBO_BOX;

        case WindowType::LISTBOX:
        case WindowType::MULTILISTBOX:
            return css::accessibility::AccessibleRole::LIST;

        case WindowType::TREELISTBOX:
            return css::accessibility::AccessibleRole::TREE;

        case WindowType::FIXEDTEXT:
            return css::accessibility::AccessibleRole::LABEL;

        case WindowType::FIXEDLINE:
            if (!GetText().isEmpty())
                return css::accessibility::AccessibleRole::LABEL;

            return css::accessibility::AccessibleRole::SEPARATOR;

        case WindowType::FIXEDBITMAP:
        case WindowType::FIXEDIMAGE:
            return css::accessibility::AccessibleRole::ICON;

        case WindowType::GROUPBOX:
            return css::accessibility::AccessibleRole::GROUP_BOX;

        case WindowType::SCROLLBAR:
            return css::accessibility::AccessibleRole::SCROLL_BAR;

        case WindowType::SLIDER:
        case WindowType::SPLITTER:
        case WindowType::SPLITWINDOW:
            return css::accessibility::AccessibleRole::SPLIT_PANE;

        case WindowType::DATEBOX:
        case WindowType::TIMEBOX:
        case WindowType::DATEFIELD:
        case WindowType::TIMEFIELD:
            return css::accessibility::AccessibleRole::DATE_EDITOR;

        case WindowType::METRICFIELD:
        case WindowType::CURRENCYFIELD:
        case WindowType::SPINBUTTON:
        case WindowType::SPINFIELD:
        case WindowType::FORMATTEDFIELD:
            return css::accessibility::AccessibleRole::SPIN_BOX;

        case WindowType::TOOLBOX:
            return css::accessibility::AccessibleRole::TOOL_BAR;

        case WindowType::STATUSBAR:
            return css::accessibility::AccessibleRole::STATUS_BAR;

        case WindowType::TABPAGE:
            return css::accessibility::AccessibleRole::PANEL;

        case WindowType::TABCONTROL:
            return css::accessibility::AccessibleRole::PAGE_TAB_LIST;

        case WindowType::DOCKINGWINDOW:
            return (mpClassification->mbFrame) ? css::accessibility::AccessibleRole::FRAME
                                               : css::accessibility::AccessibleRole::PANEL;

        case WindowType::FLOATINGWINDOW:
            return lcl_actsAsAccessibleFrame(this) ? css::accessibility::AccessibleRole::FRAME
                                                   : css::accessibility::AccessibleRole::WINDOW;

        case WindowType::WORKWINDOW:
            return css::accessibility::AccessibleRole::ROOT_PANE;

        case WindowType::SCROLLBARBOX:
            return css::accessibility::AccessibleRole::FILLER;

        case WindowType::HELPTEXTWINDOW:
            return css::accessibility::AccessibleRole::TOOL_TIP;

        case WindowType::PROGRESSBAR:
            return css::accessibility::AccessibleRole::PROGRESS_BAR;

        case WindowType::RULER:
            return css::accessibility::AccessibleRole::RULER;

        case WindowType::SCROLLWINDOW:
            return css::accessibility::AccessibleRole::SCROLL_PANE;

        case WindowType::WINDOW:
        case WindowType::CONTROL:
        case WindowType::BORDERWINDOW:
        case WindowType::SYSTEMCHILDWINDOW:
        default:
            if (IsNativeFrame())
                return css::accessibility::AccessibleRole::FRAME;

            if (IsScrollable())
                return css::accessibility::AccessibleRole::SCROLL_PANE;

            // #106002#, contextmenus are windows (i.e. toplevel)
            if (this->ImplGetWindow()->IsMenuFloatingWindow())
                return css::accessibility::AccessibleRole::WINDOW;

            // #104051# WINDOW seems to be a bad default role, use LAYEREDPANE instead
            // a WINDOW is interpreted as a top-level window, which is typically not the case
            //nRole = accessibility::AccessibleRole::WINDOW;
            return css::accessibility::AccessibleRole::PANEL;
    }

    return css::accessibility::AccessibleRole::UNKNOWN;
}

sal_uInt16 Window::GetAccessibleRole() const
{
    if (!mpAccessibleData)
        return css::accessibility::AccessibleRole::UNKNOWN;

    sal_uInt16 nRole = mpAccessibleData->getAccessibleRole();

    if (nRole == css::accessibility::AccessibleRole::UNKNOWN)
        nRole = getDefaultAccessibleRole();

    return nRole;
}

void Window::SetAccessibleName(const OUString& rName)
{
    if (!mpAccessibleData)
        return;

    OUString oldName = GetAccessibleName();
    mpAccessibleData->setAccessibleName(rName);
    CallEventListeners(VclEventId::WindowFrameTitleChanged, &oldName);
}

OUString Window::GetAccessibleName() const
{
    if (!mpAccessibleData)
        return OUString();

    if (auto aName = mpAccessibleData->getAccessibleName())
        return *aName;

    return getDefaultAccessibleName();
}

OUString Window::getDefaultAccessibleName() const
{
    switch (GetType())
    {
        case WindowType::MULTILINEEDIT:
        case WindowType::PATTERNFIELD:
        case WindowType::METRICFIELD:
        case WindowType::CURRENCYFIELD:
        case WindowType::EDIT:
        case WindowType::DATEBOX:
        case WindowType::TIMEBOX:
        case WindowType::CURRENCYBOX:
        case WindowType::LONGCURRENCYBOX:
        case WindowType::DATEFIELD:
        case WindowType::TIMEFIELD:
        case WindowType::SPINFIELD:
        case WindowType::FORMATTEDFIELD:
        case WindowType::COMBOBOX:
        case WindowType::LISTBOX:
        case WindowType::MULTILISTBOX:
        case WindowType::TREELISTBOX:
        case WindowType::METRICBOX:
        {
            OUString aName;

            if (vcl::Window* pLabel = GetAccessibleRelationLabeledBy(); pLabel && pLabel != this)
            {
                aName = pLabel->GetText();
            }

            if (aName.isEmpty())
                aName = GetQuickHelpText();

            if (aName.isEmpty())
                aName = GetText();

            return removeMnemonicFromString(aName);
        }

        case WindowType::IMAGEBUTTON:
        case WindowType::PUSHBUTTON:
        {
            OUString aName = GetText();

            if (aName.isEmpty())
            {
                aName = GetQuickHelpText();

                if (aName.isEmpty())
                    aName = GetHelpText();
            }

            return removeMnemonicFromString(aName);
        }

        case WindowType::MOREBUTTON:
            return removeMnemonicFromString(maText);

        case WindowType::TOOLBOX:
        default:
            return removeMnemonicFromString(GetText());
    }
}

void Window::SetAccessibleDescription(const OUString& rDescription)
{
    if (!mpAccessibleData)
        return;

    std::optional<OUString> currentDesc = mpAccessibleData->getAccessibleDescription();

    SAL_WARN_IF(currentDesc && *currentDesc != rDescription, "vcl",
                "AccessibleDescription already set");

    mpAccessibleData->setAccessibleDescription(rDescription);
}

OUString Window::GetAccessibleDescription() const
{
    if (!mpAccessibleData)
        return OUString();

    if (auto aDesc = mpAccessibleData->getAccessibleDescription())
        return *aDesc;

    // Special code for help text windows. ZT asks the border window for the
    // description so we have to forward this request to our inner window.
    if (const vcl::Window* pWin = this->ImplGetWindow();
        pWin->GetType() == WindowType::HELPTEXTWINDOW)
        return pWin->GetHelpText();

    return GetHelpText();
}

void Window::SetAccessibleRelationLabeledBy(vcl::Window* pLabeledBy)
{
    if (mpAccessibleData)
        mpAccessibleData->setAccessibleRelationLabeledBy(pLabeledBy);
}

void Window::SetAccessibleRelationLabelFor(vcl::Window* pLabelFor)
{
    if (mpAccessibleData)
        mpAccessibleData->setAccessibleRelationLabelFor(pLabelFor);
}

vcl::Window* Window::GetAccessibleRelationMemberOf() const
{
    if (!isContainerWindow(this) && !isContainerWindow(GetParent()))
        return getLegacyNonLayoutAccessibleRelationMemberOf();

    return nullptr;
}

vcl::Window* Window::getAccessibleRelationLabelFor() const
{
    if (!mpAccessibleData)
        return nullptr;

    return mpAccessibleData->getAccessibleRelationLabelFor();
}

vcl::Window* Window::GetAccessibleRelationLabelFor() const
{
    vcl::Window* pWindow = getAccessibleRelationLabelFor();

    if (pWindow)
        return pWindow;

    if (!isContainerWindow(this) && !isContainerWindow(GetParent()))
        return getLegacyNonLayoutAccessibleRelationLabelFor();

    return nullptr;
}

vcl::Window* Window::GetAccessibleRelationLabeledBy() const
{
    if (!mpAccessibleData)
        return nullptr;

    if (vcl::Window* pWin = mpAccessibleData->getAccessibleRelationLabeledBy())
        return pWin;

    if (auto const& rMnemonicLabels = list_mnemonic_labels(); !rMnemonicLabels.empty())
    {
        //if we have multiple labels, then prefer the first that is visible
        for (auto const& rCandidate : rMnemonicLabels)
        {
            if (rCandidate->IsVisible())
                return rCandidate;
        }

        return rMnemonicLabels[0];
    }

    if (!isContainerWindow(this) && !isContainerWindow(GetParent()))
        return getLegacyNonLayoutAccessibleRelationLabeledBy();

    return nullptr;
}

bool Window::AreAccessibilityEventsSuppressed()
{
    for (vcl::Window* pWin = this; pWin && pWin->mpClassification; pWin = pWin->GetParent())
    {
        if (pWin->mpAccessibleData && pWin->mpAccessibleData->isEventsSuspended())
            return true;
    }

    return false;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
