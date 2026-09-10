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

#include <sal/log.hxx>

#include <vcl/layout.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/window.hxx>
#include <vcl/menu.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/accessibility/vclxaccessiblecomponent.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/wrkwin.hxx>

#include <ImplAccessibleInfos.hxx>
#include <window.h>
#include <WindowClassification.hxx>
#include <WindowHierarchy.hxx>
#include <WindowAccessibleData.hxx>
#include <accessibility/floatingwindowaccessible.hxx>
#include <accessibility/vclxaccessiblefixedtext.hxx>
#include <accessibility/vclxaccessiblestatusbar.hxx>
#include <accessibility/vclxaccessibletabcontrol.hxx>
#include <accessibility/vclxaccessibletabpagewindow.hxx>
#include <brdwin.hxx>

#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;
using namespace ::com::sun::star;


ImplAccessibleInfos::ImplAccessibleInfos()
{
    nAccessibleRole = accessibility::AccessibleRole::UNKNOWN;
    pLabeledByWindow = nullptr;
    pLabelForWindow = nullptr;
}

ImplAccessibleInfos::~ImplAccessibleInfos()
{
}

namespace vcl {

rtl::Reference<comphelper::OAccessible> Window::GetAccessible(bool bCreate)
{
    // do not optimize hierarchy for the top level border win (ie, when there is no parent)
    /* // do not optimize accessible hierarchy at all to better reflect real VCL hierarchy
    if ( GetParent() && ( GetType() == WindowType::BORDERWINDOW ) && ( GetChildCount() == 1 ) )
    //if( !ImplIsAccessibleCandidate() )
    {
        vcl::Window* pChild = GetAccessibleChildWindow( 0 );
        if ( pChild )
            return pChild->GetAccessible();
    }
    */
    if (!mpAccessibleData)
        return {};

    if (!mpAccessibleData->mpAccessible.is() && !mpClassification->mbInDispose && bCreate)
        mpAccessibleData->mpAccessible = CreateAccessible();

    return mpAccessibleData->mpAccessible;
}

namespace {

bool hasFloatingChild(vcl::Window *pWindow)
{
    vcl::Window * pChild = pWindow->GetAccessibleChildWindow(0);
    return pChild && pChild->GetType() == WindowType::FLOATINGWINDOW;
}
};

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

    if (eType == WindowType::BORDERWINDOW && hasFloatingChild(this))
        return new FloatingWindowAccessible(this);

    if ((eType == WindowType::HELPTEXTWINDOW) || (eType == WindowType::FIXEDLINE))
        return new VCLXAccessibleFixedText(this);

    return new VCLXAccessibleComponent(this);
}

