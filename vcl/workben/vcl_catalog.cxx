/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/mapunit.hxx>
#include <cppuhelper/bootstrap.hxx>
#include <comphelper/processfactory.hxx>

#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/tabctrl.hxx>
#include <vcl/tabpage.hxx>
#include <vcl/toolkit/button.hxx>
#include <vcl/toolkit/edit.hxx>
#include <vcl/toolkit/scrbar.hxx>
#include <vcl/toolkit/spinfld.hxx>
#include <vcl/toolkit/vclmedit.hxx>
#include <vcl/vclmain.hxx>
#include <vcl/menu.hxx>
#include <vcl/keycod.hxx>
#include <vcl/keycodes.hxx>
#include <vcl/layout.hxx>
#include <vcl/virdev.hxx>

#include <wizdlg.hxx>

#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>

class EditDrawCanvas : public vcl::Window
{
private:
    VclPtr<Edit> mpSourceEdit;

public:
    EditDrawCanvas(vcl::Window* pParent, VclPtr<Edit> pEdit)
        : vcl::Window(pParent, WB_BORDER)
        , mpSourceEdit(pEdit)
    {
        SetBackground(Wallpaper(COL_WHITE));
    }

    void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& /*rRect*/) override
    {
        // Use system-adaptive background color
        const StyleSettings& rSettings = Application::GetSettings().GetStyleSettings();
        Color aBgColor = rSettings.GetWindowColor();

        //  Wipe the canvas window background clean
        rRenderContext.SetFillColor(aBgColor);
        rRenderContext.SetLineColor(aBgColor);
        Size aCanvasSize = GetOutputSizePixel();
        rRenderContext.DrawRect(tools::Rectangle(Point(0, 0), aCanvasSize));

        // Allocate an off-screen device matching the exact size of the edit widget
        ScopedVclPtrInstance<VirtualDevice> pOffscreenDevice(rRenderContext);
        Size aEditSize = mpSourceEdit->GetSizePixel();
        pOffscreenDevice->SetOutputSizePixel(aEditSize);

        // Fill the off-screen buffer background with the adaptive color
        pOffscreenDevice->SetFillColor(aBgColor);
        pOffscreenDevice->SetLineColor(aBgColor);
        pOffscreenDevice->DrawRect(tools::Rectangle(Point(0, 0), aEditSize));

        // Force the edit control to draw its layout in the isolated buffer
        mpSourceEdit->Draw(*pOffscreenDevice.get(), Point(0, 0), SystemTextColorFlags::NONE);

        // Extract the snapshot bitmap
        Bitmap aSnapshot = pOffscreenDevice->GetBitmap(Point(0, 0), aEditSize);

        // Shift primary canvas context to logical space safely
        auto aStateGuard = rRenderContext.ScopedPush(vcl::PushFlags::MAPMODE);
        rRenderContext.SetMapMode(MapMode(MapUnit::Map100thMM));

        // Draw the isolated bitmap snapshot at your target logical coordinate
        Point aLogicPos(2000, 2000);
        rRenderContext.DrawBitmap(aLogicPos, aSnapshot);
    }
};

class CatalogMainWindow : public WorkWindow
{
private:
    // Menus
    VclPtr<PushButton> mpMenuBtn;
    VclPtr<PopupMenu> mpFileMenu;

    // UI Elements
    VclPtr<TabControl> mpTabs;

    VclPtr<TabPage> mpBtnPage;
    VclPtr<PushButton> mpTestBtn;

    VclPtr<TabPage> mpInpPage;
    VclPtr<Edit> mpTestEdit;
    VclPtr<EditDrawCanvas> mpCanvas;

    VclPtr<TabPage> mpScrollPage;
    VclPtr<ScrollBar> mpVertScroll;
    VclPtr<ScrollBar> mpHorzScroll;
    VclPtr<FixedText> mpScrollStatus;

