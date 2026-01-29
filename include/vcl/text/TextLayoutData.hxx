/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/vclptr.hxx>
#include <tools/gen.hxx>
#include <rtl/ustring.hxx>
#include <vector>

class Control;
class Point;
class Pair;

namespace vcl::text
{
struct UNLESS_MERGELIBS_MORE(VCL_DLLPUBLIC) TextLayoutData
{
    // contains the string really displayed
    // there must be exactly one bounding rectangle in m_aUnicodeBoundRects
    // for every character in m_aDisplayText
    OUString m_aDisplayText;
    // the bounding rectangle of every character
    // where one character may consist of many glyphs
    std::vector<tools::Rectangle> m_aUnicodeBoundRects;
    // start indices of lines
    std::vector<tools::Long> m_aLineIndices;
    // notify parent control on destruction
    VclPtr<const Control> m_pParent;

    TextLayoutData();
    ~TextLayoutData();

    tools::Rectangle GetCharacterBounds(tools::Long nIndex) const;
    // returns the character index for corresponding to rPoint (in control coordinates)
    // -1 is returned if no character is at that point
    tools::Long GetIndexForPoint(const Point& rPoint) const;
    // returns the interval [start,end] of line nLine
    // returns [-1,-1] for an invalid line
    ::Pair GetLineStartEnd(tools::Long nLine) const;
    /** ToRelativeLineIndex changes a layout data index to a count relative to its line.

    This is equivalent to getting the line start/end pairs with
    GetLineStartEnd until the index lies within [start,end] of a line

    @param nIndex
    the absolute index inside the display text to be changed to a relative index

    @returns
    the relative index inside the displayed line or -1 if the absolute index does
    not match any line
    */
    tools::Long ToRelativeLineIndex(tools::Long nIndex) const;
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
