/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <sal/main.h>
#include <tools/gen.hxx>

#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/outdev.hxx>
#include <vcl/font.hxx>
#include <vcl/metafile/MetaActionType.hxx>

#include <comphelper/processfactory.hxx>
#include <cppuhelper/bootstrap.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <i18nlangtag/mslangid.hxx>

#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/XDesktop2.hpp>

#include <iostream>
#include <iomanip>

using namespace com::sun::star;

class ShowMetafileApp : public Application
{
public:
    virtual int Main() override;
    virtual void Init() override;
    virtual void DeInit() override;

private:
    uno::Reference<lang::XMultiServiceFactory> xServiceManager;
    uno::Reference<frame::XDesktop2> xDesktop;
};

void ShowMetafileApp::Init()
{
    try
    {
        auto xContext = cppu::defaultBootstrap_InitialComponentContext();
        xServiceManager.set(xContext->getServiceManager(), uno::UNO_QUERY);

        if (!xServiceManager.is())
            Application::Abort(u"Bootstrap failure - no service manager"_ustr);

        comphelper::setProcessServiceFactory(xServiceManager);

        // Grab the Desktop instance so we can terminate it later
        xDesktop = frame::Desktop::create(xContext);

        // Initialize Language to avoid warnings and ensure correct layout
        LanguageTag::setConfiguredSystemLanguage(MsLangId::getSystemLanguage());
    }
    catch (const uno::Exception& e)
    {
        std::cerr << "Initialization failed: " << e.Message << std::endl;
        std::exit(1);
    }
}

void ShowMetafileApp::DeInit()
{
    try
    {
        if (xDesktop.is())
        {
            xDesktop->terminate();
            xDesktop.clear();
        }

        auto xContext = uno::Reference<lang::XComponent>(comphelper::getProcessComponentContext(),
                                                         uno::UNO_QUERY);
        if (xContext.is())
            xContext->dispose();
    }
    catch (...)
    {
    }

    ::comphelper::setProcessServiceFactory(nullptr);
}

int ShowMetafileApp::Main()
{
    std::cout << "Initializing VirtualDevice..." << std::endl;

    ScopedVclPtr<VirtualDevice> pDev = VclPtr<VirtualDevice>::Create();
    pDev->SetOutputSizePixel(Size(1000, 1000));

    // Use a standard font
    vcl::Font aFont(u"Liberation Sans"_ustr, Size(0, 12));
    pDev->SetFont(aFont);

    DrawTextFlags nFlags
        = DrawTextFlags::Center | DrawTextFlags::MultiLine | DrawTextFlags::PathEllipsis;

    // Use text that is clearly wider than the rectangle to force the ellipsis logic
    OUString aText
        = u"Start of the line ................................................. end of the line"_ustr;

    // Narrow rectangle to force wrapping/ellipsis
    tools::Rectangle aRect(Point(10, 10), Size(100, 50));

    std::cout << "Recording DrawText operation..." << std::endl;
    GDIMetaFile aMtf;
    aMtf.Record(pDev.get());

    pDev->DrawText(aRect, aText, nFlags);

    aMtf.Stop();

    std::cout << "\n--- Metafile Dump ---" << std::endl;
    std::cout << "Action Count: " << aMtf.GetActionSize() << "\n" << std::endl;

    int nIndent = 0;

    for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
    {
        MetaAction* pAction = aMtf.GetAction(i);
        MetaActionType nType = pAction->GetType();

        if (nType == MetaActionType::COMMENT)
        {
            auto* pComment = static_cast<MetaCommentAction*>(pAction);
            OString sComment = pComment->GetComment();

            if (sComment.startsWith("EndGroup"))
                nIndent = std::max(0, nIndent - 2);

            std::cout << std::string(nIndent, ' ') << "[" << i << "] COMMENT: " << sComment.getStr()
                      << std::endl;

            if (sComment.startsWith("BeginGroup"))
                nIndent += 2;
        }
        else
        {
            std::cout << std::string(nIndent, ' ') << "[" << i
                      << "] Action Type: " << static_cast<int>(nType);

            if (nType == MetaActionType::TEXTRECT)
                std::cout << " (MetaTextRectAction - Atomic)";
            else if (nType == MetaActionType::TEXT || nType == MetaActionType::TEXTARRAY)
                std::cout << " (MetaTextAction - Primitive)";

            std::cout << std::endl;
        }
    }
    std::cout << "---------------------" << std::endl;

    return 0;
}

SAL_IMPLEMENT_MAIN()
{
    ShowMetafileApp aApp;
    InitVCL();
    int ret = aApp.Main();
    DeInitVCL();
    return ret;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