    // SpinField Context
    VclPtr<TabPage> mpSpinPage;
    VclPtr<SpinField> mpTestSpinField;
    VclPtr<FixedText> mpSpinStatus;

    // Nested TabControl Context
    VclPtr<TabPage> mpTabCtrlPage;
    VclPtr<TabControl> mpNestedTabCtrl;
    VclPtr<TabPage> mpNestedPage1;
    VclPtr<TabPage> mpNestedPage2;

    // VclMultiLineEdit Context
    VclPtr<TabPage> mpMEditPage;
    VclPtr<VclMultiLineEdit> mpVclMultiLineEdit;

    VclPtr<PushButton> mpRoadmapWizardDemoBtn;

    // Local controller components for our manual wizard tracking drive
    sal_uInt16 mnWizardStep;
    VclPtr<vcl::RoadmapWizard> mpActiveWizard;
    VclPtr<TabPage> mpWizPages[3];

    void SyncWizardNavigation()
    {
        if (!mpActiveWizard)
            return;

        // Toggle page visibilities safely via public window states
        for (int i = 0; i < 3; ++i)
        {
            if (mpWizPages[i])
            {
                if (i == mnWizardStep)
                    mpWizPages[i]->Show();
                else
                    mpWizPages[i]->Hide();
            }
        }

        // Highlight the side tracker rail index
        mpActiveWizard->SelectRoadmapItemByID(mnWizardStep);
    }

public:
    CatalogMainWindow()
        : WorkWindow(nullptr, WB_STDWORK)
        , mnWizardStep(0)
    {
        SetText("VCL Widget Catalog (Pure C++)");
        SetSizePixel(Size(800, 600));

        // BUILD THE POPUP MENU
        mpFileMenu = VclPtr<PopupMenu>::Create();
        mpFileMenu->InsertItem(1, "Quit");
        mpFileMenu->SetAccelKey(1, vcl::KeyCode(KEY_Q, KEY_MOD1));
        mpFileMenu->SetSelectHdl(LINK(this, CatalogMainWindow, MenuSelectHdl));

        // BUILD THE TRIGGER BUTTON
        mpMenuBtn = VclPtr<PushButton>::Create(this, WB_TABSTOP);
        mpMenuBtn->SetText(u"File Menu \u25BE"_ustr);
        mpMenuBtn->SetPosSizePixel(Point(10, 10), Size(100, 30));
        mpMenuBtn->SetClickHdl(LINK(this, CatalogMainWindow, MenuBtnClickHdl));
        mpMenuBtn->Show();

        // BUILD THE TABS
        mpTabs = VclPtr<TabControl>::Create(this);
        mpTabs->SetPosSizePixel(Point(10, 50), Size(780, 540));

        // BUILD THE WIZARD DEMO LAUNCHER
        mpRoadmapWizardDemoBtn = VclPtr<PushButton>::Create(this, WB_TABSTOP);
        mpRoadmapWizardDemoBtn->SetText(u"Launch RoadmapWizard..."_ustr);
        mpRoadmapWizardDemoBtn->SetPosSizePixel(Point(120, 10), Size(180, 30));
        mpRoadmapWizardDemoBtn->SetClickHdl(
            LINK(this, CatalogMainWindow, OnRoadmapWizardDemoClick));
        mpRoadmapWizardDemoBtn->Show();

        // 1. Buttons Tab
        mpTabs->InsertPage(1, "Buttons");
        mpBtnPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(1, mpBtnPage.get());

        mpTestBtn = VclPtr<PushButton>::Create(mpBtnPage.get(), WB_TABSTOP);
        mpTestBtn->SetText("Coordinate Test");
        mpTestBtn->SetPosSizePixel(Point(20, 20), Size(150, 30));
        mpTestBtn->Show();

        // 2. Inputs Tab
        mpTabs->InsertPage(2, "Inputs");
        mpInpPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(2, mpInpPage.get());

        mpTestEdit = VclPtr<Edit>::Create(mpInpPage.get(), WB_BORDER | WB_TABSTOP);
        mpTestEdit->SetText("LogicSize Test (gjpqy)");
        Size aOptSize = mpTestEdit->GetOptimalSize();
        mpTestEdit->SetPosSizePixel(Point(20, 20), Size(200, aOptSize.Height()));
        mpTestEdit->Show();

        // The Canvas
        mpCanvas = VclPtr<EditDrawCanvas>::Create(mpInpPage.get(), mpTestEdit);
        mpCanvas->SetPosSizePixel(Point(20, 60), Size(400, 300));
        mpCanvas->Show();

        // 3. ScrollBars Tab
        mpTabs->InsertPage(3, "ScrollBars");
        mpScrollPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(3, mpScrollPage.get());

        // Vertical ScrollBar configuration
        mpVertScroll = VclPtr<ScrollBar>::Create(mpScrollPage.get(), WB_VERT | WB_DRAG);
        mpVertScroll->SetPosSizePixel(Point(340, 20), Size(20, 200));
        mpVertScroll->SetRangeMin(0);
        mpVertScroll->SetRangeMax(500);
        mpVertScroll->SetVisibleSize(200);
        mpVertScroll->SetLineSize(10);
        mpVertScroll->SetPageSize(50);
        mpVertScroll->SetThumbPos(0);
        mpVertScroll->SetScrollHdl(LINK(this, CatalogMainWindow, OnScrollEvent));
        mpVertScroll->Show();

        // Horizontal ScrollBar configuration
        mpHorzScroll = VclPtr<ScrollBar>::Create(mpScrollPage.get(), WB_HORZ | WB_DRAG);
        mpHorzScroll->SetPosSizePixel(Point(20, 230), Size(300, 20));
        mpHorzScroll->SetRangeMin(0);
        mpHorzScroll->SetRangeMax(1000);
        mpHorzScroll->SetVisibleSize(300);
        mpHorzScroll->SetLineSize(20);
        mpHorzScroll->SetPageSize(100);
        mpHorzScroll->SetThumbPos(0);
        mpHorzScroll->SetScrollHdl(LINK(this, CatalogMainWindow, OnScrollEvent));
        mpHorzScroll->Show();

        // Tracking Context Label
        mpScrollStatus = VclPtr<FixedText>::Create(mpScrollPage.get(), WB_CENTER);
        mpScrollStatus->SetPosSizePixel(Point(20, 100), Size(300, 30));
        mpScrollStatus->SetText(u"Scroll Tracking - X: 0, Y: 0"_ustr);
        mpScrollStatus->Show();

        // 4. SpinFields Tab (spinfld.cxx)
        mpTabs->InsertPage(4, "SpinFields");
        mpSpinPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(4, mpSpinPage.get());

        mpTestSpinField
            = VclPtr<SpinField>::Create(mpSpinPage.get(), WB_BORDER | WB_TABSTOP | WB_SPIN);
        mpTestSpinField->SetPosSizePixel(Point(20, 20), Size(120, 30));
        mpTestSpinField->SetText(u"10"_ustr);
        mpTestSpinField->SetUpHdl(LINK(this, CatalogMainWindow, OnSpinUp));
        mpTestSpinField->SetDownHdl(LINK(this, CatalogMainWindow, OnSpinDown));
        mpTestSpinField->Show();

        mpSpinStatus = VclPtr<FixedText>::Create(mpSpinPage.get(), WB_LEFT);
        mpSpinStatus->SetPosSizePixel(Point(160, 25), Size(200, 20));
        mpSpinStatus->SetText(u"Current Value: 10"_ustr);
        mpSpinStatus->Show();

        // 5. TabControls Tab (tabctrl.cxx)
        mpTabs->InsertPage(5, "TabControls");
        mpTabCtrlPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(5, mpTabCtrlPage.get());

        mpNestedTabCtrl = VclPtr<TabControl>::Create(mpTabCtrlPage.get());
        mpNestedTabCtrl->SetPosSizePixel(Point(20, 20), Size(400, 250));

        mpNestedTabCtrl->InsertPage(1, "Nested Alpha");
        mpNestedPage1 = VclPtr<TabPage>::Create(mpNestedTabCtrl.get());
        mpNestedTabCtrl->SetTabPage(1, mpNestedPage1.get());
        {
            auto pNestedLabel1 = VclPtr<FixedText>::Create(mpNestedPage1.get(), WB_CENTER);
            pNestedLabel1->SetText(u"Inside Tab Page Alpha"_ustr);
            pNestedLabel1->SetPosSizePixel(Point(20, 50), Size(360, 20));
            pNestedLabel1->Show();
        }

        mpNestedTabCtrl->InsertPage(2, "Nested Beta");
        mpNestedPage2 = VclPtr<TabPage>::Create(mpNestedTabCtrl.get());
        mpNestedTabCtrl->SetTabPage(2, mpNestedPage2.get());
        {
            auto pNestedLabel2 = VclPtr<FixedText>::Create(mpNestedPage2.get(), WB_CENTER);
            pNestedLabel2->SetText(u"Inside Tab Page Beta"_ustr);
            pNestedLabel2->SetPosSizePixel(Point(20, 50), Size(360, 20));
            pNestedLabel2->Show();
        }
        mpNestedTabCtrl->Show();

        // 6. VclMultiLineEdits Tab (vclmedit.cxx)
        mpTabs->InsertPage(6, "VclMultiLineEdits");
        mpMEditPage = VclPtr<TabPage>::Create(mpTabs.get());
        mpTabs->SetTabPage(6, mpMEditPage.get());

        mpVclMultiLineEdit = VclPtr<VclMultiLineEdit>::Create(mpMEditPage.get(),
                                                              WB_BORDER | WB_VSCROLL | WB_TABSTOP);
        mpVclMultiLineEdit->SetPosSizePixel(Point(20, 20), Size(500, 200));
        mpVclMultiLineEdit->SetText(
            u"Line 1: VCL VclMultiLineEdit test bed.\nLine 2: Supports raw line breaks.\nLine 3: Scrollbars active."_ustr);
        mpVclMultiLineEdit->Show();

        mpTabs->Show();
    }

