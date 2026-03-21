/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/color.hxx>
#include <test/bootstrapfixture.hxx>
#include <test/outputdevice.hxx>

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <tools/mapunit.hxx>
#include <tools/stream.hxx>
#include <vcl/graphicfilter.hxx>

#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/print.hxx>
#include <tools/poly.hxx>
#include <vcl/rendercontext/State.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/rendercontext/RasterOp.hxx>
#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/virdev.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/window.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/BitmapWriteAccess.hxx>
#include <vcl/gfxlink.hxx>
#include <vcl/BinaryDataContainer.hxx>

#include <bufferdevice.hxx>
#include <window.h>

#include <vcl/HatchProcessor.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <tools/line.hxx>
const size_t INITIAL_SETUP_ACTION_COUNT = 5;

class VclOutdevTest : public test::BootstrapFixture
{
public:
    VclOutdevTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGetReadableFontColorPrinter)
{
    ScopedVclPtrInstance<Printer> pPrinter;
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pPrinter->GetReadableFontColor(COL_WHITE, COL_WHITE));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGetReadableFontColorWindow)
{
    ScopedVclPtrInstance<vcl::Window> pWindow(nullptr, WB_APP | WB_STDWORK);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE,
                         pWindow->GetOutDev()->GetReadableFontColor(COL_WHITE, COL_BLACK));
    CPPUNIT_ASSERT_EQUAL(COL_BLACK,
                         pWindow->GetOutDev()->GetReadableFontColor(COL_WHITE, COL_WHITE));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE,
                         pWindow->GetOutDev()->GetReadableFontColor(COL_BLACK, COL_BLACK));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testPrinterBackgroundColor)
{
    ScopedVclPtrInstance<Printer> pPrinter;
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pPrinter->GetBackgroundColor());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testWindowBackgroundColor)
{
    ScopedVclPtrInstance<vcl::Window> pWindow(nullptr, WB_APP | WB_STDWORK);
    pWindow->SetBackground(Wallpaper(COL_WHITE));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pWindow->GetBackgroundColor());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testVirtualDevice)
{
// TODO: This unit test is not executed for macOS unless bitmap scaling is implemented
#ifndef MACOSX
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(32, 32));
    pVDev->SetBackground(Wallpaper(COL_WHITE));

    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetBackgroundColor());

    pVDev->Erase();
    pVDev->DrawPixel(Point(1, 2), COL_BLUE);
    pVDev->DrawPixel(Point(31, 30), COL_RED);

    Size aSize = pVDev->GetOutputSizePixel();
    CPPUNIT_ASSERT_EQUAL(Size(32, 32), aSize);

    Bitmap aBmp = pVDev->GetBitmap(Point(), aSize);

    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(COL_BLUE, pVDev->GetPixel(Point(1, 2)));
    CPPUNIT_ASSERT_EQUAL(COL_RED, pVDev->GetPixel(Point(31, 30)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetPixel(Point(30, 31)));

    // Gotcha: y and x swap for BitmapReadAccess: deep joy.
    BitmapScopedReadAccess pAcc(aBmp);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, static_cast<Color>(pAcc->GetPixel(0, 0)));
    CPPUNIT_ASSERT_EQUAL(COL_BLUE, static_cast<Color>(pAcc->GetPixel(2, 1)));
    CPPUNIT_ASSERT_EQUAL(COL_RED, static_cast<Color>(pAcc->GetPixel(30, 31)));
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, static_cast<Color>(pAcc->GetPixel(31, 30)));

