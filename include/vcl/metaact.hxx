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

#endif // INCLUDED_VCL_METAACT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