    virtual void StateChanged(StateChangedType nType) override
    {
        WorkWindow::StateChanged(nType);

        if (nType == StateChangedType::InitShow)
        {
            if (mpMenuBtn)
                mpMenuBtn->Invalidate();
            if (mpRoadmapWizardDemoBtn)
                mpRoadmapWizardDemoBtn->Invalidate();

            this->Invalidate(InvalidateFlags::Children);
            this->PaintImmediately();
        }
    }

    virtual void dispose() override
    {
        mpVclMultiLineEdit.disposeAndClear();
        mpMEditPage.disposeAndClear();

        mpNestedPage2.disposeAndClear();
        mpNestedPage1.disposeAndClear();
        mpNestedTabCtrl.disposeAndClear();
        mpTabCtrlPage.disposeAndClear();

        mpSpinStatus.disposeAndClear();
        mpTestSpinField.disposeAndClear();
        mpSpinPage.disposeAndClear();

        mpScrollStatus.disposeAndClear();
        mpHorzScroll.disposeAndClear();
        mpVertScroll.disposeAndClear();
        mpScrollPage.disposeAndClear();

        mpCanvas.disposeAndClear();
        mpTestEdit.disposeAndClear();
        mpInpPage.disposeAndClear();
        mpTestBtn.disposeAndClear();
        mpBtnPage.disposeAndClear();
        mpTabs.disposeAndClear();
        mpFileMenu.disposeAndClear();
        mpRoadmapWizardDemoBtn.disposeAndClear();
        WorkWindow::dispose();
    }

