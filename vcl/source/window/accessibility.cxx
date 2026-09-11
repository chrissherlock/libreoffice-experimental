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

ImplAccessibleInfos::ImplAccessibleInfos()
{
    nAccessibleRole = css::accessibility::AccessibleRole::UNKNOWN;
    pLabeledByWindow = nullptr;
    pLabelForWindow = nullptr;
}

ImplAccessibleInfos::~ImplAccessibleInfos()
{
}

namespace vcl {

rtl::Reference<comphelper::OAccessible> Window::GetAccessible(bool bCreate)
{
    if (!mpAccessibleData)
        return {};

    if (!mpAccessibleData->hasAccessible() && !mpClassification->mbInDispose && bCreate)
        mpAccessibleData->setAccessible(CreateAccessible());

    return mpAccessibleData->getAccessible();
}

static bool lcl_hasFloatingChild(vcl::Window *pWindow)
{
    vcl::Window * pChild = pWindow->GetAccessibleChildWindow(0);
    return pChild && pChild->GetType() == WindowType::FLOATINGWINDOW;
}

rtl::Reference<comphelper::OAccessible> Window::CreateAccessible()
{
    const WindowType eType = GetType();

    if (eType == WindowType::STATUSBAR)
        return new VCLXAccessibleStatusBar(this);

    if (eType == WindowType::TABCONTROL)
        return new VCLXAccessibleTabControl(this);

    if (eType == WindowType::TABPAGE && GetAccessibleParentWindow() && GetAccessibleParentWindow()->GetType() == WindowType::TABCONTROL)
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
    if( !mpClassification->mbBorderWin )
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

    vcl::Window* pParent = mpHierarchy->mpParent;

    if (GetType() == WindowType::MENUBARWINDOW)
    {
        // report the menubar as a child of THE workwindow
        if (vcl::Window* pRealParent = GetParent())
        {
            vcl::Window* pWorkWin = pRealParent->GetWindow(GetWindowType::FirstChild);
            while (pWorkWin && (pWorkWin == this))
                pWorkWin = pWorkWin->GetWindow(GetWindowType::Next);

            if (pWorkWin)
                pParent = pWorkWin;
        }
    }
    // If this is a floating window which has a native border window, then that border should be reported as
    // the accessible parent
    else if (GetType() == WindowType::FLOATINGWINDOW &&
             mpHierarchy->mpBorderWindow &&
             mpHierarchy->mpBorderWindow->ImplGetWindowClassification()->mbFrame)
    {
        pParent = mpHierarchy->mpBorderWindow;
    }
    else if (pParent && !pParent->ImplIsAccessibleCandidate())
    {
        pParent = pParent->ImplGetParent();
    }

    return pParent;
}

sal_uInt16 Window::GetAccessibleChildWindowCount()
{
    if (!mpClassification)
        return 0;

    sal_uInt16 nChildren = 0;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while( pChild )
    {
        if( pChild->IsVisible() )
            nChildren++;
        pChild = pChild->mpHierarchy->mpNext;
    }

    // report the menubarwindow as a child of THE workwindow
    if( GetType() == WindowType::BORDERWINDOW )
    {
        ImplBorderWindow *pBorderWindow = static_cast<ImplBorderWindow*>(this);
        if( pBorderWindow->mpMenuBarWindow &&
            pBorderWindow->mpMenuBarWindow->IsVisible()
            )
            --nChildren;
    }
    else if( GetType() == WindowType::WORKWINDOW )
    {
        WorkWindow *pWorkWindow = static_cast<WorkWindow*>(this);
        if( pWorkWindow->GetMenuBar() &&
            pWorkWindow->GetMenuBar()->GetWindow() &&
            pWorkWindow->GetMenuBar()->GetWindow()->IsVisible()
            )
            ++nChildren;
    }

    return nChildren;
}

vcl::Window* Window::GetAccessibleChildWindow( sal_uInt16 n )
{
    // report the menubarwindow as the first child of THE workwindow
    if( GetType() == WindowType::WORKWINDOW && static_cast<WorkWindow *>(this)->GetMenuBar() )
    {
        if( n == 0 )
        {
            MenuBar *pMenuBar = static_cast<WorkWindow *>(this)->GetMenuBar();
            if( pMenuBar->GetWindow() && pMenuBar->GetWindow()->IsVisible() )
                return pMenuBar->GetWindow();
        }
        else
            --n;
    }

    // transform n to child number including invisible children
    sal_uInt16 nChildren = n;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while( pChild )
    {
        if( pChild->IsVisible() )
        {
            if( ! nChildren )
                break;
            nChildren--;
        }
        pChild = pChild->mpHierarchy->mpNext;
    }

    if( GetType() == WindowType::BORDERWINDOW && pChild && pChild->GetType() == WindowType::MENUBARWINDOW )
    {
        do pChild = pChild->mpHierarchy->mpNext; while( pChild && ! pChild->IsVisible() );
        SAL_WARN_IF( !pChild, "vcl", "GetAccessibleChildWindow(): wrong index in border window");
    }

    if ( pChild && ( pChild->GetType() == WindowType::BORDERWINDOW ) && ( pChild->GetChildCount() == 1 ) )
    {
        pChild = pChild->GetChild( 0 );
    }
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

void Window::SetAccessibleRole( sal_uInt16 nRole )
{
    if (mpAccessibleData)
    {
        SAL_WARN_IF( mpAccessibleData->getAccessibleRole() != css::accessibility::AccessibleRole::UNKNOWN, "vcl", "AccessibleRole already set!" );
        mpAccessibleData->setAccessibleRole(nRole);
    }
}

sal_uInt16 Window::getDefaultAccessibleRole() const
{
    sal_uInt16 nRole = css::accessibility::AccessibleRole::UNKNOWN;
    switch (GetType())
    {
        case WindowType::MESSBOX:
        case WindowType::INFOBOX:
        case WindowType::WARNINGBOX:
        case WindowType::ERRORBOX:
        case WindowType::QUERYBOX:
            nRole = css::accessibility::AccessibleRole::ALERT;
            break;

        case WindowType::MODELESSDIALOG:
        case WindowType::TABDIALOG:
        case WindowType::BUTTONDIALOG:
        case WindowType::DIALOG:
            nRole = css::accessibility::AccessibleRole::DIALOG;
            break;

        case WindowType::PUSHBUTTON:
        case WindowType::OKBUTTON:
        case WindowType::CANCELBUTTON:
        case WindowType::HELPBUTTON:
        case WindowType::IMAGEBUTTON:
        case WindowType::MOREBUTTON:
            nRole = css::accessibility::AccessibleRole::PUSH_BUTTON;
            break;
        case WindowType::MENUBUTTON:
            nRole = css::accessibility::AccessibleRole::BUTTON_MENU;
            break;

        case WindowType::RADIOBUTTON:
            nRole = css::accessibility::AccessibleRole::RADIO_BUTTON;
            break;
        case WindowType::TRISTATEBOX:
        case WindowType::CHECKBOX:
            nRole = css::accessibility::AccessibleRole::CHECK_BOX;
            break;

        case WindowType::MULTILINEEDIT:
            nRole = css::accessibility::AccessibleRole::SCROLL_PANE;
            break;

        case WindowType::PATTERNFIELD:
        case WindowType::EDIT:
            nRole = static_cast<Edit const*>(this)->IsPassword()
                        ? css::accessibility::AccessibleRole::PASSWORD_TEXT
                        : css::accessibility::AccessibleRole::TEXT;
            break;

        case WindowType::PATTERNBOX:
        case WindowType::NUMERICBOX:
        case WindowType::METRICBOX:
        case WindowType::CURRENCYBOX:
        case WindowType::LONGCURRENCYBOX:
        case WindowType::COMBOBOX:
            nRole = css::accessibility::AccessibleRole::COMBO_BOX;
            break;

        case WindowType::LISTBOX:
        case WindowType::MULTILISTBOX:
            nRole = css::accessibility::AccessibleRole::LIST;
            break;

        case WindowType::TREELISTBOX:
            nRole = css::accessibility::AccessibleRole::TREE;
            break;

        case WindowType::FIXEDTEXT:
            nRole = css::accessibility::AccessibleRole::LABEL;
            break;
        case WindowType::FIXEDLINE:
            if (!GetText().isEmpty())
                nRole = css::accessibility::AccessibleRole::LABEL;
            else
                nRole = css::accessibility::AccessibleRole::SEPARATOR;
            break;

        case WindowType::FIXEDBITMAP:
        case WindowType::FIXEDIMAGE:
            nRole = css::accessibility::AccessibleRole::ICON;
            break;
        case WindowType::GROUPBOX:
            nRole = css::accessibility::AccessibleRole::GROUP_BOX;
            break;
        case WindowType::SCROLLBAR:
            nRole = css::accessibility::AccessibleRole::SCROLL_BAR;
            break;

        case WindowType::SLIDER:
        case WindowType::SPLITTER:
        case WindowType::SPLITWINDOW:
            nRole = css::accessibility::AccessibleRole::SPLIT_PANE;
            break;

        case WindowType::DATEBOX:
        case WindowType::TIMEBOX:
        case WindowType::DATEFIELD:
        case WindowType::TIMEFIELD:
            nRole = css::accessibility::AccessibleRole::DATE_EDITOR;
            break;

        case WindowType::METRICFIELD:
        case WindowType::CURRENCYFIELD:
        case WindowType::SPINBUTTON:
        case WindowType::SPINFIELD:
        case WindowType::FORMATTEDFIELD:
            nRole = css::accessibility::AccessibleRole::SPIN_BOX;
            break;

        case WindowType::TOOLBOX:
            nRole = css::accessibility::AccessibleRole::TOOL_BAR;
            break;
        case WindowType::STATUSBAR:
            nRole = css::accessibility::AccessibleRole::STATUS_BAR;
            break;

        case WindowType::TABPAGE:
            nRole = css::accessibility::AccessibleRole::PANEL;
            break;
        case WindowType::TABCONTROL:
            nRole = css::accessibility::AccessibleRole::PAGE_TAB_LIST;
            break;

        case WindowType::DOCKINGWINDOW:
            nRole = (mpClassification->mbFrame) ? css::accessibility::AccessibleRole::FRAME
                                            : css::accessibility::AccessibleRole::PANEL;
            break;

        case WindowType::FLOATINGWINDOW:
            nRole = (mpClassification->mbFrame
                     || (mpHierarchy->mpBorderWindow
                         && mpHierarchy->mpBorderWindow->mpClassification->mbFrame)
                     || (GetStyle() & WB_OWNERDRAWDECORATION))
                        ? css::accessibility::AccessibleRole::FRAME
                        : css::accessibility::AccessibleRole::WINDOW;
            break;

        case WindowType::WORKWINDOW:
            nRole = css::accessibility::AccessibleRole::ROOT_PANE;
            break;

        case WindowType::SCROLLBARBOX:
            nRole = css::accessibility::AccessibleRole::FILLER;
            break;

        case WindowType::HELPTEXTWINDOW:
            nRole = css::accessibility::AccessibleRole::TOOL_TIP;
            break;

        case WindowType::PROGRESSBAR:
            nRole = css::accessibility::AccessibleRole::PROGRESS_BAR;
            break;

        case WindowType::RULER:
            nRole = css::accessibility::AccessibleRole::RULER;
            break;

        case WindowType::SCROLLWINDOW:
            nRole = css::accessibility::AccessibleRole::SCROLL_PANE;
            break;

        case WindowType::WINDOW:
        case WindowType::CONTROL:
        case WindowType::BORDERWINDOW:
        case WindowType::SYSTEMCHILDWINDOW:
        default:
            if (IsNativeFrame())
                nRole = css::accessibility::AccessibleRole::FRAME;
            else if (IsScrollable())
                nRole = css::accessibility::AccessibleRole::SCROLL_PANE;
            else if (this->ImplGetWindow()->IsMenuFloatingWindow())
                // #106002#, contextmenus are windows (i.e. toplevel)
                nRole = css::accessibility::AccessibleRole::WINDOW;
            else
                // #104051# WINDOW seems to be a bad default role, use LAYEREDPANE instead
                // a WINDOW is interpreted as a top-level window, which is typically not the case
                //nRole = accessibility::AccessibleRole::WINDOW;
                nRole = css::accessibility::AccessibleRole::PANEL;
    }
    return nRole;
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

void Window::SetAccessibleName( const OUString& rName )
{
    if (mpAccessibleData)
    {
        OUString oldName = GetAccessibleName();
        mpAccessibleData->setAccessibleName(rName);
        CallEventListeners(VclEventId::WindowFrameTitleChanged, &oldName);
    }
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
    OUString aAccessibleName;
    switch ( GetType() )
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
            vcl::Window *pLabel = GetAccessibleRelationLabeledBy();
            if ( pLabel && pLabel != this )
                aAccessibleName = pLabel->GetText();
            if (aAccessibleName.isEmpty())
                aAccessibleName = GetQuickHelpText();
            if (aAccessibleName.isEmpty())
                aAccessibleName = GetText();
        }
        break;

        case WindowType::IMAGEBUTTON:
        case WindowType::PUSHBUTTON:
            aAccessibleName = GetText();
            if (aAccessibleName.isEmpty())
            {
                aAccessibleName = GetQuickHelpText();
                if (aAccessibleName.isEmpty())
                    aAccessibleName = GetHelpText();
            }
        break;

        case WindowType::TOOLBOX:
            aAccessibleName = GetText();
            break;

        case WindowType::MOREBUTTON:
            aAccessibleName = maText;
            break;

        default:
            aAccessibleName = GetText();
            break;
    }

    return removeMnemonicFromString( aAccessibleName );
}

void Window::SetAccessibleDescription( const OUString& rDescription )
{
    if (mpAccessibleData)
    {
        std::optional<OUString> currentDesc = mpAccessibleData->getAccessibleDescription();
        SAL_WARN_IF(currentDesc && *currentDesc != rDescription, "vcl", "AccessibleDescription already set");
        mpAccessibleData->setAccessibleDescription(rDescription);
    }
}

OUString Window::GetAccessibleDescription() const
{
    if (!mpAccessibleData)
        return OUString();

    if (auto aDesc = mpAccessibleData->getAccessibleDescription())
        return *aDesc;

    // Special code for help text windows. ZT asks the border window for the
    // description so we have to forward this request to our inner window.
    const vcl::Window* pWin = this->ImplGetWindow();
    if (pWin->GetType() == WindowType::HELPTEXTWINDOW)
        return pWin->GetHelpText();

    return GetHelpText();
}

void Window::SetAccessibleRelationLabeledBy( vcl::Window* pLabeledBy )
{
    if (mpAccessibleData)
        mpAccessibleData->setAccessibleRelationLabeledBy(pLabeledBy);
}

void Window::SetAccessibleRelationLabelFor( vcl::Window* pLabelFor )
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

    auto const& aMnemonicLabels = list_mnemonic_labels();
    if (!aMnemonicLabels.empty())
    {
        //if we have multiple labels, then prefer the first that is visible
        for (auto const & rCandidate : aMnemonicLabels)
        {
            if (rCandidate->IsVisible())
                return rCandidate;
        }
        return aMnemonicLabels[0];
    }

    if (!isContainerWindow(this) && !isContainerWindow(GetParent()))
        return getLegacyNonLayoutAccessibleRelationLabeledBy();

    return nullptr;
}

bool Window::AreAccessibilityEventsSuppressed()
{
    vcl::Window *pParent = this;
    while (pParent && pParent->mpClassification)
    {
        if (pParent->mpAccessibleData && pParent->mpAccessibleData->isEventsSuspended())
            return true;
        pParent = pParent->GetParent();
    }
    return false;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