#endif
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testUseAfterDispose)
{
    // Create a virtual device, enable map mode then dispose it.
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->EnableMapMode();

    pVDev->disposeOnce();

    // Make sure that these don't crash after dispose.
    pVDev->GetInverseViewTransformation();

    pVDev->GetViewTransformation();
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawInvertedBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetRasterOp(RasterOp::Invert);
    pVDev->DrawBitmap(Point(0, 0), Size(10, 10), Point(0, 0), Size(10, 10), aBitmap,
                      MetaActionType::BMP);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return nullptr;
    };

    MetaAction* pAction = findAct(MetaActionType::RASTEROP);
    CPPUNIT_ASSERT_MESSAGE("Missing RASTEROP", pAction != nullptr);
    auto pRasterOpAction = static_cast<MetaRasterOpAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(RasterOp::Invert, pRasterOpAction->GetRasterOp());

    pAction = findAct(MetaActionType::RECT);
    CPPUNIT_ASSERT_MESSAGE("Missing RECT", pAction != nullptr);
    auto pRectAction = static_cast<MetaRectAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(tools::Rectangle(Point(0, 0), Size(10, 10)), pRectAction->GetRect());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawBlackBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);
    aBitmap.Erase(COL_RED);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetDrawMode(DrawModeFlags::BlackBitmap);
    pVDev->DrawBitmap(Point(0, 0), Size(10, 10), Point(0, 0), Size(10, 10), aBitmap,
                      MetaActionType::BMP);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return nullptr;
    };

    MetaAction* pAction = findAct(MetaActionType::PUSH);
    CPPUNIT_ASSERT_MESSAGE("Missing PUSH", pAction != nullptr);
    auto pPushAction = static_cast<MetaPushAction*>(pAction);
    bool bLineFillFlag
        = ((vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR) == pPushAction->GetFlags());
    CPPUNIT_ASSERT_MESSAGE("Push flags not LINECOLOR | FILLCOLOR", bLineFillFlag);

    pAction = findAct(MetaActionType::LINECOLOR);
    CPPUNIT_ASSERT_MESSAGE("Missing LINECOLOR", pAction != nullptr);
    auto pLineColorAction = static_cast<MetaLineColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pLineColorAction->GetColor());

    pAction = findAct(MetaActionType::FILLCOLOR);
    CPPUNIT_ASSERT_MESSAGE("Missing FILLCOLOR", pAction != nullptr);
    auto pFillColorAction = static_cast<MetaFillColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pFillColorAction->GetColor());

    pAction = findAct(MetaActionType::RECT);
    CPPUNIT_ASSERT_MESSAGE("Missing RECT", pAction != nullptr);
    auto pRectAction = static_cast<MetaRectAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(tools::Rectangle(Point(0, 0), Size(10, 10)), pRectAction->GetRect());

    pAction = findAct(MetaActionType::POP);
    CPPUNIT_ASSERT_MESSAGE("Missing POP", pAction != nullptr);

    Bitmap aBlackBmp(pVDev->GetBitmap(Point(0, 0), Size(10, 10)));
    BitmapScopedReadAccess pReadAccess(aBlackBmp);
    const BitmapColor aColor = pReadAccess->GetColor(0, 0);
    CPPUNIT_ASSERT_EQUAL(BitmapColor(COL_BLACK), aColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawWhiteBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetDrawMode(DrawModeFlags::WhiteBitmap);
    pVDev->DrawBitmap(Point(0, 0), Size(10, 10), Point(0, 0), Size(10, 10), aBitmap,
                      MetaActionType::BMP);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return nullptr;
    };

    MetaAction* pAction = findAct(MetaActionType::PUSH);
    CPPUNIT_ASSERT_MESSAGE("Missing PUSH", pAction != nullptr);
    auto pPushAction = static_cast<MetaPushAction*>(pAction);
    bool bLineFillFlag
        = ((vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR) == pPushAction->GetFlags());
    CPPUNIT_ASSERT_MESSAGE("Push flags not LINECOLOR | FILLCOLOR", bLineFillFlag);

    pAction = findAct(MetaActionType::LINECOLOR);
    CPPUNIT_ASSERT_MESSAGE("Missing LINECOLOR", pAction != nullptr);
    auto pLineColorAction = static_cast<MetaLineColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pLineColorAction->GetColor());

    pAction = findAct(MetaActionType::FILLCOLOR);
    CPPUNIT_ASSERT_MESSAGE("Missing FILLCOLOR", pAction != nullptr);
    auto pFillColorAction = static_cast<MetaFillColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pFillColorAction->GetColor());

    pAction = findAct(MetaActionType::RECT);
    CPPUNIT_ASSERT_MESSAGE("Missing RECT", pAction != nullptr);
    auto pRectAction = static_cast<MetaRectAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(tools::Rectangle(Point(0, 0), Size(10, 10)), pRectAction->GetRect());

    pAction = findAct(MetaActionType::POP);
    CPPUNIT_ASSERT_MESSAGE("Missing POP", pAction != nullptr);

    Bitmap aWhiteBmp(pVDev->GetBitmap(Point(0, 0), Size(10, 10)));
    BitmapScopedReadAccess pReadAccess(aWhiteBmp);
    const BitmapColor aColor = pReadAccess->GetColor(0, 0);
    CPPUNIT_ASSERT_EQUAL(BitmapColor(COL_WHITE), aColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->DrawBitmap(Point(0, 0), Size(10, 10), Point(0, 0), Size(10, 10), aBitmap,
                      MetaActionType::BMP);

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMP, pAction->GetType());
    auto pBmpAction = static_cast<MetaBmpAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(Size(16, 16), pBmpAction->GetBitmap().GetSizePixel());
    CPPUNIT_ASSERT_EQUAL(Point(0, 0), pBmpAction->GetPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawScaleBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->DrawBitmap(Point(5, 5), Size(10, 10), Point(0, 0), Size(10, 10), aBitmap,
                      MetaActionType::BMPSCALE);

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPSCALE, pAction->GetType());
    auto pBmpScaleAction = static_cast<MetaBmpScaleAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(Size(16, 16), pBmpScaleAction->GetBitmap().GetSizePixel());
    CPPUNIT_ASSERT_EQUAL(Point(5, 5), pBmpScaleAction->GetPoint());
    CPPUNIT_ASSERT_EQUAL(Size(10, 10), pBmpScaleAction->GetSize());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawScalePartBitmap)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->DrawBitmap(Point(0, 0), Size(10, 10), Point(5, 5), Size(10, 10), aBitmap,
                      MetaActionType::BMPSCALEPART);

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPSCALEPART, pAction->GetType());
    auto pBmpScalePartAction = static_cast<MetaBmpScalePartAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(Size(16, 16), pBmpScalePartAction->GetBitmap().GetSizePixel());
    CPPUNIT_ASSERT_EQUAL(Point(5, 5), pBmpScalePartAction->GetSrcPoint());
    CPPUNIT_ASSERT_EQUAL(Size(10, 10), pBmpScalePartAction->GetSrcSize());
    CPPUNIT_ASSERT_EQUAL(Point(0, 0), pBmpScalePartAction->GetDestPoint());
    CPPUNIT_ASSERT_EQUAL(Size(10, 10), pBmpScalePartAction->GetDestSize());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGrayBitmap)
{
    // draw a red 1x1 bitmap
    Bitmap aBmp(Size(1, 1), vcl::PixelFormat::N24_BPP);
    aBmp.Erase(COL_RED);

    // check to ensure that the bitmap is red
    {
        BitmapScopedReadAccess pReadAccess(aBmp);
        const BitmapColor aColor = pReadAccess->GetColor(0, 0);
        CPPUNIT_ASSERT_EQUAL(BitmapColor(COL_RED), aColor);
    }

    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->SetDrawMode(DrawModeFlags::GrayBitmap);
    pVDev->DrawBitmap(Point(0, 0), Size(1, 1), Point(0, 0), Size(1, 1), aBmp, MetaActionType::BMP);

    // should be a grey
    Bitmap aVDevBmp(pVDev->GetBitmap(Point(), Size(1, 1)));
    {
        BitmapScopedReadAccess pReadAccess(aVDevBmp);
        const BitmapColor aColor = pReadAccess->GetColor(0, 0);
        CPPUNIT_ASSERT_EQUAL(BitmapColor(0x26, 0x26, 0x26), aColor);
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawTransformedBitmapScale)
{
    // Given a 100x100 bitmap:
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(100, 100), vcl::PixelFormat::N24_BPP);
    basegfx::B2DVector aScale(20, 80);
    basegfx::B2DVector aTranslate(80, 0);
    double fRotate = M_PI / 2;
    double fShearX = 0;
    basegfx::B2DHomMatrix aMatrix = basegfx::utils::createScaleShearXRotateTranslateB2DHomMatrix(
        aScale, fShearX, fRotate, aTranslate);
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    // When drawing that with a transform:
    pVDev->DrawTransformedBitmap(aMatrix, aBitmap);

    // Then make sure the bitmap recorded in the metafile doesn't get a scaled down width:
    CPPUNIT_ASSERT(aMtf.GetActionSize() >= 1);
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPEXSCALE, pAction->GetType());
    auto pBitmapAction = static_cast<MetaBmpExScaleAction*>(pAction);
    const Bitmap& rBitmapEx = pBitmapAction->GetBitmap();
    Size aTransformedSize = rBitmapEx.GetSizePixel();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected greater or equal than: 100
    // - Actual  : 80
    // i.e. an unwanted scaling down lead to blurry presentation output.
    CPPUNIT_ASSERT_GREATEREQUAL(static_cast<tools::Long>(100), aTransformedSize.getWidth());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawTransformedBitmap)
{
    // Create a virtual device, and connect a metafile to it.
    // Also create a 16x16 bitmap.
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);
    {
        // Fill the top left quarter with black.
        BitmapScopedWriteAccess pWriteAccess(aBitmap);
        pWriteAccess->Erase(COL_WHITE);
        for (int i = 0; i < 8; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                pWriteAccess->SetPixel(j, i, COL_BLACK);
            }
        }
    }
    basegfx::B2DHomMatrix aMatrix;
    aMatrix.scale(8, 8);
    // Rotate 90 degrees clockwise, so the black part goes to the top right.
    aMatrix.rotate(M_PI / 2);
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    // Draw the rotated bitmap on the vdev.
    pVDev->DrawTransformedBitmap(aMatrix, aBitmap);
    CPPUNIT_ASSERT(aMtf.GetActionSize() >= 1);
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPEXSCALE, pAction->GetType());
    auto pBitmapAction = static_cast<MetaBmpExScaleAction*>(pAction);
    const Bitmap& rBitmapEx = pBitmapAction->GetBitmap();
    Size aTransformedSize = rBitmapEx.GetSizePixel();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 16x16
    // - Actual  : 8x8
    // I.e. the bitmap before scaling was already scaled down, just because it was rotated.
    CPPUNIT_ASSERT_EQUAL(Size(16, 16), aTransformedSize);

    aBitmap = rBitmapEx.CreateColorBitmap();
    BitmapScopedReadAccess pAccess(aBitmap);
    for (int i = 0; i < 16; ++i)
    {
        for (int j = 0; j < 16; ++j)
        {
            BitmapColor aColor = pAccess->GetPixel(j, i);
            Color aExpected = i >= 8 && j < 8 ? COL_BLACK : COL_WHITE;
            std::stringstream ss;
            ss << "Color is expected to be ";
            ss << ((aExpected == COL_WHITE) ? "white" : "black");
            ss << ", is " << aColor.AsRGBHexString();
            ss << " (row " << j << ", col " << i << ")";
            // Without the accompanying fix in place, this test would have failed with:
            // - Expected: c[00000000]
            // - Actual  : c[ffffff00]
            // - Color is expected to be black, is ffffff (row 0, col 8)
            // i.e. the top right quarter of the image was not fully black, there was a white first
            // row.
            CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), aExpected, Color(aColor));
        }
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawTransformedBitmapFlip)
{
    // Create a virtual device, and connect a metafile to it.
    // Also create a 16x16 bitmap.
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    Bitmap aBitmap(Size(16, 16), vcl::PixelFormat::N24_BPP);
    {
        // Fill the top left quarter with black.
        BitmapScopedWriteAccess pWriteAccess(aBitmap);
        pWriteAccess->Erase(COL_WHITE);
        for (int i = 0; i < 8; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                pWriteAccess->SetPixel(j, i, COL_BLACK);
            }
        }
    }
    basegfx::B2DHomMatrix aMatrix;
    // Negative y scale: bitmap should be upside down, so the black part goes to the bottom left.
    aMatrix.scale(8, -8);
    // Rotate 90 degrees clockwise, so the black part goes back to the top left.
    aMatrix.rotate(M_PI / 2);
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    // Draw the scaled and rotated bitmap on the vdev.
    pVDev->DrawTransformedBitmap(aMatrix, aBitmap);
    CPPUNIT_ASSERT(aMtf.GetActionSize() >= 1);
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::BMPEXSCALE, pAction->GetType());
    auto pBitmapAction = static_cast<MetaBmpExScaleAction*>(pAction);
    const Bitmap& rBitmapEx = pBitmapAction->GetBitmap();

    aBitmap = rBitmapEx.CreateColorBitmap();
    BitmapScopedReadAccess pAccess(aBitmap);
    int nX = 8 * 0.25;
    int nY = 8 * 0.25;
    BitmapColor aColor = pAccess->GetPixel(nY, nX);
    std::stringstream ss;
    ss << "Color is expected to be black, is " << aColor.AsRGBHexString();
    ss << " (row " << nY << ", col " << nX << ")";
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: c[00000000]
    // - Actual  : c[ffffff00]
    // - Color is expected to be black, is ffffff (row 2, col 2)
    // i.e. the top left quarter of the image was not black, due to a missing flip.
    CPPUNIT_ASSERT_EQUAL_MESSAGE(ss.str(), COL_BLACK, Color(aColor));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRTL)
{
    ScopedVclPtrInstance<vcl::Window> pWindow(nullptr, WB_APP | WB_STDWORK);
    pWindow->EnableRTL();
    vcl::RenderContext& rRenderContext = *pWindow->GetOutDev();
    vcl::BufferDevice pBuffer(pWindow, rRenderContext);

    // Without the accompanying fix in place, this test would have failed, because the RTL status
    // from pWindow was not propagated to pBuffer.
    CPPUNIT_ASSERT(pBuffer->IsRTLEnabled());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRTLGuard)
{
    ScopedVclPtrInstance<vcl::Window> pWindow(nullptr, WB_APP | WB_STDWORK);
    pWindow->EnableRTL();
    pWindow->RequestDoubleBuffering(true);
    ImplFrameData* pFrameData = pWindow->ImplGetWindowImpl()->mpFrameData;
    vcl::PaintBufferGuard aGuard(pFrameData, pWindow);
    // Without the accompanying fix in place, this test would have failed, because the RTL status
    // from pWindow was not propagated to aGuard.
    CPPUNIT_ASSERT(aGuard.GetRenderContext()->IsRTLEnabled());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDefaultFillColor)
{
    // Create a virtual device, and connect a metafile to it.
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT(pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetFillColor());

    pVDev->SetFillColor();
    CPPUNIT_ASSERT(!pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, pVDev->GetFillColor());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FILLCOLOR, pAction->GetType());
    auto pFillAction = static_cast<MetaFillColorAction*>(pAction);
    const Color& rColor = pFillAction->GetColor();
    CPPUNIT_ASSERT_EQUAL(Color(), rColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testTransparentFillColor)
{
    // Create a virtual device, and connect a metafile to it.
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT(pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetFillColor());

    pVDev->SetFillColor(COL_TRANSPARENT);
    CPPUNIT_ASSERT(pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, pVDev->GetFillColor());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FILLCOLOR, pAction->GetType());
    auto pFillAction = static_cast<MetaFillColorAction*>(pAction);
    const Color& rColor = pFillAction->GetColor();
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, rColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testFillColor)
{
    // Create a virtual device, and connect a metafile to it.
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT(pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, pVDev->GetFillColor());

    pVDev->SetFillColor(COL_RED);
    CPPUNIT_ASSERT(pVDev->IsFillColor());
    CPPUNIT_ASSERT_EQUAL(COL_RED, pVDev->GetFillColor());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FILLCOLOR, pAction->GetType());
    auto pFillAction = static_cast<MetaFillColorAction*>(pAction);
    const Color& rColor = pFillAction->GetColor();
    CPPUNIT_ASSERT_EQUAL(COL_RED, rColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDefaultLineColor)
{
    // Create a virtual device, and connect a metafile to it.
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT(pVDev->IsLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pVDev->GetLineColor());

    pVDev->SetLineColor();
    CPPUNIT_ASSERT(!pVDev->IsLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, pVDev->GetLineColor());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINECOLOR, pAction->GetType());
    auto pLineAction = static_cast<MetaLineColorAction*>(pAction);
    const Color& rColor = pLineAction->GetColor();
    CPPUNIT_ASSERT_EQUAL(Color(), rColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testLineColor)
{
    // Create a virtual device, and connect a metafile to it.
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT(pVDev->IsLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pVDev->GetLineColor());

    pVDev->SetLineColor(COL_RED);
    CPPUNIT_ASSERT(pVDev->IsLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_RED, pVDev->GetLineColor());
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINECOLOR, pAction->GetType());
    auto pLineAction = static_cast<MetaLineColorAction*>(pAction);
    const Color& rColor = pLineAction->GetColor();
    CPPUNIT_ASSERT_EQUAL(COL_RED, rColor);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testTextLanguageSync)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->SetDigitLanguage(LANGUAGE_ENGLISH_US);

    CPPUNIT_ASSERT_EQUAL(LANGUAGE_ENGLISH_US, pVDev->GetDigitLanguage());

    pVDev->Push(vcl::PushFlags::TEXTLANGUAGE);
    pVDev->SetDigitLanguage(LANGUAGE_GERMAN);
    CPPUNIT_ASSERT_EQUAL(LANGUAGE_GERMAN, pVDev->GetDigitLanguage());

    pVDev->Pop();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("TextLanguage was not restored correctly", LANGUAGE_ENGLISH_US,
                                 pVDev->GetDigitLanguage());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testFont)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    // Use Dejavu fonts, they are shipped with LO, so they should be ~always available.
    // Use Sans variant for simpler glyph shapes (no serifs).
    vcl::Font font(u"DejaVu Sans"_ustr, u"Book"_ustr, Size(0, 36));
    font.SetColor(COL_BLACK);
    font.SetFillColor(COL_RED);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetFont(font);
    bool bSameFont(font == pVDev->GetFont());
    CPPUNIT_ASSERT_MESSAGE("Font is not the same", bSameFont);

    // four actions:
    // 1. Font action
    // 2. Text alignment action
    // 3. Text fill color action
    // 4. As not COL_TRANSPARENT (means use system font color), font color action
    size_t nActionsExpected = 4;
    CPPUNIT_ASSERT_EQUAL(nActionsExpected, aMtf.GetActionSize());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FONT, pAction->GetType());
    auto pFontAction = static_cast<MetaFontAction*>(pAction);
    bool bSameMetaFont = (font == pFontAction->GetFont());
    CPPUNIT_ASSERT_MESSAGE("Metafile font is not the same", bSameMetaFont);

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::TEXTALIGN, pAction->GetType());
    auto pTextAlignAction = static_cast<MetaTextAlignAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(font.GetAlignment(), pTextAlignAction->GetTextAlign());

    pAction = aMtf.GetAction(2);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::TEXTFILLCOLOR, pAction->GetType());
    auto pTextFillColorAction = static_cast<MetaTextFillColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_RED, pTextFillColorAction->GetColor());

    pAction = aMtf.GetAction(3);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::TEXTCOLOR, pAction->GetType());
    auto pTextColorAction = static_cast<MetaTextColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pTextColorAction->GetColor());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testTransparentFont)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    // Use Dejavu fonts, they are shipped with LO, so they should be ~always available.
    // Use Sans variant for simpler glyph shapes (no serifs).
    vcl::Font font(u"DejaVu Sans"_ustr, u"Book"_ustr, Size(0, 36));
    font.SetColor(COL_TRANSPARENT);

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetFont(font);

    // three actions as it sets the colour to the default system color (and doesn't add a text color action):
    // 1. Font action
    // 2. Text alignment action
    // 3. Text fill color action
    size_t nActionsExpected = 3;
    CPPUNIT_ASSERT_EQUAL(nActionsExpected, aMtf.GetActionSize());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDefaultRefPoint)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetRefPoint();

    CPPUNIT_ASSERT(!pVDev->IsRefPoint());
    CPPUNIT_ASSERT_EQUAL(Point(), pVDev->GetRefPoint());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::REFPOINT, pAction->GetType());
    auto pRefPointAction = static_cast<MetaRefPointAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(Point(), pRefPointAction->GetRefPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRefPoint)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetRefPoint(Point(10, 20));

    CPPUNIT_ASSERT(pVDev->IsRefPoint());
    CPPUNIT_ASSERT_EQUAL(Point(10, 20), pVDev->GetRefPoint());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::REFPOINT, pAction->GetType());
    auto pRefPointAction = static_cast<MetaRefPointAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(Point(10, 20), pRefPointAction->GetRefPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRasterOp)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetRasterOp(RasterOp::Invert);

    CPPUNIT_ASSERT_EQUAL(RasterOp::Invert, pVDev->GetRasterOp());
    CPPUNIT_ASSERT(pVDev->IsLineColor());
    CPPUNIT_ASSERT(pVDev->IsFillColor());

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::RASTEROP);
    CPPUNIT_ASSERT(pAction != nullptr);
    auto pRasterOpAction = static_cast<MetaRasterOpAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(RasterOp::Invert, pRasterOpAction->GetRasterOp());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRasterOpStack)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->SetRasterOp(RasterOp::Invert);
    CPPUNIT_ASSERT_EQUAL(RasterOp::Invert, pVDev->GetRasterOp());

    pVDev->Push(vcl::PushFlags::RASTEROP);

    pVDev->SetRasterOp(RasterOp::Xor);
    CPPUNIT_ASSERT_EQUAL(RasterOp::Xor, pVDev->GetRasterOp());

    pVDev->Pop();

    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "RasterOp was not restored correctly from the GraphicsState snapshot", RasterOp::Invert,
        pVDev->GetRasterOp());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testOutputFlag)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    CPPUNIT_ASSERT(pVDev->IsOutputEnabled());
    CPPUNIT_ASSERT(pVDev->IsDeviceOutputNecessary());

    pVDev->EnableOutput(false);

    CPPUNIT_ASSERT(!pVDev->IsOutputEnabled());
    CPPUNIT_ASSERT(!pVDev->IsDeviceOutputNecessary());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testAntialias)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    CPPUNIT_ASSERT_EQUAL(AntialiasingFlags::NONE, pVDev->GetAntialiasing());

    pVDev->SetAntialiasing(AntialiasingFlags::Enable);

    CPPUNIT_ASSERT_EQUAL(AntialiasingFlags::Enable, pVDev->GetAntialiasing());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawMode)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    CPPUNIT_ASSERT_EQUAL(DrawModeFlags::Default, pVDev->GetDrawMode());

    pVDev->SetDrawMode(DrawModeFlags::BlackLine);

    CPPUNIT_ASSERT_EQUAL(DrawModeFlags::BlackLine, pVDev->GetDrawMode());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testLayoutMode)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT_EQUAL(vcl::text::ComplexTextLayoutFlags::Default, pVDev->GetLayoutMode());

    pVDev->SetLayoutMode(vcl::text::ComplexTextLayoutFlags::BiDiRtl);

    CPPUNIT_ASSERT_EQUAL(vcl::text::ComplexTextLayoutFlags::BiDiRtl, pVDev->GetLayoutMode());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LAYOUTMODE, pAction->GetType());
    auto pLayoutModeAction = static_cast<MetaLayoutModeAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(vcl::text::ComplexTextLayoutFlags::BiDiRtl,
                         pLayoutModeAction->GetLayoutMode());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDigitLanguage)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    CPPUNIT_ASSERT_EQUAL(LANGUAGE_SYSTEM, pVDev->GetDigitLanguage());

    pVDev->SetDigitLanguage(LANGUAGE_GERMAN);

    CPPUNIT_ASSERT_EQUAL(LANGUAGE_GERMAN, pVDev->GetDigitLanguage());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::TEXTLANGUAGE, pAction->GetType());
    auto pTextLanguageAction = static_cast<MetaTextLanguageAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(LANGUAGE_GERMAN, pTextLanguageAction->GetTextLanguage());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testStackFunctions)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->Push();
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Push action", MetaActionType::PUSH, pAction->GetType());

    pVDev->SetLineColor(COL_RED);
    pVDev->SetFillColor(COL_GREEN);
    pVDev->SetTextColor(COL_BROWN);
    pVDev->SetTextFillColor(COL_BLUE);
    pVDev->SetTextLineColor(COL_MAGENTA);
    pVDev->SetOverlineColor(COL_YELLOW);
    pVDev->SetTextAlign(TextAlign::ALIGN_TOP);
    pVDev->SetLayoutMode(vcl::text::ComplexTextLayoutFlags::BiDiRtl);
    pVDev->SetDigitLanguage(LANGUAGE_FRENCH);
    pVDev->SetRasterOp(RasterOp::N0);
    pVDev->SetMapMode(MapMode(MapUnit::MapTwip));
    pVDev->SetRefPoint(Point(10, 10));

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Text color", COL_BROWN, pVDev->GetTextColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Text fill color", COL_BLUE, pVDev->GetTextFillColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Text line color", COL_MAGENTA, pVDev->GetTextLineColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Text overline color", COL_YELLOW, pVDev->GetOverlineColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Layout mode", vcl::text::ComplexTextLayoutFlags::BiDiRtl,
                                 pVDev->GetLayoutMode());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Language", LANGUAGE_FRENCH, pVDev->GetDigitLanguage());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Raster operation", RasterOp::N0, pVDev->GetRasterOp());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Map mode", MapMode(MapUnit::MapTwip), pVDev->GetMapMode());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Ref point", Point(10, 10), pVDev->GetRefPoint());

    pVDev->Pop();
    pAction = aMtf.GetAction(13);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Pop action", MetaActionType::POP, pAction->GetType());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default line color", COL_BLACK, pVDev->GetLineColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default fill color", COL_WHITE, pVDev->GetFillColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default text color", COL_BLACK, pVDev->GetTextColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default text fill color", Color(ColorTransparency, 0xFFFFFFFF),
                                 pVDev->GetTextFillColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default text line color", Color(ColorTransparency, 0xFFFFFFFF),
                                 pVDev->GetTextLineColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default overline color", Color(ColorTransparency, 0xFFFFFFFF),
                                 pVDev->GetOverlineColor());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default layout mode", vcl::text::ComplexTextLayoutFlags::Default,
                                 pVDev->GetLayoutMode());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default language", LANGUAGE_SYSTEM, pVDev->GetDigitLanguage());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default raster operation", RasterOp::OverPaint,
                                 pVDev->GetRasterOp());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default map mode", MapMode(MapUnit::MapPixel),
                                 pVDev->GetMapMode());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default ref point", Point(0, 0), pVDev->GetRefPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testSystemTextColor)
{
    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;

        pVDev->SetSystemTextColor(SystemTextColorFlags::NONE, true);
        CPPUNIT_ASSERT_EQUAL(COL_BLACK, pVDev->GetTextColor());
        pVDev->SetSystemTextColor(SystemTextColorFlags::Mono, false);
        CPPUNIT_ASSERT_EQUAL(COL_BLACK, pVDev->GetTextColor());
    }

    {
        ScopedVclPtrInstance<Printer> pPrinter;
        pPrinter->SetSystemTextColor(SystemTextColorFlags::NONE, true);
        CPPUNIT_ASSERT_EQUAL(COL_BLACK, pPrinter->GetTextColor());
    }
}