    DECL_LINK(MenuSelectHdl, Menu*, bool);
    DECL_LINK(MenuBtnClickHdl, Button*, void);
    DECL_LINK(OnRoadmapWizardDemoClick, Button*, void);
    DECL_LINK(OnWizPrevClick, Button*, void);
    DECL_LINK(OnWizNextClick, Button*, void);
    DECL_LINK(OnScrollEvent, ScrollBar*, void);
    DECL_LINK(OnSpinUp, SpinField&, void);
    DECL_LINK(OnSpinDown, SpinField&, void);
};

IMPL_LINK(CatalogMainWindow, MenuSelectHdl, Menu*, pMenu, bool)
{
    if (pMenu->GetCurItemId() == 1)
    {
        Application::Quit();
        return true;
    }

    return false;
}

IMPL_LINK(CatalogMainWindow, MenuBtnClickHdl, Button*, pButton, void)
{
    tools::Rectangle aBtnRect(pButton->GetPosPixel(), pButton->GetSizePixel());
    mpFileMenu->Execute(this, aBtnRect, PopupMenuFlags::ExecuteDown);
}

IMPL_LINK_NOARG(CatalogMainWindow, OnWizPrevClick, Button*, void)
{
    if (mnWizardStep > 0)
    {
        mnWizardStep--;
        SyncWizardNavigation();
    }
}

