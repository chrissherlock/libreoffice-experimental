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

#ifndef INCLUDED_VCL_METAACT_HXX
#define INCLUDED_VCL_METAACT_HXX

#include <config_options.h>
#include <memory>
#include <vcl/dllapi.h>
#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <vcl/wall.hxx>
#include <vcl/font.hxx>
#include <tools/poly.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/region.hxx>
#include <vcl/outdevstate.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/gfxlink.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/metaactiontypes.hxx>
#include <salhelper/simplereferenceobject.hxx>
#include <rtl/ref.hxx>

class OutputDevice;
class SvStream;
enum class DrawTextFlags;

struct ImplMetaReadData
{
    rtl_TextEncoding meActualCharSet;
    int mnParseDepth;

    ImplMetaReadData()
        : meActualCharSet(RTL_TEXTENCODING_ASCII_US)
        , mnParseDepth(0)
    {
    }
};

struct ImplMetaWriteData
{
    rtl_TextEncoding meActualCharSet;

    ImplMetaWriteData()
        : meActualCharSet(RTL_TEXTENCODING_ASCII_US)
    {
    }
};

class VCL_DLLPUBLIC MetaAction : public salhelper::SimpleReferenceObject
{
private:
    MetaActionType mnType;

protected:
    virtual ~MetaAction() override;

public:
    MetaAction();
    explicit MetaAction(MetaActionType nType);
    MetaAction(MetaAction const&);

    virtual void Execute(OutputDevice* pOut);

    oslInterlockedCount GetRefCount() const { return m_nCount; }

    virtual rtl::Reference<MetaAction> Clone();

    virtual void Move(long nHorzMove, long nVertMove);
    virtual void Scale(double fScaleX, double fScaleY);

    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData);
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData);

    MetaActionType GetType() const { return mnType; }
    /** \#i10613# Extracted from Printer::GetPreparedMetaFile. Returns true
        if given action requires special transparency handling
    */
    virtual bool IsTransparent() const { return false; }

