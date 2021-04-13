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

#pragma once

#include <rtl/ref.hxx>
#include <salhelper/simplereferenceobject.hxx>
#include <tools/long.hxx>
#include <tools/ref.hxx>

#include <vcl/dllapi.h>

#include <font/FontAttributes.hxx>

class FontInstance;
class FontCharMap;
struct FontMatchStatus;
class FontSelectPattern;
class FontFamily;

namespace vcl
{
struct FontCapabilities;
}

typedef tools::SvRef<FontCharMap> FontCharMapRef;

struct FontMatchStatus
{
public:
    int mnFaceMatch;
    int mnHeightMatch;
    int mnWidthMatch;
    const OUString* mpTargetStyleName;
};

// TODO: no more direct access to members
// TODO: get rid of height/width for scalable fonts
// TODO: make cloning cheaper

/**
 * abstract base class for physical font faces
 *
 * It acts as a factory for its corresponding FontInstances and
 * can be extended to cache device and font instance specific data.
 */
class VCL_PLUGIN_PUBLIC FontFace : public FontAttributes, public salhelper::SimpleReferenceObject
{
public:
    virtual rtl::Reference<FontInstance> CreateFontInstance(const FontSelectPattern&) const = 0;

    int GetHeight() const { return mnHeight; }
    int GetWidth() const { return mnWidth; }
    virtual sal_IntPtr GetFontId() const = 0;
    virtual FontCharMapRef GetFontCharMap() const = 0;
    virtual bool GetFontCapabilities(vcl::FontCapabilities&) const = 0;

    bool IsBetterMatch(const FontSelectPattern&, FontMatchStatus&) const;
    sal_Int32 CompareWithSize(const FontFace&) const;
    sal_Int32 CompareIgnoreSize(const FontFace&) const;

protected:
    explicit FontFace(const FontAttributes&);
    void SetBitmapSize(int nW, int nH)
    {
        mnWidth = nW;
        mnHeight = nH;
    }

    tools::Long mnWidth; // Width (in pixels)
    tools::Long mnHeight; // Height (in pixels)
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