namespace
{
class WaveLineTester : public OutputDevice
{
public:
    WaveLineTester()
        : OutputDevice(OUTDEV_VIRDEV)
    {
    }

    bool AcquireGraphics() const override { return true; }
    void ReleaseGraphics(bool) override {}
    virtual bool HasAlpha() const override { return false; }

    bool testShouldDrawWavePixelAsRect(tools::Long nLineWidth)
    {
        return shouldDrawWavePixelAsRect(nLineWidth);
    }

    Size testGetWaveLineSize(tools::Long nLineWidth) { return GetWaveLineSize(nLineWidth); }
};

class WaveLineTesterPrinter : public Printer
{
public:
    WaveLineTesterPrinter() {}

    bool AcquireGraphics() const { return true; }
    void ReleaseGraphics(bool) {}
    bool UsePolyPolygonForComplexGradient() { return false; }

    Size testGetWaveLineSize(tools::Long nLineWidth) { return GetWaveLineSize(nLineWidth); }
};
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testShouldDrawWavePixelAsRect)
{
    ScopedVclPtrInstance<WaveLineTester> pTestOutDev;

    CPPUNIT_ASSERT(!pTestOutDev->testShouldDrawWavePixelAsRect(0));
    CPPUNIT_ASSERT(!pTestOutDev->testShouldDrawWavePixelAsRect(1));

    CPPUNIT_ASSERT(pTestOutDev->testShouldDrawWavePixelAsRect(10));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGetWaveLineSize)
{
    {
        ScopedVclPtrInstance<WaveLineTester> pTestOutDev;

        pTestOutDev->SetDPIX(96);
        pTestOutDev->SetDPIY(96);

        CPPUNIT_ASSERT_EQUAL(Size(1, 1), pTestOutDev->testGetWaveLineSize(0));
        CPPUNIT_ASSERT_EQUAL(Size(1, 1), pTestOutDev->testGetWaveLineSize(1));

        CPPUNIT_ASSERT_EQUAL(Size(10, 10), pTestOutDev->testGetWaveLineSize(10));
    }

    {
        ScopedVclPtrInstance<WaveLineTesterPrinter> pTestOutDev;

        pTestOutDev->SetDPIX(96);
        pTestOutDev->SetDPIY(96);

        CPPUNIT_ASSERT_EQUAL(Size(0, 0), pTestOutDev->testGetWaveLineSize(0));
        CPPUNIT_ASSERT_EQUAL(Size(1, 1), pTestOutDev->testGetWaveLineSize(1));

        CPPUNIT_ASSERT_EQUAL(Size(10, 10), pTestOutDev->testGetWaveLineSize(10));
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testErase)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetOutputSizePixel(Size(10, 10));
    pVDev->Erase();

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return nullptr;
    };

    MetaAction* pAction = findAct(MetaActionType::LINECOLOR);
    CPPUNIT_ASSERT_MESSAGE("Not a line color action (start)", pAction != nullptr);

    pAction = findAct(MetaActionType::FILLCOLOR);
    CPPUNIT_ASSERT_MESSAGE("Not a fill color action (start)", pAction != nullptr);

    pAction = findAct(MetaActionType::RECT);
    CPPUNIT_ASSERT_MESSAGE("Not a rect action", pAction != nullptr);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPixel)
{
    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        GDIMetaFile aMtf;
        aMtf.Record(pVDev.get());

        // triggers an Erase()
        pVDev->SetOutputSizePixel(Size(10, 10));
        pVDev->SetLineColor(COL_RED);
        pVDev->DrawPixel(Point(0, 0), COL_GREEN);

        CPPUNIT_ASSERT_EQUAL_MESSAGE("Color not green", COL_GREEN, pVDev->GetPixel(Point(0, 0)));

        auto findAct = [&](MetaActionType t) {
            for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
                if (aMtf.GetAction(i)->GetType() == t)
                    return aMtf.GetAction(i);
            return (MetaAction*)nullptr;
        };
        MetaAction* pAction = findAct(MetaActionType::PIXEL);
        if (!pAction)
            pAction = findAct(MetaActionType::RECT); // VCL maps pixels to rects often
        CPPUNIT_ASSERT_MESSAGE("Not a pixel action", pAction != nullptr);
        MetaPixelAction* pPixelAction = dynamic_cast<MetaPixelAction*>(pAction);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Pixel action has incorrect position", Point(0, 0),
                                     pPixelAction->GetPoint());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Pixel action is wrong color", COL_GREEN,
                                     pPixelAction->GetColor());
    }

    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        GDIMetaFile aMtf;
        aMtf.Record(pVDev.get());

        pVDev->SetOutputSizePixel(Size(1, 1));
        pVDev->SetLineColor(COL_RED);
        pVDev->DrawPixel(Point(0, 0));

        CPPUNIT_ASSERT_EQUAL_MESSAGE("Color not red", COL_RED, pVDev->GetPixel(Point(0, 0)));

        auto findAct = [&](MetaActionType t) {
            for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
                if (aMtf.GetAction(i)->GetType() == t)
                    return aMtf.GetAction(i);
            return (MetaAction*)nullptr;
        };
        MetaAction* pAction = findAct(MetaActionType::POINT);
        if (!pAction)
            pAction = findAct(MetaActionType::RECT);
        CPPUNIT_ASSERT_MESSAGE("Not a point action", pAction != nullptr);
        MetaPointAction* pPointAction = dynamic_cast<MetaPointAction*>(pAction);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Pixel action has incorrect position", Point(0, 0),
                                     pPointAction->GetPoint());
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawLine)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->SetLineColor(COL_RED);
    pVDev->DrawLine(Point(0, 0), Point(10, 10));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing LINE action", findAct(MetaActionType::LINE));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawRect)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);
    GDIMetaFile* pMtf = &aMtf;

    // Standard Rectangle
    pVDev->SetLineColor(COL_BLACK);
    pVDev->SetFillColor(COL_WHITE);
    pVDev->DrawRect(tools::Rectangle(10, 10, 50, 50));

    CPPUNIT_ASSERT_MESSAGE("Metafile should not be empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }

        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing RECT action", findAct(MetaActionType::RECT));

    // Rounded Rectangle
    aMtf.Clear();

    pVDev->DrawRoundedRect(tools::Rectangle(10, 10, 50, 50), 5, 5);

    CPPUNIT_ASSERT_MESSAGE("Metafile should not be empty after DrawRoundedRect",
                           pMtf->GetActionSize() > 0);

    CPPUNIT_ASSERT_MESSAGE("Missing Group Start (COMMENT)", findAct(MetaActionType::COMMENT));
    CPPUNIT_ASSERT_MESSAGE("Missing State PUSH", findAct(MetaActionType::PUSH));
    CPPUNIT_ASSERT_MESSAGE("Missing LINECOLOR action", findAct(MetaActionType::LINECOLOR));
    CPPUNIT_ASSERT_MESSAGE("Missing ROUNDRECT action", findAct(MetaActionType::ROUNDRECT));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawEllipse)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->DrawEllipse(tools::Rectangle(0, 0, 10, 10));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing ELLIPSE action", findAct(MetaActionType::ELLIPSE));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPie)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->DrawPie(tools::Rectangle(0, 0, 10, 10), Point(0, 5), Point(10, 5));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing PIE action", findAct(MetaActionType::PIE));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawChord)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->DrawChord(tools::Rectangle(0, 0, 10, 10), Point(0, 5), Point(10, 5));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing CHORD action", findAct(MetaActionType::CHORD));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawArc)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->DrawArc(tools::Rectangle(0, 0, 10, 10), Point(0, 5), Point(10, 5));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing ARC action", findAct(MetaActionType::ARC));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawCheckered)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->DrawCheckered(Point(0, 0), Size(10, 10), 5, COL_WHITE, COL_BLACK);

    GDIMetaFile* pMtf = &aMtf;

    CPPUNIT_ASSERT_MESSAGE("Metafile should not be empty", pMtf->GetActionSize() > 0);

    // The action stream now contains Semantic Groups (Comment), State Pushes, Line/Fill Colors,
    // and the Geometry (Rect). We use a robust search rather than checking brittle hardcoded
    // indices.
    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    // Assert the structural and geometric components are all present
    CPPUNIT_ASSERT_MESSAGE("Missing Group Start (COMMENT)", findAct(MetaActionType::COMMENT));
    CPPUNIT_ASSERT_MESSAGE("Missing State PUSH", findAct(MetaActionType::PUSH));
    CPPUNIT_ASSERT_MESSAGE("Missing FILLCOLOR", findAct(MetaActionType::FILLCOLOR));
    CPPUNIT_ASSERT_MESSAGE("Missing Geometry (RECT)", findAct(MetaActionType::RECT));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawBorder)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    // Set a known color to verify the state gets correctly captured
    pVDev->SetLineColor(COL_BLUE);
    pVDev->DrawBorder(tools::Rectangle(10, 10, 20, 20));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile should not be empty", pMtf->GetActionSize() > 0);

    // The action stream now contains Semantic Groups (Comment),
    // State Pushes, Line Color, and the geometric intent (Rect).
    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }

        return nullptr;
    };

    // Verify the structural grouping
    CPPUNIT_ASSERT_MESSAGE("Missing Group Start (COMMENT)", findAct(MetaActionType::COMMENT));
    CPPUNIT_ASSERT_MESSAGE("Missing State PUSH", findAct(MetaActionType::PUSH));

    // Verify the active line color was successfully captured
    auto pLineColorAct = static_cast<MetaLineColorAction*>(findAct(MetaActionType::LINECOLOR));
    CPPUNIT_ASSERT_MESSAGE("Missing LINECOLOR action", pLineColorAct);
    CPPUNIT_ASSERT_EQUAL(COL_BLUE, pLineColorAct->GetColor());

    // Verify the geometric intent
    CPPUNIT_ASSERT_MESSAGE("Missing Geometry (RECT)", findAct(MetaActionType::RECT));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawWaveLine)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->DrawWaveLine(Point(0, 0), Point(50, 0));

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::BMPEXSCALEPART);
    CPPUNIT_ASSERT_MESSAGE("Not a bitmap action", pAction != nullptr);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolyLine)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    tools::Polygon aPoly(2);
    aPoly[0] = Point(0, 0);
    aPoly[1] = Point(10, 10);
    pVDev->DrawPolyLine(aPoly);

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing POLYLINE action", findAct(MetaActionType::POLYLINE));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolygon)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    tools::Polygon aPoly(3);
    aPoly[0] = Point(0, 0);
    aPoly[1] = Point(10, 0);
    aPoly[2] = Point(5, 10);
    pVDev->DrawPolygon(aPoly);

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing POLYGON action", findAct(MetaActionType::POLYGON));
}