public:
    static MetaAction* ReadMetaAction(SvStream& rIStm, ImplMetaReadData* pData);
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaPixelAction final : public MetaAction
{
private:
    Point maPt;
    Color maColor;

public:
    MetaPixelAction();
    MetaPixelAction(MetaPixelAction const&) = default;
    MetaPixelAction(MetaPixelAction&&) = default;
    MetaPixelAction& operator=(MetaPixelAction const&) = delete; // due to MetaAction
    MetaPixelAction& operator=(MetaPixelAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPixelAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaPixelAction(const Point& rPt, const Color& rColor);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetPoint() const { return maPt; }
    const Color& GetColor() const { return maColor; }
};

class SAL_DLLPUBLIC_RTTI MetaPointAction final : public MetaAction
{
private:
    Point maPt;

public:
    MetaPointAction();
    MetaPointAction(MetaPointAction const&) = default;
    MetaPointAction(MetaPointAction&&) = default;
    MetaPointAction& operator=(MetaPointAction const&) = delete; // due to MetaAction
    MetaPointAction& operator=(MetaPointAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPointAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaPointAction(const Point&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetPoint() const { return maPt; }
};

class VCL_DLLPUBLIC MetaLineAction final : public MetaAction
{
private:
    LineInfo maLineInfo;
    Point maStartPt;
    Point maEndPt;

public:
    MetaLineAction();
    MetaLineAction(MetaLineAction const&) = default;
    MetaLineAction(MetaLineAction&&) = default;
    MetaLineAction& operator=(MetaLineAction const&) = delete; // due to MetaAction
    MetaLineAction& operator=(MetaLineAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaLineAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaLineAction(const Point& rStart, const Point& rEnd);
    MetaLineAction(const Point& rStart, const Point& rEnd, const LineInfo& rLineInfo);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetStartPoint() const { return maStartPt; }
    const Point& GetEndPoint() const { return maEndPt; }
    const LineInfo& GetLineInfo() const { return maLineInfo; }
};

class VCL_DLLPUBLIC MetaRectAction final : public MetaAction
{
private:
    tools::Rectangle maRect;

public:
    MetaRectAction();
    MetaRectAction(MetaRectAction const&) = default;
    MetaRectAction(MetaRectAction&&) = default;
    MetaRectAction& operator=(MetaRectAction const&) = delete; // due to MetaAction
    MetaRectAction& operator=(MetaRectAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaRectAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaRectAction(const tools::Rectangle&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaRoundRectAction final : public MetaAction
{
private:
    tools::Rectangle maRect;
    sal_uInt32 mnHorzRound;
    sal_uInt32 mnVertRound;

public:
    MetaRoundRectAction();
    MetaRoundRectAction(MetaRoundRectAction const&) = default;
    MetaRoundRectAction(MetaRoundRectAction&&) = default;
    MetaRoundRectAction& operator=(MetaRoundRectAction const&) = delete; // due to MetaAction
    MetaRoundRectAction& operator=(MetaRoundRectAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaRoundRectAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaRoundRectAction(const tools::Rectangle& rRect, sal_uInt32 nHorzRound,
                        sal_uInt32 nVertRound);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
    sal_uInt32 GetHorzRound() const { return mnHorzRound; }
    sal_uInt32 GetVertRound() const { return mnVertRound; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaEllipseAction final : public MetaAction
{
private:
    tools::Rectangle maRect;

public:
    MetaEllipseAction();
    MetaEllipseAction(MetaEllipseAction const&) = default;
    MetaEllipseAction(MetaEllipseAction&&) = default;
    MetaEllipseAction& operator=(MetaEllipseAction const&) = delete; // due to MetaAction
    MetaEllipseAction& operator=(MetaEllipseAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaEllipseAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaEllipseAction(const tools::Rectangle&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaArcAction final : public MetaAction
{
private:
    tools::Rectangle maRect;
    Point maStartPt;
    Point maEndPt;

public:
    MetaArcAction();
    MetaArcAction(MetaArcAction const&) = default;
    MetaArcAction(MetaArcAction&&) = default;
    MetaArcAction& operator=(MetaArcAction const&) = delete; // due to MetaAction
    MetaArcAction& operator=(MetaArcAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaArcAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaArcAction(const tools::Rectangle& rRect, const Point& rStart, const Point& rEnd);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
    const Point& GetStartPoint() const { return maStartPt; }
    const Point& GetEndPoint() const { return maEndPt; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaPieAction final : public MetaAction
{
private:
    tools::Rectangle maRect;
    Point maStartPt;
    Point maEndPt;

public:
    MetaPieAction();
    MetaPieAction(MetaPieAction const&) = default;
    MetaPieAction(MetaPieAction&&) = default;
    MetaPieAction& operator=(MetaPieAction const&) = delete; // due to MetaAction
    MetaPieAction& operator=(MetaPieAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPieAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaPieAction(const tools::Rectangle& rRect, const Point& rStart, const Point& rEnd);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
    const Point& GetStartPoint() const { return maStartPt; }
    const Point& GetEndPoint() const { return maEndPt; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaChordAction final : public MetaAction
{
private:
    tools::Rectangle maRect;
    Point maStartPt;
    Point maEndPt;

public:
    MetaChordAction();
    MetaChordAction(MetaChordAction const&) = default;
    MetaChordAction(MetaChordAction&&) = default;
    MetaChordAction& operator=(MetaChordAction const&) = delete; // due to MetaAction
    MetaChordAction& operator=(MetaChordAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaChordAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaChordAction(const tools::Rectangle& rRect, const Point& rStart, const Point& rEnd);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
    const Point& GetStartPoint() const { return maStartPt; }
    const Point& GetEndPoint() const { return maEndPt; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaPolyLineAction final : public MetaAction
{
private:
    LineInfo maLineInfo;
    tools::Polygon maPoly;

public:
    MetaPolyLineAction();
    MetaPolyLineAction(MetaPolyLineAction const&) = default;
    MetaPolyLineAction(MetaPolyLineAction&&) = default;
    MetaPolyLineAction& operator=(MetaPolyLineAction const&) = delete; // due to MetaAction
    MetaPolyLineAction& operator=(MetaPolyLineAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPolyLineAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaPolyLineAction(const tools::Polygon&);
    explicit MetaPolyLineAction(const tools::Polygon&, const LineInfo&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Polygon& GetPolygon() const { return maPoly; }
    const LineInfo& GetLineInfo() const { return maLineInfo; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaPolygonAction final : public MetaAction
{
private:
    tools::Polygon maPoly;

public:
    MetaPolygonAction();
    MetaPolygonAction(MetaPolygonAction const&) = default;
    MetaPolygonAction(MetaPolygonAction&&) = default;
    MetaPolygonAction& operator=(MetaPolygonAction const&) = delete; // due to MetaAction
    MetaPolygonAction& operator=(MetaPolygonAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPolygonAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaPolygonAction(const tools::Polygon&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Polygon& GetPolygon() const { return maPoly; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaPolyPolygonAction final : public MetaAction
{
private:
    tools::PolyPolygon maPolyPoly;

public:
    MetaPolyPolygonAction();
    MetaPolyPolygonAction(MetaPolyPolygonAction const&) = default;
    MetaPolyPolygonAction(MetaPolyPolygonAction&&) = default;
    MetaPolyPolygonAction& operator=(MetaPolyPolygonAction const&) = delete; // due to MetaAction
    MetaPolyPolygonAction& operator=(MetaPolyPolygonAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaPolyPolygonAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    explicit MetaPolyPolygonAction(const tools::PolyPolygon&);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::PolyPolygon& GetPolyPolygon() const { return maPolyPoly; }
};

class SAL_DLLPUBLIC_RTTI MetaTextAction final : public MetaAction
{
private:
    Point maPt;
    OUString maStr;
    sal_Int32 mnIndex;
    sal_Int32 mnLen;

public:
    MetaTextAction();
    MetaTextAction(MetaTextAction const&) = default;
    MetaTextAction(MetaTextAction&&) = default;
    MetaTextAction& operator=(MetaTextAction const&) = delete; // due to MetaAction
    MetaTextAction& operator=(MetaTextAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaTextAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaTextAction(const Point& rPt, const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetPoint() const { return maPt; }
    const OUString& GetText() const { return maStr; }
    sal_Int32 GetIndex() const { return mnIndex; }
    sal_Int32 GetLen() const { return mnLen; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaTextArrayAction final : public MetaAction
{
private:
    Point maStartPt;
    OUString maStr;
    std::unique_ptr<long[]> mpDXAry;
    sal_Int32 mnIndex;
    sal_Int32 mnLen;

    virtual ~MetaTextArrayAction() override;

public:
    MetaTextArrayAction();
    MetaTextArrayAction(const MetaTextArrayAction& rAction);
    MetaTextArrayAction(const Point& rStartPt, const OUString& rStr, const long* pDXAry,
                        sal_Int32 nIndex, sal_Int32 nLen);

    virtual void Execute(OutputDevice* pOut) override;

    virtual rtl::Reference<MetaAction> Clone() override;

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    const Point& GetPoint() const { return maStartPt; }
    const OUString& GetText() const { return maStr; }
    sal_Int32 GetIndex() const { return mnIndex; }
    sal_Int32 GetLen() const { return mnLen; }
    long* GetDXArray() const { return mpDXAry.get(); }
};

class SAL_DLLPUBLIC_RTTI MetaStretchTextAction final : public MetaAction
{
private:
    Point maPt;
    OUString maStr;
    sal_uInt32 mnWidth;
    sal_Int32 mnIndex;
    sal_Int32 mnLen;

public:
    MetaStretchTextAction();
    MetaStretchTextAction(MetaStretchTextAction const&) = default;
    MetaStretchTextAction(MetaStretchTextAction&&) = default;
    MetaStretchTextAction& operator=(MetaStretchTextAction const&) = delete; // due to MetaAction
    MetaStretchTextAction& operator=(MetaStretchTextAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaStretchTextAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaStretchTextAction(const Point& rPt, sal_uInt32 nWidth, const OUString& rStr,
                          sal_Int32 nIndex, sal_Int32 nLen);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetPoint() const { return maPt; }
    const OUString& GetText() const { return maStr; }
    sal_uInt32 GetWidth() const { return mnWidth; }
    sal_Int32 GetIndex() const { return mnIndex; }
    sal_Int32 GetLen() const { return mnLen; }
};

class SAL_DLLPUBLIC_RTTI MetaTextRectAction final : public MetaAction
{
private:
    tools::Rectangle maRect;
    OUString maStr;
    DrawTextFlags mnStyle;

public:
    MetaTextRectAction();
    MetaTextRectAction(MetaTextRectAction const&) = default;
    MetaTextRectAction(MetaTextRectAction&&) = default;
    MetaTextRectAction& operator=(MetaTextRectAction const&) = delete; // due to MetaAction
    MetaTextRectAction& operator=(MetaTextRectAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaTextRectAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaTextRectAction(const tools::Rectangle& rRect, const OUString& rStr, DrawTextFlags nStyle);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const tools::Rectangle& GetRect() const { return maRect; }
    const OUString& GetText() const { return maStr; }
    DrawTextFlags GetStyle() const { return mnStyle; }
};

class SAL_DLLPUBLIC_RTTI MetaTextLineAction final : public MetaAction
{
private:
    Point maPos;
    long mnWidth;
    FontStrikeout meStrikeout;
    FontLineStyle meUnderline;
    FontLineStyle meOverline;

public:
    MetaTextLineAction();
    MetaTextLineAction(MetaTextLineAction const&) = default;
    MetaTextLineAction(MetaTextLineAction&&) = default;
    MetaTextLineAction& operator=(MetaTextLineAction const&) = delete; // due to MetaAction
    MetaTextLineAction& operator=(MetaTextLineAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaTextLineAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaTextLineAction(const Point& rPos, long nWidth, FontStrikeout eStrikeout,
                       FontLineStyle eUnderline, FontLineStyle eOverline);
    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Point& GetStartPoint() const { return maPos; }
    long GetWidth() const { return mnWidth; }
    FontStrikeout GetStrikeout() const { return meStrikeout; }
    FontLineStyle GetUnderline() const { return meUnderline; }
    FontLineStyle GetOverline() const { return meOverline; }
};

class VCL_DLLPUBLIC MetaBmpAction final : public MetaAction
{
private:
    Bitmap maBmp;
    Point maPt;

public:
    MetaBmpAction();
    MetaBmpAction(MetaBmpAction const&) = default;
    MetaBmpAction(MetaBmpAction&&) = default;
    MetaBmpAction& operator=(MetaBmpAction const&) = delete; // due to MetaAction
    MetaBmpAction& operator=(MetaBmpAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpAction(const Point& rPt, const Bitmap& rBmp);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Bitmap& GetBitmap() const { return maBmp; }
    const Point& GetPoint() const { return maPt; }
};

class VCL_DLLPUBLIC MetaBmpScaleAction final : public MetaAction
{
private:
    Bitmap maBmp;
    Point maPt;
    Size maSz;

public:
    MetaBmpScaleAction();
    MetaBmpScaleAction(MetaBmpScaleAction const&) = default;
    MetaBmpScaleAction(MetaBmpScaleAction&&) = default;
    MetaBmpScaleAction& operator=(MetaBmpScaleAction const&) = delete; // due to MetaAction
    MetaBmpScaleAction& operator=(MetaBmpScaleAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpScaleAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpScaleAction(const Point& rPt, const Size& rSz, const Bitmap& rBmp);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Bitmap& GetBitmap() const { return maBmp; }
    const Point& GetPoint() const { return maPt; }
    const Size& GetSize() const { return maSz; }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaBmpScalePartAction final : public MetaAction
{
private:
    Bitmap maBmp;
    Point maDstPt;
    Size maDstSz;
    Point maSrcPt;
    Size maSrcSz;

public:
    MetaBmpScalePartAction();
    MetaBmpScalePartAction(MetaBmpScalePartAction const&) = default;
    MetaBmpScalePartAction(MetaBmpScalePartAction&&) = default;
    MetaBmpScalePartAction& operator=(MetaBmpScalePartAction const&) = delete; // due to MetaAction
    MetaBmpScalePartAction& operator=(MetaBmpScalePartAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpScalePartAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpScalePartAction(const Point& rDstPt, const Size& rDstSz, const Point& rSrcPt,
                           const Size& rSrcSz, const Bitmap& rBmp);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const Bitmap& GetBitmap() const { return maBmp; }
    const Point& GetDestPoint() const { return maDstPt; }
    const Size& GetDestSize() const { return maDstSz; }
    const Point& GetSrcPoint() const { return maSrcPt; }
    const Size& GetSrcSize() const { return maSrcSz; }
};

class VCL_DLLPUBLIC MetaBmpExAction final : public MetaAction
{
private:
    BitmapEx maBmpEx;
    Point maPt;

public:
    MetaBmpExAction();
    MetaBmpExAction(MetaBmpExAction const&) = default;
    MetaBmpExAction(MetaBmpExAction&&) = default;
    MetaBmpExAction& operator=(MetaBmpExAction const&) = delete; // due to MetaAction
    MetaBmpExAction& operator=(MetaBmpExAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpExAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpExAction(const Point& rPt, const BitmapEx& rBmpEx);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const BitmapEx& GetBitmapEx() const { return maBmpEx; }
    const Point& GetPoint() const { return maPt; }
    bool IsTransparent() const override { return GetBitmapEx().IsTransparent(); }
};

class VCL_DLLPUBLIC MetaBmpExScaleAction final : public MetaAction
{
private:
    BitmapEx maBmpEx;
    Point maPt;
    Size maSz;

public:
    MetaBmpExScaleAction();
    MetaBmpExScaleAction(MetaBmpExScaleAction const&) = default;
    MetaBmpExScaleAction(MetaBmpExScaleAction&&) = default;
    MetaBmpExScaleAction& operator=(MetaBmpExScaleAction const&) = delete; // due to MetaAction
    MetaBmpExScaleAction& operator=(MetaBmpExScaleAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpExScaleAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpExScaleAction(const Point& rPt, const Size& rSz, const BitmapEx& rBmpEx);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const BitmapEx& GetBitmapEx() const { return maBmpEx; }
    const Point& GetPoint() const { return maPt; }
    const Size& GetSize() const { return maSz; }
    bool IsTransparent() const override { return GetBitmapEx().IsTransparent(); }
};

class UNLESS_MERGELIBS(VCL_DLLPUBLIC) MetaBmpExScalePartAction final : public MetaAction
{
private:
    BitmapEx maBmpEx;
    Point maDstPt;
    Size maDstSz;
    Point maSrcPt;
    Size maSrcSz;

public:
    MetaBmpExScalePartAction();
    MetaBmpExScalePartAction(MetaBmpExScalePartAction const&) = default;
    MetaBmpExScalePartAction(MetaBmpExScalePartAction&&) = default;
    MetaBmpExScalePartAction& operator=(MetaBmpExScalePartAction const&)
        = delete; // due to MetaAction
    MetaBmpExScalePartAction& operator=(MetaBmpExScalePartAction&&) = delete; // due to MetaAction
private:
    virtual ~MetaBmpExScalePartAction() override;

public:
    virtual void Execute(OutputDevice* pOut) override;
    virtual rtl::Reference<MetaAction> Clone() override;
    virtual void Write(SvStream& rOStm, ImplMetaWriteData* pData) override;
    virtual void Read(SvStream& rIStm, ImplMetaReadData* pData) override;

    MetaBmpExScalePartAction(const Point& rDstPt, const Size& rDstSz, const Point& rSrcPt,
                             const Size& rSrcSz, const BitmapEx& rBmpEx);

    virtual void Move(long nHorzMove, long nVertMove) override;
    virtual void Scale(double fScaleX, double fScaleY) override;

    const BitmapEx& GetBitmapEx() const { return maBmpEx; }
    const Point& GetDestPoint() const { return maDstPt; }
    const Size& GetDestSize() const { return maDstSz; }
    const Point& GetSrcPoint() const { return maSrcPt; }
    const Size& GetSrcSize() const { return maSrcSz; }
    bool IsTransparent() const override { return GetBitmapEx().IsTransparent(); }
};

#endif // INCLUDED_VCL_METAACT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