IMPL_LINK_NOARG(CatalogMainWindow, OnWizNextClick, Button*, void)
{
    if (mnWizardStep < 2)
    {
        mnWizardStep++;
        SyncWizardNavigation();
    }
    else if (mpActiveWizard)
    {
        mpActiveWizard->EndDialog(RET_OK);
    }
}

IMPL_LINK_NOARG(CatalogMainWindow, OnScrollEvent, ScrollBar*, void)
{
    if (!mpHorzScroll || !mpVertScroll || !mpScrollStatus)
        return;

    tools::Long nX = mpHorzScroll->GetThumbPos();
    tools::Long nY = mpVertScroll->GetThumbPos();

    OUString sStatus = u"Scroll Tracking - X: "_ustr + OUString::number(nX) + u", Y: "_ustr
                       + OUString::number(nY);
    mpScrollStatus->SetText(sStatus);
}

IMPL_LINK(CatalogMainWindow, OnSpinUp, SpinField&, rField, void)
{
    if (!mpSpinStatus)
        return;

    OUString sVal = rField.GetText();
    sal_Int32 nVal = sVal.toInt32() + 1;

    OUString sNewVal = OUString::number(nVal);
    rField.SetText(sNewVal);
    mpSpinStatus->SetText(u"Current Value: "_ustr + sNewVal);
}

IMPL_LINK(CatalogMainWindow, OnSpinDown, SpinField&, rField, void)
{
    if (!mpSpinStatus)
        return;

    OUString sVal = rField.GetText();
    sal_Int32 nVal = sVal.toInt32() - 1;

    OUString sNewVal = OUString::number(nVal);
    rField.SetText(sNewVal);
    mpSpinStatus->SetText(u"Current Value: "_ustr + sNewVal);
}