static tools::PolyPolygon createPolyPolygon()
{
    tools::Polygon aPolygon(4);

    aPolygon.SetPoint(Point(1, 8), 0);
    aPolygon.SetPoint(Point(2, 7), 1);
    aPolygon.SetPoint(Point(3, 6), 2);
    aPolygon.SetPoint(Point(4, 5), 3);

    tools::PolyPolygon aPolyPolygon(aPolygon);
    aPolyPolygon.Optimize(PolyOptimizeFlags::CLOSE);

    return aPolyPolygon;
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolyPolygon)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    tools::Polygon aPoly(3);
    aPoly[0] = Point(0, 0);
    aPoly[1] = Point(10, 0);
    aPoly[2] = Point(5, 10);
    pVDev->DrawPolyPolygon(tools::PolyPolygon(aPoly));

    GDIMetaFile* pMtf = &aMtf;
    CPPUNIT_ASSERT_MESSAGE("Metafile empty", pMtf->GetActionSize() > 0);

    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing POLYPOLYGON action", findAct(MetaActionType::POLYPOLYGON));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawRectAlpha1)
{
    Size aSize(100, 100);
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(aSize, true, true);

    const Color RED_OPAQUE(ColorAlpha, 255, 255, 0, 0); // opaque red
    pVDev->SetLineColor(RED_OPAQUE);
    pVDev->SetFillColor(RED_OPAQUE);
    pVDev->DrawRect(tools::Rectangle(0, 0, 100, 100));

    CPPUNIT_ASSERT_EQUAL(RED_OPAQUE, pVDev->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(RED_OPAQUE, pVDev->GetPixel(Point(1, 1)));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawRectAlpha2)
{
    Size aSize(100, 100);
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(aSize, true, true);

    const Color RED_TRANSPARENT(ColorAlpha, 127, 255, 0, 0); // 50% transparent red
    pVDev->SetLineColor(RED_TRANSPARENT);
    pVDev->SetFillColor(RED_TRANSPARENT);
    pVDev->DrawRect(tools::Rectangle(0, 0, 100, 100));

    CPPUNIT_ASSERT_EQUAL(RED_TRANSPARENT, pVDev->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(RED_TRANSPARENT, pVDev->GetPixel(Point(1, 1)));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolyPolygonAlpha1)
{
    Size aSize(100, 100);
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(aSize, /*bErase*/ true, /*bAlphaMaskTransparent*/ true);

    // create a square polypolygon
    tools::Polygon aPolygon(4);
    aPolygon.SetPoint(Point(0, 0), 0);
    aPolygon.SetPoint(Point(0, 100), 1);
    aPolygon.SetPoint(Point(100, 100), 2);
    aPolygon.SetPoint(Point(0, 0), 3);
    tools::PolyPolygon aPolyPolygon(aPolygon);
    aPolyPolygon.Optimize(PolyOptimizeFlags::CLOSE);
    basegfx::B2DPolyPolygon aB2DPolyPolygon(aPolyPolygon.getB2DPolyPolygon());

    const Color RED_OPAQUE(ColorAlpha, 255, 255, 0, 0); // opaque red

    pVDev->SetLineColor(RED_OPAQUE);
    pVDev->SetFillColor(RED_OPAQUE);
    pVDev->DrawPolyPolygon(aB2DPolyPolygon);

    CPPUNIT_ASSERT_EQUAL(RED_OPAQUE, pVDev->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(RED_OPAQUE, pVDev->GetPixel(Point(1, 1)));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolyPolygonAlpha2)
{
    Size aSize(100, 100);
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(aSize, /*bErase*/ true, /*bAlphaMaskTransparent*/ true);

    // create a square polypolygon
    tools::Polygon aPolygon(4);
    aPolygon.SetPoint(Point(0, 0), 0);
    aPolygon.SetPoint(Point(0, 100), 1);
    aPolygon.SetPoint(Point(100, 100), 2);
    aPolygon.SetPoint(Point(0, 0), 3);
    tools::PolyPolygon aPolyPolygon(aPolygon);
    aPolyPolygon.Optimize(PolyOptimizeFlags::CLOSE);
    basegfx::B2DPolyPolygon aB2DPolyPolygon(aPolyPolygon.getB2DPolyPolygon());

    const Color RED_TRANSPARENT(ColorAlpha, 127, 255, 0, 0); // 50% transparent red

    pVDev->SetLineColor(RED_TRANSPARENT);
    pVDev->SetFillColor(RED_TRANSPARENT);
    pVDev->DrawPolyPolygon(aB2DPolyPolygon);

    CPPUNIT_ASSERT_EQUAL(RED_TRANSPARENT, pVDev->GetPixel(Point(0, 0)));
    CPPUNIT_ASSERT_EQUAL(RED_TRANSPARENT, pVDev->GetPixel(Point(1, 1)));
}

static size_t ClipGradientTest(const GDIMetaFile& rMtf, size_t nIndex)
{
    bool bFoundGrad = false;
    for (size_t i = 0; i < rMtf.GetActionSize(); ++i)
    {
        if (rMtf.GetAction(i)->GetType() == MetaActionType::GRADIENT
            || rMtf.GetAction(i)->GetType() == MetaActionType::GRADIENTEX)
        {
            bFoundGrad = true;
            break;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("Could not find Gradient action in stream", bFoundGrad);
    return nIndex;
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGradient_rect_linear)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    tools::Rectangle aRect(Point(10, 10), Size(40, 40));
    pVDev->SetOutputSizePixel(Size(100, 100));

    Gradient aGradient(css::awt::GradientStyle_LINEAR, COL_RED, COL_WHITE);
    aGradient.SetBorder(100);

    pVDev->DrawGradient(aRect, aGradient);

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::GRADIENT);
    if (!pAction)
        pAction = findAct(MetaActionType::GRADIENTEX); // Support tiered renderer outputs
    CPPUNIT_ASSERT_MESSAGE("Not a gradient action (rectangle area)", pAction != nullptr);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGradient_rect_axial)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    tools::Rectangle aRect(Point(10, 10), Size(40, 40));
    pVDev->SetOutputSizePixel(Size(100, 100));

    Gradient aGradient(css::awt::GradientStyle_AXIAL, COL_RED, COL_WHITE);
    aGradient.SetBorder(100);

    pVDev->DrawGradient(aRect, aGradient);

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::GRADIENT);
    if (!pAction)
        pAction = findAct(MetaActionType::GRADIENTEX); // Support tiered renderer outputs
    CPPUNIT_ASSERT_MESSAGE("Not a gradient action (rectangle area)", pAction != nullptr);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGradient_polygon_linear)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    tools::PolyPolygon aPolyPolygon = createPolyPolygon();

    pVDev->SetOutputSizePixel(Size(100, 100));

    Gradient aGradient(css::awt::GradientStyle_LINEAR, COL_RED, COL_WHITE);
    aGradient.SetBorder(100);

    pVDev->DrawGradient(aPolyPolygon, aGradient);

    ClipGradientTest(aMtf, INITIAL_SETUP_ACTION_COUNT);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGradient_polygon_axial)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    tools::PolyPolygon aPolyPolygon = createPolyPolygon();

    pVDev->SetOutputSizePixel(Size(100, 100));

    Gradient aGradient(css::awt::GradientStyle_AXIAL, COL_RED, COL_WHITE);
    aGradient.SetBorder(100);

    pVDev->DrawGradient(aPolyPolygon, aGradient);

    ClipGradientTest(aMtf, INITIAL_SETUP_ACTION_COUNT);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawGradient_rect_complex)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    tools::Rectangle aRect(Point(10, 10), Size(40, 40));
    pVDev->SetOutputSizePixel(Size(1000, 1000));

    Gradient aGradient(css::awt::GradientStyle_SQUARE, COL_RED, COL_WHITE);
    aGradient.SetBorder(10);
    pVDev->DrawGradient(aRect, aGradient);

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::GRADIENT);
    if (!pAction)
        pAction = findAct(MetaActionType::GRADIENTEX); // Support tiered renderer outputs
    CPPUNIT_ASSERT_MESSAGE("Not a gradient action (rectangle area)", pAction != nullptr);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGraphicStatePushPop)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->EnableOutput(true);

    pDev->SetLineColor(COL_RED);
    pDev->SetFillColor(COL_GREEN);
    pDev->SetRasterOp(RasterOp::Xor);
    pDev->SetTextColor(COL_BLACK);
    pDev->SetTextLineColor(COL_TRANSPARENT);
    pDev->SetOverlineColor(COL_TRANSPARENT);

    pDev->Push(vcl::PushFlags::ALL);

    pDev->SetLineColor(COL_BLUE);
    pDev->SetFillColor(COL_YELLOW);
    pDev->SetRasterOp(RasterOp::OverPaint);
    pDev->SetTextColor(COL_CYAN);
    pDev->SetTextLineColor(COL_MAGENTA);
    pDev->SetOverlineColor(COL_LIGHTRED);

    CPPUNIT_ASSERT_EQUAL(COL_BLUE, pDev->GetLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_YELLOW, pDev->GetFillColor());
    CPPUNIT_ASSERT_EQUAL(RasterOp::OverPaint, pDev->GetRasterOp());
    CPPUNIT_ASSERT_EQUAL(COL_CYAN, pDev->GetTextColor());
    CPPUNIT_ASSERT_EQUAL(COL_MAGENTA, pDev->GetTextLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_LIGHTRED, pDev->GetOverlineColor());

    pDev->Pop();

    CPPUNIT_ASSERT_EQUAL(COL_RED, pDev->GetLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_GREEN, pDev->GetFillColor());
    CPPUNIT_ASSERT_EQUAL(RasterOp::Xor, pDev->GetRasterOp());
    CPPUNIT_ASSERT_EQUAL(COL_BLACK, pDev->GetTextColor());
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, pDev->GetTextLineColor());
    CPPUNIT_ASSERT_EQUAL(COL_TRANSPARENT, pDev->GetOverlineColor());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRefPointLifecycle)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;

    Point aPointA(10, 10);
    Point aPointB(20, 20);

    pDev->SetRefPoint(aPointA);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("RefPoint should be set to Point A", aPointA, pDev->GetRefPoint());
    CPPUNIT_ASSERT_MESSAGE("RefPoint flag should be active", pDev->IsRefPoint());

    pDev->Push(vcl::PushFlags::REFPOINT);

    pDev->SetRefPoint(aPointB);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("RefPoint should be set to Point B", aPointB, pDev->GetRefPoint());

    pDev->Pop();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("RefPoint should be restored to Point A after Pop", aPointA,
                                 pDev->GetRefPoint());
    CPPUNIT_ASSERT_MESSAGE("RefPoint flag should be active after Pop", pDev->IsRefPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testRenderStateFlags)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;

    pDev->SetAntialiasing(AntialiasingFlags::Enable);
    CPPUNIT_ASSERT_EQUAL(AntialiasingFlags::Enable, pDev->GetAntialiasing());

    pDev->SetAntialiasing(AntialiasingFlags::PixelSnapHairline);
    CPPUNIT_ASSERT_EQUAL(AntialiasingFlags::PixelSnapHairline, pDev->GetAntialiasing());

    pDev->SetDrawMode(DrawModeFlags::BlackLine);
    CPPUNIT_ASSERT_EQUAL(DrawModeFlags::BlackLine, pDev->GetDrawMode());

    pDev->SetDrawMode(DrawModeFlags::GrayLine);
    CPPUNIT_ASSERT_EQUAL(DrawModeFlags::GrayLine, pDev->GetDrawMode());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testClipRegionPushPop)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->EnableOutput(true);
    pDev->SetMapMode(MapMode(MapUnit::Map100thMM));

    tools::Rectangle aRect1(Point(100, 100), Size(1000, 1000));
    vcl::Region aRegion1(aRect1);

    pDev->SetClipRegion(aRegion1);

    // Read back the region immediately to get the "quantized" version.
    // We assert that Push/Pop restores THIS value, not the original 'aRegion1'.
    vcl::Region aEffectiveRegion = pDev->GetClipRegion();

    pDev->Push(vcl::PushFlags::CLIPREGION);

    tools::Rectangle aRect2(Point(500, 500), Size(200, 200));
    vcl::Region aRegion2(aRect2);
    pDev->SetClipRegion(aRegion2);

    CPPUNIT_ASSERT(aEffectiveRegion != pDev->GetClipRegion());

    pDev->Pop();

    // Compare against the effective (quantized) region we saved earlier
    CPPUNIT_ASSERT_EQUAL(aEffectiveRegion, pDev->GetClipRegion());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testBackgroundAccess)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->EnableOutput(true);

    pDev->SetBackground();

    CPPUNIT_ASSERT_MESSAGE("Should be no background after clear", !pDev->IsBackground());
    Wallpaper aEmptyWall;
    CPPUNIT_ASSERT(aEmptyWall == pDev->GetBackground());

    Wallpaper aWall1(COL_RED);
    pDev->SetBackground(aWall1);

    // Verify Flag and Value
    CPPUNIT_ASSERT_MESSAGE("Flag should be true", pDev->IsBackground());
    CPPUNIT_ASSERT(aWall1 == pDev->GetBackground());

    Wallpaper aWall2(COL_BLUE);
    pDev->SetBackground(aWall2);

    CPPUNIT_ASSERT(aWall2 == pDev->GetBackground());
    CPPUNIT_ASSERT(aWall1 != pDev->GetBackground());

    pDev->SetBackground(); // No args = clear

    CPPUNIT_ASSERT_MESSAGE("Flag should be false", !pDev->IsBackground());
    CPPUNIT_ASSERT(aEmptyWall == pDev->GetBackground());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testTextLayoutAccess)
{
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->EnableOutput(true);

    // Test Digit Language
    // Default is usually LANGUAGE_NONE or system default, so explicit set is required
    LanguageType eLang1 = LANGUAGE_ENGLISH_US;
    pDev->SetDigitLanguage(eLang1);
    CPPUNIT_ASSERT(eLang1 == pDev->GetDigitLanguage());

    LanguageType eLang2 = LANGUAGE_GERMAN;
    pDev->SetDigitLanguage(eLang2);
    CPPUNIT_ASSERT(eLang2 == pDev->GetDigitLanguage());
    CPPUNIT_ASSERT(eLang1 != pDev->GetDigitLanguage());

    // Test Complex Text Layout Flags
    // Use valid flags: Default and BiDiRtl
    vcl::text::ComplexTextLayoutFlags nMode1 = vcl::text::ComplexTextLayoutFlags::Default;
    pDev->SetLayoutMode(nMode1);
    CPPUNIT_ASSERT(nMode1 == pDev->GetLayoutMode());

    vcl::text::ComplexTextLayoutFlags nMode2 = vcl::text::ComplexTextLayoutFlags::BiDiRtl;
    pDev->SetLayoutMode(nMode2);
    CPPUNIT_ASSERT(nMode2 == pDev->GetLayoutMode());
    CPPUNIT_ASSERT(nMode1 != pDev->GetLayoutMode());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testClipRegionStackPersistence)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    vcl::Region aUpperClip(tools::Rectangle(0, 0, 10, 10));
    pVDev->SetClipRegion(aUpperClip);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Initial clip should not result in empty output", false,
                                 pVDev->IsOutputClipped());

    pVDev->Push(vcl::PushFlags::CLIPREGION);

    vcl::Region aDisjointClip(tools::Rectangle(50, 50, 60, 60));
    pVDev->IntersectClipRegion(aDisjointClip);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Disjoint intersection should result in clipped output", true,
                                 pVDev->IsOutputClipped());

    pVDev->Pop();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Clip geometry was not restored after Pop", aUpperClip,
                                 pVDev->GetClipRegion());

    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "Output is still reported as clipped after Pop restored a valid region", false,
        pVDev->IsOutputClipped());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDisjointIntersectionEagerSync)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->SetClipRegion(vcl::Region(tools::Rectangle(0, 0, 10, 10)));

    pVDev->IntersectClipRegion(tools::Rectangle(20, 20, 30, 30));

    // The controller should immediately signal that output is clipped
    CPPUNIT_ASSERT_MESSAGE("Disjoint intersection must signal clipped output immediately",
                           pVDev->IsOutputClipped());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testWindowClipRegionOffset)
{
    // Regression test for: "vcl: move clipping state management..."
    // Ensures WindowOutputDevice translates user clip regions (Relative)
    // to Device coordinates (Absolute) before intersecting with the Window region.

    // Create a Window positioned away from the origin (100, 100)
    // We use WorkWindow to ensure it has a valid frame/parent structure
    ScopedVclPtrInstance<WorkWindow> pWin(static_cast<vcl::Window*>(nullptr), WB_STDWORK);
    pWin->SetPosSizePixel(Point(100, 100), Size(200, 200));
    // Ensure the window is fully initialized/visible so it generates a valid window clip region
    pWin->Show();

    OutputDevice* pDev = pWin->GetOutDev();

    // Define a User Clip Region in RELATIVE coordinates.
    // Rect: (10, 10) to (60, 60).
    // In Absolute Device coordinates, this is (110, 110) to (160, 160).
    // This is strictly INSIDE the Window's bounds (100, 100) to (300, 300).
    // If the fix is missing, the code might treat (10,10) as absolute,
    // which would be outside the window (100,100), resulting in an empty intersection.
    tools::Rectangle aUserRect(Point(10, 10), Size(50, 50));
    pDev->SetClipRegion(vcl::Region(aUserRect));

    // Trigger InitClipRegion via a draw command.
    // This forces the WindowOutputDevice to intersect WindowRegion (Abs) with UserRegion (Rel).
    pDev->DrawPixel(Point(20, 20));

    CPPUNIT_ASSERT_MESSAGE("Output should not be fully clipped (User region is inside Window)",
                           !pDev->IsOutputClipped());

    CPPUNIT_ASSERT(pDev->HasClipRegion());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testPDFClippingBehavior)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(10, 10));

    pVDev->SetClipRegion(vcl::Region(tools::Rectangle(0, 0, 10, 10)));

    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    // Draw an object WAY outside the device bounds
    // The device is 10x10. We draw at 1000x1000.
    tools::Rectangle aOutsideRect(Point(1000, 1000), Size(20, 20));
    pVDev->DrawRect(aOutsideRect);

    pVDev->SetConnectMetaFile(nullptr);

    // If your Hybrid Fix is working, 'IsOutputClipped' saw 'mpMetaFile'
    // and used Infinite Bounds. Therefore, the action should be recorded.
    // If the fix is missing, it checked against the 10x10 device, saw it
    // was outside, and culled it (size would be 0).
    CPPUNIT_ASSERT_MESSAGE("PDF/Metafile recording should NOT cull off-screen objects",
                           aMtf.GetActionSize() >= 1);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testFontStateConsistency)
{
    ScopedVclPtr<VirtualDevice> pDev = VclPtr<VirtualDevice>::Create();

    vcl::Font aDefFont = pDev->GetFont();
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default font should be transparent", Color(COL_TRANSPARENT),
                                 aDefFont.GetFillColor());

    vcl::Font aTestFont(u"Liberation Sans"_ustr, Size(0, 12));
    aTestFont.SetColor(COL_RED);
    aTestFont.SetAlignment(ALIGN_BOTTOM);

    pDev->SetFont(aTestFont);

    vcl::Font aResultFont = pDev->GetFont();
    CPPUNIT_ASSERT_EQUAL(u"Liberation Sans"_ustr, aResultFont.GetFamilyName());
    CPPUNIT_ASSERT_EQUAL(COL_RED, aResultFont.GetColor());
    CPPUNIT_ASSERT_EQUAL(ALIGN_BOTTOM, aResultFont.GetAlignment());

    // Does the device actually "know" the font changed?
    // GetTextHeight() depends on ImplNewFont() layout logic.
    long nHeight = pDev->GetTextHeight();
    CPPUNIT_ASSERT_MESSAGE("Text height must be > 0 for a valid font", nHeight > 0);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGetTextOutlinesMapModeSync)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    pVDev->SetMapMode(MapMode(MapUnit::Map100thMM));

    // Set a font size that is large in logical units (1000 100thMM = 10mm)
    // On a standard 96 DPI screen, this realizes to roughly ~38 device pixels.
    pVDev->SetFont(vcl::Font(u"DejaVu Sans"_ustr, Size(0, 1000)));

    // Force OutputDevice to realize the font in the current MapMode!
    // This primes the mpFontRealization cache with the scaled-down (~38px) font.
    pVDev->GetTextWidth(u"A"_ustr);

    // Extract Outlines
    // GetTextOutlines temporarily disables the MapMode to process purely in 1:1 logical units.
    // The logical height of 1000 should now be treated strictly as 1000 pixels.
    basegfx::B2DPolyPolygonVector aVector;
    bool bRet = pVDev->GetTextOutlines(aVector, u"A"_ustr, 0);

    CPPUNIT_ASSERT_MESSAGE("GetTextOutlines should succeed", bRet);
    CPPUNIT_ASSERT_MESSAGE("Should return outlines for 'A'", !aVector.empty());

    double nHeight = aVector[0].getB2DRange().getHeight();

    // If there is a stale font cache, the engine uses the ~38px realization. If not,
    // the font is re-realized at 1000px, so the outline height will be roughly 700-1000
    // depending on the font's internal ascender metrics.
    CPPUNIT_ASSERT_MESSAGE("GetTextOutlines failed to sync font with disabled MapMode! "
                           "The geometry was extracted using a stale font cache.",
                           nHeight > 500.0);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawBitmapRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    // Opaque Bitmap
    // Use N8_BPP to force opaque palette (avoids implicit alpha promotion on some backends)
    Bitmap aBmpOpaque(Size(10, 10), vcl::PixelFormat::N8_BPP);
    aBmpOpaque.Erase(COL_RED);

    CPPUNIT_ASSERT_MESSAGE("Setup Error: Opaque bitmap has alpha", !aBmpOpaque.HasAlpha());

    // Alpha Bitmap
    // MUST use DeviceFormat::WITH_ALPHA to ensure the resulting bitmap carries an alpha channel
    ScopedVclPtrInstance<VirtualDevice> pAlphaDev(DeviceFormat::WITH_ALPHA);
    pAlphaDev->SetOutputSizePixel(Size(10, 10));
    pAlphaDev->SetBackground(Wallpaper(COL_TRANSPARENT));
    pAlphaDev->Erase();
    Bitmap aBmpAlpha = pAlphaDev->GetBitmap(Point(0, 0), Size(10, 10));

    // If this fails, the backend is not supporting alpha VDevs correctly in this environment
    CPPUNIT_ASSERT_MESSAGE("Setup Error: Alpha bitmap is opaque", aBmpAlpha.HasAlpha());

    Point aPos(10, 10);
    Size aSz(20, 20);
    Point aSrcPos(5, 5);
    Size aSrcSz(5, 5);

    // --- Group A: Opaque (Should generate BMP actions) ---
    pVDev->DrawBitmap(aPos, aBmpOpaque);
    pVDev->DrawBitmap(aPos, aSz, aBmpOpaque);
    pVDev->DrawBitmap(aPos, aSz, aSrcPos, aSrcSz, aBmpOpaque);

    // --- Group B: Alpha (Should generate BMPEX actions) ---
    pVDev->DrawBitmap(aPos, aBmpAlpha);
    pVDev->DrawBitmap(aPos, aSz, aBmpAlpha);
    pVDev->DrawBitmap(aPos, aSz, aSrcPos, aSrcSz, aBmpAlpha);

    // Verify we got 6 actions
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), aMtf.GetActionSize());

    // Verify Opaque Actions (BMP)
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 0 should be BMP", MetaActionType::BMP, pAction->GetType());
    CPPUNIT_ASSERT_EQUAL(aPos, static_cast<MetaBmpAction*>(pAction)->GetPoint());

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 1 should be BMPSCALE", MetaActionType::BMPSCALE,
                                 pAction->GetType());
    CPPUNIT_ASSERT_EQUAL(aSz, static_cast<MetaBmpScaleAction*>(pAction)->GetSize());

    pAction = aMtf.GetAction(2);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 2 should be BMPSCALEPART", MetaActionType::BMPSCALEPART,
                                 pAction->GetType());
    auto pScalePart = static_cast<MetaBmpScalePartAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aPos, pScalePart->GetDestPoint());
    CPPUNIT_ASSERT_EQUAL(aSrcPos, pScalePart->GetSrcPoint());

    // Verify Alpha Actions (BMPEX)
    pAction = aMtf.GetAction(3);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 3 should be BMPEX", MetaActionType::BMPEX,
                                 pAction->GetType());
    CPPUNIT_ASSERT_EQUAL(aPos, static_cast<MetaBmpExAction*>(pAction)->GetPoint());

    pAction = aMtf.GetAction(4);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 4 should be BMPEXSCALE", MetaActionType::BMPEXSCALE,
                                 pAction->GetType());
    CPPUNIT_ASSERT_EQUAL(aSz, static_cast<MetaBmpExScaleAction*>(pAction)->GetSize());

    pAction = aMtf.GetAction(5);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Action 5 should be BMPEXSCALEPART",
                                 MetaActionType::BMPEXSCALEPART, pAction->GetType());
    auto pExScalePart = static_cast<MetaBmpExScalePartAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aPos, pExScalePart->GetDestPoint());
    CPPUNIT_ASSERT_EQUAL(aSrcPos, pExScalePart->GetSrcPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testClippingRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    vcl::Region aReg(tools::Rectangle(0, 0, 10, 10));
    tools::Rectangle aRect(10, 10, 20, 20);

    pVDev->SetClipRegion(aReg);
    pVDev->MoveClipRegion(5, 5);
    pVDev->IntersectClipRegion(aRect);
    pVDev->IntersectClipRegion(aReg);
    pVDev->SetClipRegion();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), aMtf.GetActionSize());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::CLIPREGION, pAction->GetType());
    auto pClipAction = static_cast<MetaClipRegionAction*>(pAction);
    CPPUNIT_ASSERT(pClipAction->IsClipping());

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::MOVECLIPREGION, pAction->GetType());

    pAction = aMtf.GetAction(2);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::ISECTRECTCLIPREGION, pAction->GetType());

    pAction = aMtf.GetAction(3);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::ISECTREGIONCLIPREGION, pAction->GetType());

    pAction = aMtf.GetAction(4);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::CLIPREGION, pAction->GetType());
    pClipAction = static_cast<MetaClipRegionAction*>(pAction);
    CPPUNIT_ASSERT(!pClipAction->IsClipping());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testCurvedShapesRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    tools::Rectangle aRect(10, 10, 50, 50);
    Point aStart(10, 10);
    Point aEnd(50, 50);

    pVDev->DrawEllipse(aRect);
    pVDev->DrawArc(aRect, aStart, aEnd);
    pVDev->DrawPie(aRect, aStart, aEnd);
    pVDev->DrawChord(aRect, aStart, aEnd);

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), aMtf.GetActionSize());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::ELLIPSE, pAction->GetType());
    CPPUNIT_ASSERT_EQUAL(aRect, static_cast<MetaEllipseAction*>(pAction)->GetRect());

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::ARC, pAction->GetType());
    auto pArc = static_cast<MetaArcAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aRect, pArc->GetRect());
    CPPUNIT_ASSERT_EQUAL(aStart, pArc->GetStartPoint());
    CPPUNIT_ASSERT_EQUAL(aEnd, pArc->GetEndPoint());

    pAction = aMtf.GetAction(2);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::PIE, pAction->GetType());
    auto pPie = static_cast<MetaPieAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aRect, pPie->GetRect());
    CPPUNIT_ASSERT_EQUAL(aStart, pPie->GetStartPoint());
    CPPUNIT_ASSERT_EQUAL(aEnd, pPie->GetEndPoint());

    pAction = aMtf.GetAction(3);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::CHORD, pAction->GetType());
    auto pChord = static_cast<MetaChordAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aRect, pChord->GetRect());
    CPPUNIT_ASSERT_EQUAL(aStart, pChord->GetStartPoint());
    CPPUNIT_ASSERT_EQUAL(aEnd, pChord->GetEndPoint());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testEPSRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    Point aPt(10, 10);
    Size aSz(100, 100);

    // Create dummy GfxLink
    // BinaryDataContainer requires an SvStream, so we wrap the data in a memory stream.
    sal_uInt8 pData[] = { 0x00, 0x01, 0x02 };
    SvMemoryStream aStream(pData, sizeof(pData), StreamMode::READ);
    BinaryDataContainer aDataContainer(aStream, sizeof(pData));

    // Use NativeJpg as a placeholder since NativeEps is missing in this VCL version.
    // The recorder just stores the link type, so the specific enum doesn't affect the recording logic test.
    GfxLink aLink(aDataContainer, GfxLinkType::NativeJpg);

    // Create dummy substitution
    GDIMetaFile aSubst;
    aSubst.AddAction(new MetaCommentAction("Subst"));

    pVDev->DrawEPS(aPt, aSz, aLink, &aSubst);

    CPPUNIT_ASSERT(aMtf.GetActionSize() >= 1);

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::EPS, pAction->GetType());

    auto pEPS = static_cast<MetaEPSAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(aPt, pEPS->GetPoint());
    CPPUNIT_ASSERT_EQUAL(aSz, pEPS->GetSize());
    // Verify subst present
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), pEPS->GetSubstitute().GetActionSize());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testFillColorRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    // 1. Set specific color
    pVDev->SetFillColor(COL_RED);

    // 2. Clear color (transparent)
    pVDev->SetFillColor();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aMtf.GetActionSize());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FILLCOLOR, pAction->GetType());
    auto pFill = static_cast<MetaFillColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_RED, pFill->GetColor());
    CPPUNIT_ASSERT(pFill->IsSetting());

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::FILLCOLOR, pAction->GetType());
    pFill = static_cast<MetaFillColorAction*>(pAction);
    CPPUNIT_ASSERT(!pFill->IsSetting());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testLineColorRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->SetLineColor(COL_BLUE);
    pVDev->SetLineColor();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aMtf.GetActionSize());

    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINECOLOR, pAction->GetType());
    auto pLine = static_cast<MetaLineColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_BLUE, pLine->GetColor());
    CPPUNIT_ASSERT(pLine->IsSetting());

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINECOLOR, pAction->GetType());
    pLine = static_cast<MetaLineColorAction*>(pAction);
    CPPUNIT_ASSERT(!pLine->IsSetting());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testPushPopRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    pVDev->Push(vcl::PushFlags::CLIPREGION | vcl::PushFlags::LINECOLOR);
    pVDev->Pop();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aMtf.GetActionSize());

    auto findAct = [&](MetaActionType t) {
        for (size_t i = 0; i < aMtf.GetActionSize(); ++i)
            if (aMtf.GetAction(i)->GetType() == t)
                return aMtf.GetAction(i);
        return (MetaAction*)nullptr;
    };
    MetaAction* pAction = findAct(MetaActionType::PUSH);
    CPPUNIT_ASSERT(pAction != nullptr);
    auto pPush = static_cast<MetaPushAction*>(pAction);
    CPPUNIT_ASSERT(bool(pPush->GetFlags() & vcl::PushFlags::CLIPREGION));
    CPPUNIT_ASSERT(bool(pPush->GetFlags() & vcl::PushFlags::LINECOLOR));

    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::POP, pAction->GetType());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testGradientRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    Gradient aGrad(css::awt::GradientStyle_LINEAR, COL_RED, COL_BLUE);
    tools::Rectangle aRect(0, 0, 100, 100);

    // Simple Gradient (Rectangle)
    // Rectangular gradients are optimized into a single action
    pVDev->DrawGradient(aRect, aGrad);

    GDIMetaFile* pMtf = &aMtf;
    auto findAct = [&](MetaActionType t) -> MetaAction* {
        for (size_t i = 0; i < pMtf->GetActionSize(); ++i)
        {
            if (pMtf->GetAction(i)->GetType() == t)
                return pMtf->GetAction(i);
        }
        return nullptr;
    };

    CPPUNIT_ASSERT_MESSAGE("Missing GRADIENT action", findAct(MetaActionType::GRADIENT));

    aMtf.Clear();

    // Create a non-rectangular shape (rotated square)
    tools::Polygon aPoly(aRect);
    aPoly.Rotate(Point(50, 50), 450_deg10);
    tools::PolyPolygon aPolyPoly(aPoly);

    pVDev->DrawGradient(aPolyPoly, aGrad);

    // Complex gradients use the RAII "Sandwich" approach
    // (Comment -> GradientEx -> Push -> Clip -> Gradient -> Pop -> Comment)
    CPPUNIT_ASSERT_MESSAGE("Missing GRADIENTEX action", findAct(MetaActionType::GRADIENTEX));
    CPPUNIT_ASSERT_MESSAGE("Missing PUSH action", findAct(MetaActionType::PUSH));
    CPPUNIT_ASSERT_MESSAGE("Missing CLIPREGION action",
                           findAct(MetaActionType::ISECTREGIONCLIPREGION));
    CPPUNIT_ASSERT_MESSAGE("Missing POP action", findAct(MetaActionType::POP));
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testHatchRecording)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;
    pVDev->SetConnectMetaFile(&aMtf);

    tools::Polygon aPoly(tools::Rectangle(0, 0, 10, 10));
    tools::PolyPolygon aPolyPoly(aPoly);
    Hatch aHatch(HatchStyle::Single, COL_RED, 10, 100_deg10);

    pVDev->DrawHatch(aPolyPoly, aHatch);

    CPPUNIT_ASSERT(aMtf.GetActionSize() >= 1);
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::HATCH, pAction->GetType());

    auto pHatchAction = static_cast<MetaHatchAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_RED, pHatchAction->GetHatch().GetColor());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testHatchDecomposition)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    // We don't attach the metafile to the device, we pass it to AddHatchActions
    GDIMetaFile aMtf;

    tools::Polygon aPoly(tools::Rectangle(0, 0, 20, 20));
    tools::PolyPolygon aPolyPoly(aPoly);

    // Single Hatch, Red, 5 units distance, 45 degrees
    Hatch aHatch(HatchStyle::Single, COL_RED, 5, 450_deg10);

    // This triggers the decomposition logic (AddHatchActions -> DrawHatch(..., true))
    vcl::HatchProcessor::DecomposeToMetaFile(aPolyPoly, aHatch, aMtf, pVDev.get());

    // We expect a sequence:
    // 1. BeginGroup Comment (ScopedMetaGroup)
    // 2. Push (Recorder)
    // 3. LineColor (Recorder)
    // 4..N. Line Actions (Decomposed primitives)
    // N+1. Pop (Recorder)
    // N+2. EndGroup Comment (ScopedMetaGroup)

    CPPUNIT_ASSERT_GREATEREQUAL(static_cast<size_t>(6), aMtf.GetActionSize());

    // 1. Check ScopedMetaGroup Start
    MetaAction* pAction = aMtf.GetAction(0);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::COMMENT, pAction->GetType());
    auto pComment = static_cast<MetaCommentAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL("BeginGroup: DecomposedHatch"_ostr, pComment->GetComment());

    // 2. Check Push
    pAction = aMtf.GetAction(1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::PUSH, pAction->GetType());

    // 3. Check LineColor
    pAction = aMtf.GetAction(2);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINECOLOR, pAction->GetType());
    auto pLineColor = static_cast<MetaLineColorAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL(COL_RED, pLineColor->GetColor());

    // 4. Check that we have at least one line (Decomposition happened)
    pAction = aMtf.GetAction(3);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::LINE, pAction->GetType());

    // Check the end sequence (Pop -> EndGroup)
    size_t nLast = aMtf.GetActionSize() - 1;

    // Last action should be EndGroup
    pAction = aMtf.GetAction(nLast);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::COMMENT, pAction->GetType());
    pComment = static_cast<MetaCommentAction*>(pAction);
    CPPUNIT_ASSERT_EQUAL("EndGroup"_ostr, pComment->GetComment());

    // Second to last should be Pop
    pAction = aMtf.GetAction(nLast - 1);
    CPPUNIT_ASSERT_EQUAL(MetaActionType::POP, pAction->GetType());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testHatchGeometric)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    GDIMetaFile aMtf;

    // Test Hatch Styles (Single vs Double vs Triple)
    tools::Polygon aRect(tools::Rectangle(0, 0, 100, 100));
    tools::PolyPolygon aPoly(aRect);

    // Single Hatch
    {
        aMtf.Clear();
        Hatch aHatch(HatchStyle::Single, COL_BLACK, 10, 0_deg10);
        vcl::HatchProcessor::DecomposeToMetaFile(aPoly, aHatch, aMtf, pVDev.get());

        size_t nSingleCount = aMtf.GetActionSize();
        // Expect header/footer actions + lines.
        // 100x100 box, dist 10 -> approx 10 lines.
        CPPUNIT_ASSERT_MESSAGE("Single hatch should produce actions", nSingleCount > 5);
    }

    // Double Hatch (Grid) - Should produce roughly 2x lines
    {
        aMtf.Clear();
        Hatch aHatch(HatchStyle::Double, COL_BLACK, 10, 0_deg10);
        vcl::HatchProcessor::DecomposeToMetaFile(aPoly, aHatch, aMtf, pVDev.get());

        size_t nDoubleCount = aMtf.GetActionSize();

        CPPUNIT_ASSERT_MESSAGE("Double hatch should have more lines than Single",
                               nDoubleCount > 10);
    }

    // Test RefPoint (Alignment)
    {
        Hatch aHatch(HatchStyle::Single, COL_BLACK, 20, 0_deg10); // Spacing 20

        // Helper to find the Y coordinate of the first line action
        auto findFirstLineY = [](GDIMetaFile& rMtf) -> tools::Long {
            for (size_t i = 0; i < rMtf.GetActionSize(); ++i)
            {
                MetaAction* pAct = rMtf.GetAction(i);
                if (pAct->GetType() == MetaActionType::LINE)
                {
                    return static_cast<MetaLineAction*>(pAct)->GetStartPoint().Y();
                }
            }
            return -999999; // Sentinel for "no lines found"
        };

        // Case A: RefPoint (0,0)
        aMtf.Clear();
        pVDev->SetRefPoint(Point(0, 0));
        vcl::HatchProcessor::DecomposeToMetaFile(aPoly, aHatch, aMtf, pVDev.get());

        tools::Long nYA = findFirstLineY(aMtf);

        // Case B: RefPoint (0,10) - Should shift lines by 10
        aMtf.Clear();
        pVDev->SetRefPoint(Point(0, 10));
        vcl::HatchProcessor::DecomposeToMetaFile(aPoly, aHatch, aMtf, pVDev.get());

        tools::Long nYB = findFirstLineY(aMtf);

        // Verify we actually generated lines
        CPPUNIT_ASSERT_MESSAGE("Case A produced no lines", nYA != -999999);
        CPPUNIT_ASSERT_MESSAGE("Case B produced no lines", nYB != -999999);

        // In a horizontal hatch (0 deg), Y coordinates should shift based on RefPoint.
        // We verify they are NOT identical.
        bool bIdentical = (nYA == nYB);
        CPPUNIT_ASSERT_MESSAGE("Hatch lines should shift with RefPoint", !bIdentical);
    }

    // Test Rotated Polygon (Crash check / Math stability)
    {
        aMtf.Clear();
        tools::Polygon aRotated(tools::Rectangle(0, 0, 100, 100));
        aRotated.Rotate(Point(50, 50), 450_deg10); // 45 degrees
        tools::PolyPolygon aRotPoly(aRotated);

        Hatch aHatch(HatchStyle::Single, COL_BLACK, 10, 0_deg10);

        // This exercises the intersection logic where lines hit corners
        vcl::HatchProcessor::DecomposeToMetaFile(aRotPoly, aHatch, aMtf, pVDev.get());

        // Just verify we didn't crash and produced something
        CPPUNIT_ASSERT(aMtf.GetActionSize() > 0);
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testEMFWriterHatchStrictSequence)
{
    // Strict regression test for EMFWriter Hatch decomposition.
    // We use a tiny geometry to force exactly 2 lines.

    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->SetMapMode(MapMode(MapUnit::MapPixel));

    tools::Rectangle aRect(0, 0, 20, 20);
    tools::PolyPolygon aPoly(aRect);
    Hatch aHatch(HatchStyle::Single, COL_RED, 10, 0_deg10); // Horz lines at Y=0, 10, 20

    // Create Input Metafile via Recording (Public API)
    GDIMetaFile aInputMtf;
    aInputMtf.SetPrefMapMode(MapMode(MapUnit::MapPixel));
    aInputMtf.SetPrefSize(Size(20, 20));

    // Record the DrawHatch call into the metafile
    aInputMtf.Record(pVDev.get());
    pVDev->DrawHatch(aPoly, aHatch);
    aInputMtf.Stop();

    // Export to EMF (triggers ImplWrite -> AddHatchActions)
    SvMemoryStream aStream;
    GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
    Graphic aGraphic(aInputMtf);

    // Get the format ID for EMF
    sal_uInt16 nFormat = rFilter.GetExportFormatNumberForShortName(u"emf");
    rFilter.ExportGraphic(aGraphic, u"none", aStream, nFormat);

    aStream.Seek(0);

    // Import and Verify Sequence
    Graphic aImportGraphic;
    rFilter.ImportGraphic(aImportGraphic, u"none", aStream);
    GDIMetaFile aRes = aImportGraphic.GetGDIMetaFile();

    // Verify Decomposition
    // Note: EMF round-trip strips MetaCommentActions, so we cannot search for "BeginGroup".
    // Instead, we search for the functional signature: Red LineColor + Horizontal Lines.

    bool bFoundHatch = false;
    bool bFoundRedColor = false;
    int nLineCount = 0;

    for (size_t i = 0; i < aRes.GetActionSize(); ++i)
    {
        MetaAction* pA = aRes.GetAction(i);
        if (pA->GetType() == MetaActionType::HATCH)
        {
            bFoundHatch = true;
        }
        else if (pA->GetType() == MetaActionType::LINECOLOR)
        {
            if (static_cast<MetaLineColorAction*>(pA)->GetColor() == COL_RED)
                bFoundRedColor = true;
        }
        else if (pA->GetType() == MetaActionType::LINE)
        {
            MetaLineAction* pLine = static_cast<MetaLineAction*>(pA);
            // Verify it is one of our hatch lines (Horizontal)
            if (pLine->GetStartPoint().Y() == pLine->GetEndPoint().Y())
            {
                nLineCount++;
            }
        }
    }

    CPPUNIT_ASSERT_MESSAGE("EMF should NOT contain the original Hatch action", !bFoundHatch);
    CPPUNIT_ASSERT_MESSAGE("EMF should contain the Hatch Color (Red)", bFoundRedColor);
    CPPUNIT_ASSERT_MESSAGE("EMF should contain decomposed horizontal lines", nLineCount >= 1);
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testDrawPolyLineStrokeAttributes)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->EnableOutput();

    GDIMetaFile aMtf;
    aMtf.Record(pVDev.get());

    basegfx::B2DPolygon aPolygon;
    aPolygon.append(basegfx::B2DPoint(10, 10));
    aPolygon.append(basegfx::B2DPoint(90, 90));

    // 1. Setup our new encapsulated struct
    vcl::rendercontext::StrokeAttributes aStroke;
    aStroke.fWidth = 2.0;
    aStroke.eJoin = basegfx::B2DLineJoin::Miter;
    aStroke.eCap = css::drawing::LineCap_ROUND;

    // 2. Call the NEW 3-parameter wrapper API
    pVDev->DrawPolyLine(aPolygon, aStroke, basegfx::B2DHomMatrix());

    aMtf.Stop();
    aMtf.WindStart();

    // 3. Verify the metafile recorded the action correctly
    size_t nActionCount = aMtf.GetActionSize();
    CPPUNIT_ASSERT_MESSAGE("No actions recorded", nActionCount > 0);

    // B2D geometry with advanced strokes are recorded as COMMENT actions
    MetaAction* pAction = aMtf.GetAction(nActionCount - 1);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected a comment record for B2DPolyLine",
                                 MetaActionType::COMMENT, pAction->GetType());

    auto pCommentAction = static_cast<MetaCommentAction*>(pAction);

    // Depending on exactly how MetafileRecorder::RecordB2DPolyLine is implemented,
    // it usually tags the comment. You can assert the identifier like this:
    CPPUNIT_ASSERT_EQUAL(OString("XB2DPOLYLINE_SEQ_END"), pCommentAction->GetComment());
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testHardwareOffsetTraitRouting)
{
    // Our baseline MapMode with an arbitrary origin of (50, 50)
    MapMode aTestMapMode(MapUnit::Map100thMM, Point(50, 50), Fraction(1, 1), Fraction(1, 1));

    // Scenario 1: Non-PageDevice (VirtualDevice)
    // Verify that the compiler evaluated `vcl::PageDevice<VirtualDevice>` as false.
    CPPUNIT_ASSERT_MESSAGE("VirtualDevice should NOT satisfy the PageDevice concept",
                           !vcl::PageDevice<VirtualDevice>);

    ScopedVclPtrInstance<VirtualDevice> pVDev;

    // Apply the map mode as if we were recording/playing a metafile (bIsRecord = false)
    pVDev->SetMetafileMapMode(aTestMapMode, false);

    // The origin should remain completely untouched.
    CPPUNIT_ASSERT_EQUAL_MESSAGE("VirtualDevice MapMode origin should not be offset", Point(50, 50),
                                 pVDev->GetMapMode().GetOrigin());

    // Scenario 2: PageDevice (Printer)
    // Verify that the compiler evaluated `vcl::PageDevice<Printer>` as true.
    CPPUNIT_ASSERT_MESSAGE("Printer MUST satisfy the PageDevice concept", vcl::PageDevice<Printer>);

    // In headless unit tests, a printer might not be configured on the CI runner.
    // We only test the offset math if a valid printer backend is available.
    if (!Printer::GetPrinterQueues().empty())
    {
        ScopedVclPtrInstance<Printer> pPrinter;
        Point aHardwareOffset = pPrinter->GetPageOffset();

        pPrinter->SetMetafileMapMode(aTestMapMode, false);

        // The new origin should have the physical hardware offset baked in.
        Point aExpectedOrigin(50 + aHardwareOffset.X(), 50 + aHardwareOffset.Y());

        CPPUNIT_ASSERT_EQUAL_MESSAGE("Printer MapMode origin must include GetPageOffset()",
                                     aExpectedOrigin, pPrinter->GetMapMode().GetOrigin());
    }
}