void Window::SetAccessible(const rtl::Reference<comphelper::OAccessible>& rpAccessible)
{
    if (!mpAccessibleData)
        return;

    mpAccessibleData->mpAccessible = rpAccessible;
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
    if( GetType() == WindowType::MENUBARWINDOW )
    {
        // report the menubar as a child of THE workwindow
        vcl::Window *pWorkWin = GetParent()->mpHierarchy->mpFirstChild;
        while( pWorkWin && (pWorkWin == this) )
            pWorkWin = pWorkWin->mpHierarchy->mpNext;
        pParent = pWorkWin;
    }
    // If this is a floating window which has a native border window, then that border should be reported as
    // the accessible parent
    else if( GetType() == WindowType::FLOATINGWINDOW &&
        mpHierarchy->mpBorderWindow && mpHierarchy->mpBorderWindow->mpClassification->mbFrame )
    {
        pParent = mpHierarchy->mpBorderWindow;
    }
    else if( pParent && !pParent->ImplIsAccessibleCandidate() )
    {
        pParent = pParent->mpHierarchy->mpParent;
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
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    mpAccessibleData->mpAccessibleInfos->pAccessibleParent = rpParent;
}

rtl::Reference<comphelper::OAccessible> Window::GetAccessibleParent() const
{
    if (!mpAccessibleData)
        return nullptr;

    if (mpAccessibleData->mpAccessibleInfos && mpAccessibleData->mpAccessibleInfos->pAccessibleParent.is())
        return mpAccessibleData->mpAccessibleInfos->pAccessibleParent;

    if (vcl::Window* pAccessibleParentWin = GetAccessibleParentWindow())
        return pAccessibleParentWin->GetAccessible();

    return nullptr;
}

void Window::SetAccessibleRole( sal_uInt16 nRole )
{
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    SAL_WARN_IF( mpAccessibleData->mpAccessibleInfos->nAccessibleRole != accessibility::AccessibleRole::UNKNOWN, "vcl", "AccessibleRole already set!" );
    mpAccessibleData->mpAccessibleInfos->nAccessibleRole = nRole;
}

sal_uInt16 Window::getDefaultAccessibleRole() const
{
    sal_uInt16 nRole = accessibility::AccessibleRole::UNKNOWN;
    switch (GetType())
    {
        case WindowType::MESSBOX:
        case WindowType::INFOBOX:
        case WindowType::WARNINGBOX:
        case WindowType::ERRORBOX:
        case WindowType::QUERYBOX:
            nRole = accessibility::AccessibleRole::ALERT;
            break;

        case WindowType::MODELESSDIALOG:
        case WindowType::TABDIALOG:
        case WindowType::BUTTONDIALOG:
        case WindowType::DIALOG:
            nRole = accessibility::AccessibleRole::DIALOG;
            break;

        case WindowType::PUSHBUTTON:
        case WindowType::OKBUTTON:
        case WindowType::CANCELBUTTON:
        case WindowType::HELPBUTTON:
        case WindowType::IMAGEBUTTON:
        case WindowType::MOREBUTTON:
            nRole = accessibility::AccessibleRole::PUSH_BUTTON;
            break;
        case WindowType::MENUBUTTON:
            nRole = accessibility::AccessibleRole::BUTTON_MENU;
            break;

        case WindowType::RADIOBUTTON:
            nRole = accessibility::AccessibleRole::RADIO_BUTTON;
            break;
        case WindowType::TRISTATEBOX:
        case WindowType::CHECKBOX:
            nRole = accessibility::AccessibleRole::CHECK_BOX;
            break;

        case WindowType::MULTILINEEDIT:
            nRole = accessibility::AccessibleRole::SCROLL_PANE;
            break;

        case WindowType::PATTERNFIELD:
        case WindowType::EDIT:
            nRole = static_cast<Edit const*>(this)->IsPassword()
                        ? accessibility::AccessibleRole::PASSWORD_TEXT
                        : accessibility::AccessibleRole::TEXT;
            break;

        case WindowType::PATTERNBOX:
        case WindowType::NUMERICBOX:
        case WindowType::METRICBOX:
        case WindowType::CURRENCYBOX:
        case WindowType::LONGCURRENCYBOX:
        case WindowType::COMBOBOX:
            nRole = accessibility::AccessibleRole::COMBO_BOX;
            break;

        case WindowType::LISTBOX:
        case WindowType::MULTILISTBOX:
            nRole = accessibility::AccessibleRole::LIST;
            break;

        case WindowType::TREELISTBOX:
            nRole = accessibility::AccessibleRole::TREE;
            break;

        case WindowType::FIXEDTEXT:
            nRole = accessibility::AccessibleRole::LABEL;
            break;
        case WindowType::FIXEDLINE:
            if (!GetText().isEmpty())
                nRole = accessibility::AccessibleRole::LABEL;
            else
                nRole = accessibility::AccessibleRole::SEPARATOR;
            break;

        case WindowType::FIXEDBITMAP:
        case WindowType::FIXEDIMAGE:
            nRole = accessibility::AccessibleRole::ICON;
            break;
        case WindowType::GROUPBOX:
            nRole = accessibility::AccessibleRole::GROUP_BOX;
            break;
        case WindowType::SCROLLBAR:
            nRole = accessibility::AccessibleRole::SCROLL_BAR;
            break;

        case WindowType::SLIDER:
        case WindowType::SPLITTER:
        case WindowType::SPLITWINDOW:
            nRole = accessibility::AccessibleRole::SPLIT_PANE;
            break;

        case WindowType::DATEBOX:
        case WindowType::TIMEBOX:
        case WindowType::DATEFIELD:
        case WindowType::TIMEFIELD:
            nRole = accessibility::AccessibleRole::DATE_EDITOR;
            break;

        case WindowType::METRICFIELD:
        case WindowType::CURRENCYFIELD:
        case WindowType::SPINBUTTON:
        case WindowType::SPINFIELD:
        case WindowType::FORMATTEDFIELD:
            nRole = accessibility::AccessibleRole::SPIN_BOX;
            break;

        case WindowType::TOOLBOX:
            nRole = accessibility::AccessibleRole::TOOL_BAR;
            break;
        case WindowType::STATUSBAR:
            nRole = accessibility::AccessibleRole::STATUS_BAR;
            break;

        case WindowType::TABPAGE:
            nRole = accessibility::AccessibleRole::PANEL;
            break;
        case WindowType::TABCONTROL:
            nRole = accessibility::AccessibleRole::PAGE_TAB_LIST;
            break;

        case WindowType::DOCKINGWINDOW:
            nRole = (mpClassification->mbFrame) ? accessibility::AccessibleRole::FRAME
                                            : accessibility::AccessibleRole::PANEL;
            break;

        case WindowType::FLOATINGWINDOW:
            nRole = (mpClassification->mbFrame
                     || (mpHierarchy->mpBorderWindow
                         && mpHierarchy->mpBorderWindow->mpClassification->mbFrame)
                     || (GetStyle() & WB_OWNERDRAWDECORATION))
                        ? accessibility::AccessibleRole::FRAME
                        : accessibility::AccessibleRole::WINDOW;
            break;

        case WindowType::WORKWINDOW:
            nRole = accessibility::AccessibleRole::ROOT_PANE;
            break;

        case WindowType::SCROLLBARBOX:
            nRole = accessibility::AccessibleRole::FILLER;
            break;

        case WindowType::HELPTEXTWINDOW:
            nRole = accessibility::AccessibleRole::TOOL_TIP;
            break;

        case WindowType::PROGRESSBAR:
            nRole = accessibility::AccessibleRole::PROGRESS_BAR;
            break;

        case WindowType::RULER:
            nRole = accessibility::AccessibleRole::RULER;
            break;

        case WindowType::SCROLLWINDOW:
            nRole = accessibility::AccessibleRole::SCROLL_PANE;
            break;

        case WindowType::WINDOW:
        case WindowType::CONTROL:
        case WindowType::BORDERWINDOW:
        case WindowType::SYSTEMCHILDWINDOW:
        default:
            if (IsNativeFrame())
                nRole = accessibility::AccessibleRole::FRAME;
            else if (IsScrollable())
                nRole = accessibility::AccessibleRole::SCROLL_PANE;
            else if (this->ImplGetWindow()->IsMenuFloatingWindow())
                // #106002#, contextmenus are windows (i.e. toplevel)
                nRole = accessibility::AccessibleRole::WINDOW;
            else
                // #104051# WINDOW seems to be a bad default role, use LAYEREDPANE instead
                // a WINDOW is interpreted as a top-level window, which is typically not the case
                //nRole = accessibility::AccessibleRole::WINDOW;
                nRole = accessibility::AccessibleRole::PANEL;
    }
    return nRole;
}

sal_uInt16 Window::GetAccessibleRole() const
{
    if (!mpAccessibleData)
        return accessibility::AccessibleRole::UNKNOWN;

    sal_uInt16 nRole = mpAccessibleData->mpAccessibleInfos
                           ? mpAccessibleData->mpAccessibleInfos->nAccessibleRole
                           : accessibility::AccessibleRole::UNKNOWN;
    if (nRole == accessibility::AccessibleRole::UNKNOWN)
        nRole = getDefaultAccessibleRole();
    return nRole;
}

void Window::SetAccessibleName( const OUString& rName )
{
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    OUString oldName = GetAccessibleName();

    mpAccessibleData->mpAccessibleInfos->pAccessibleName = rName;

    CallEventListeners(VclEventId::WindowFrameTitleChanged, &oldName);
}

OUString Window::GetAccessibleName() const
{
    if (!mpAccessibleData)
        return OUString();

    if (mpAccessibleData->mpAccessibleInfos && mpAccessibleData->mpAccessibleInfos->pAccessibleName)
        return *mpAccessibleData->mpAccessibleInfos->pAccessibleName;

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
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    std::optional<OUString>& rCurrentDescription = mpAccessibleData->mpAccessibleInfos->pAccessibleDescription;
    SAL_WARN_IF(rCurrentDescription && *rCurrentDescription != rDescription, "vcl", "AccessibleDescription already set");
    rCurrentDescription = rDescription;
}

OUString Window::GetAccessibleDescription() const
{
    if (!mpAccessibleData)
        return OUString();

    OUString aAccessibleDescription;
    if (mpAccessibleData->mpAccessibleInfos && mpAccessibleData->mpAccessibleInfos->pAccessibleDescription )
    {
        aAccessibleDescription = *mpAccessibleData->mpAccessibleInfos->pAccessibleDescription;
    }
    else
    {
        // Special code for help text windows. ZT asks the border window for the
        // description so we have to forward this request to our inner window.
        const vcl::Window* pWin = this->ImplGetWindow();
        if (pWin->GetType() == WindowType::HELPTEXTWINDOW)
            aAccessibleDescription = pWin->GetHelpText();
        else
            aAccessibleDescription = GetHelpText();
    }

    return aAccessibleDescription;
}

void Window::SetAccessibleRelationLabeledBy( vcl::Window* pLabeledBy )
{
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    mpAccessibleData->mpAccessibleInfos->pLabeledByWindow = pLabeledBy;
}

void Window::SetAccessibleRelationLabelFor( vcl::Window* pLabelFor )
{
    if (!mpAccessibleData)
        return;

    if (!mpAccessibleData->mpAccessibleInfos)
        mpAccessibleData->mpAccessibleInfos.reset(new ImplAccessibleInfos);

    mpAccessibleData->mpAccessibleInfos->pLabelForWindow = pLabelFor;
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

    if (mpAccessibleData->mpAccessibleInfos && mpAccessibleData->mpAccessibleInfos->pLabelForWindow)
        return mpAccessibleData->mpAccessibleInfos->pLabelForWindow;

    return nullptr;
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

    if (mpAccessibleData->mpAccessibleInfos && mpAccessibleData->mpAccessibleInfos->pLabeledByWindow)
        return mpAccessibleData->mpAccessibleInfos->pLabeledByWindow;

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