IMPL_LINK_NOARG(CatalogMainWindow, OnRoadmapWizardDemoClick, Button*, void)
{
    ScopedVclPtrInstance<vcl::RoadmapWizard> pWizard(this, WB_CLOSEABLE | WB_MOVEABLE | WB_SIZEABLE,
                                                     vcl::RoadmapWizard::InitFlag::Default);
    pWizard->SetText(u"VCL Catalog: RoadmapWizard Demo"_ustr);
    pWizard->set_id(u"roadmap_wizard_demo"_ustr);

    pWizard->DeleteRoadmapItems();
    pWizard->InsertRoadmapItem(0, u"Introduction"_ustr, 0, true);
    pWizard->InsertRoadmapItem(1, u"Configuration"_ustr, 1, true);
    pWizard->InsertRoadmapItem(2, u"Summary"_ustr, 2, true);
    pWizard->ShowRoadmap(true);

    Size aTargetPageSize(400, 300);
    pWizard->SetPageSizePixel(aTargetPageSize);

    // Initialize layout tracking components
    mpActiveWizard = pWizard.get();
    mnWizardStep = 0;

    mpWizPages[0] = VclPtr<TabPage>::Create(pWizard.get(), WB_NOTABSTOP);
    mpWizPages[0]->SetSizePixel(aTargetPageSize);
    auto pLabel0 = VclPtr<FixedText>::Create(mpWizPages[0].get(), WB_CENTER);
    pLabel0->SetText(u"Step 1: Welcome to the VCL Roadmap Pipeline Layout Testbed."_ustr);
    pLabel0->SetPosSizePixel(Point(20, 80), Size(360, 30));
    pLabel0->Show();

    mpWizPages[1] = VclPtr<TabPage>::Create(pWizard.get(), WB_NOTABSTOP);
    mpWizPages[1]->SetSizePixel(aTargetPageSize);
    auto pLabel1 = VclPtr<FixedText>::Create(mpWizPages[1].get(), WB_CENTER);
    pLabel1->SetText(u"Step 2: Modify geometric parameters or check invariant traits."_ustr);
    pLabel1->SetPosSizePixel(Point(20, 80), Size(360, 30));
    pLabel1->Show();

    mpWizPages[2] = VclPtr<TabPage>::Create(pWizard.get(), WB_NOTABSTOP);
    mpWizPages[2]->SetSizePixel(aTargetPageSize);
    auto pLabel2 = VclPtr<FixedText>::Create(mpWizPages[2].get(), WB_CENTER);
    pLabel2->SetText(u"Step 3: Synthesis complete. Review execution artifacts."_ustr);
    pLabel2->SetPosSizePixel(Point(20, 80), Size(360, 30));
    pLabel2->Show();

    pWizard->AddPage(mpWizPages[0].get());
    pWizard->AddPage(mpWizPages[1].get());
    pWizard->AddPage(mpWizPages[2].get());

    // Traverse the child controls to hijack navigation clicks away from the unseeded machine stack
    vcl::Window* pChild = pWizard->GetWindow(GetWindowType::FirstChild);
    while (pChild)
    {
        if (pChild->GetType() == WindowType::PUSHBUTTON
            || pChild->GetType() == WindowType::OKBUTTON)
        {
            PushButton* pBtn = static_cast<PushButton*>(pChild);
            OUString sId = pBtn->get_id();

            if (sId == "previous")
            {
                pBtn->SetClickHdl(LINK(this, CatalogMainWindow, OnWizPrevClick));
            }
            else if (sId == "next" || sId == "finish")
            {
                pBtn->SetClickHdl(LINK(this, CatalogMainWindow, OnWizNextClick));
            }
        }
        pChild = pChild->GetWindow(GetWindowType::Next);
    }

    pWizard->SetOutputSizePixel(pWizard->GetMinOutputSizePixel());
    SyncWizardNavigation();

    pWizard->Execute();

    // Secure cleanup sequence
    mpWizPages[0].disposeAndClear();
    mpWizPages[1].disposeAndClear();
    mpWizPages[2].disposeAndClear();
    mpActiveWizard = nullptr;
}

class VclCatalogApp : public Application
{
public:
    VclCatalogApp()
    {
        try
        {
            css::uno::Reference<css::uno::XComponentContext> xContext
                = cppu::defaultBootstrap_InitialComponentContext();
            css::uno::Reference<css::lang::XMultiServiceFactory> xServiceManager(
                xContext->getServiceManager(), css::uno::UNO_QUERY);
            comphelper::setProcessServiceFactory(xServiceManager);
        }
        catch (...)
        {
        }
    }

    virtual int Main() override
    {
        ScopedVclPtrInstance<CatalogMainWindow> pMainWindow;
        pMainWindow->Show();
        pMainWindow->GrabFocus();

        Execute();

        return 0;
    }
};

void vclmain::createApplication() { static VclCatalogApp aApp; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