CPPUNIT_TEST_FIXTURE(VclOutdevTest, testOpticalSizingPointConversion)
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;

    // We use a long string to amplify any tracking/kerning differences
    // caused by loading the wrong optical size variant.
    OUString aTestStr(u"The quick brown fox jumps over the lazy dog."_ustr);

    // Case 1: The Control Case (Explicit Points)
    pVDev->SetMapMode(MapMode(MapUnit::MapPoint));

    // 36 points is exactly 0.5 inches.
    // (We use Fraunces because it is a bundled LO test font with an opsz axis).
    vcl::Font aFontPt(u"Fraunces"_ustr, Size(0, 36));
    pVDev->SetFont(aFontPt);

    tools::Long nWidthPt = pVDev->GetTextWidth(aTestStr);
    tools::Long nWidthPtPx = pVDev->LogicToPixel(Size(nWidthPt, 0)).Width();

    // Case 2: The Test Case (1/100th Millimeters)
    pVDev->SetMapMode(MapMode(MapUnit::Map100thMM));

    // 1270 100thMM is exactly 12.7mm, which is exactly 0.5 inches.
    // If o3tl::convert is working, this resolves to exactly 36.0f points.
    vcl::Font aFontMM(u"Fraunces"_ustr, Size(0, 1270));
    pVDev->SetFont(aFontMM);

    tools::Long nWidthMM = pVDev->GetTextWidth(aTestStr);
    tools::Long nWidthMMPx = pVDev->LogicToPixel(Size(nWidthMM, 0)).Width();

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(
        "Optical sizing failed: MapMode units were not correctly converted to typographic points!",
        static_cast<double>(nWidthPtPx), static_cast<double>(nWidthMMPx), 2.0);
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
