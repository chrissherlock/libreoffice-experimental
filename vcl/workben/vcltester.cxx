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

#include <sal/main.h>
#include <tools/extendapplicationenvironment.hxx>

#include <cppuhelper/bootstrap.hxx>
#include <comphelper/processfactory.hxx>

#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/layout.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/vclptr.hxx>

#include <unistd.h>
#include <stdio.h>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace cppu;

// Forward declaration
void Main();

SAL_IMPLEMENT_MAIN()
{
    tools::extendApplicationEnvironment();

    Reference<XComponentContext> xContext = defaultBootstrap_InitialComponentContext();
    Reference<XMultiServiceFactory> xServiceManager(xContext->getServiceManager(), UNO_QUERY);

    if (!xServiceManager.is())
        Application::Abort("Failed to bootstrap");

    comphelper::setProcessServiceFactory(xServiceManager);

    InitVCL();
    ::Main();
    DeInitVCL();

    return 0;
}

class MyWin : public WorkWindow
{
public:
    MyWin(Window* pParent, WinBits nWinStyle);

    void Paint(vcl::RenderContext&, const tools::Rectangle& rRect) override;
    void Resize() override;

private:
    VclPtr<VclBox> mpBox;
    VclPtr<VclHBox> mpHBox;
    VclPtr<ListBox> mpListbox;
};

void Main()
{
    MyWin aMainWin(NULL, WB_APP | WB_STDWORK);
    aMainWin.SetText(OUString("VCLDemo - VCL Workbench"));
    aMainWin.Show();

    Application::Execute();
}

MyWin::MyWin(Window* pParent, WinBits nWinStyle)
    : WorkWindow(pParent, nWinStyle)
    , mpBox(VclPtrInstance<VclVBox>(this, false, 1))
    , mpHBox(VclPtrInstance<VclHBox>(mpBox.get(), true, 1))
    , mpListbox(VclPtrInstance<ListBox>(mpHBox.get()))
{
    mpBox->Show();

    mpListbox->InsertEntry("Pixel test");
    mpListbox->InsertEntry("Pixel test 2");
    mpListbox->SelectEntryPos(0);
    mpListbox->ToggleDropDown();
    mpListbox->Show();

    mpHBox->Show();
}

void MyWin::Paint(vcl::RenderContext&, const tools::Rectangle&)
{
    Point aPt(GetSizePixel().Width() / 2, GetSizePixel().Height() / 2);

    DrawPixel(aPt, COL_RED);
}

void MyWin::Resize()
{
    Color aColor(COL_RED);
    fprintf(stderr, "MyWin::Resize()\n\tColor: #%.2x%.2x%.2x%.2x\n", aColor.GetRed(),
            aColor.GetGreen(), aColor.GetBlue(), aColor.GetTransparency());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
