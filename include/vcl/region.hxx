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
#include <tools/gen.hxx>
#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>

#include <vcl/dllapi.h>

#include <iterator>
#include <cstddef>
#include <memory>
#include <optional>

class RegionBand;

namespace vcl { class Window; }
class OutputDevice;
class Bitmap;

typedef std::vector< tools::Rectangle > RectangleVector;

namespace vcl {

class SAL_WARN_UNUSED VCL_DLLPUBLIC Region
{
private:
    friend class ::OutputDevice;
    friend class ::vcl::Window;
    friend class ::Bitmap;

    // possible contents
    std::optional< basegfx::B2DPolyPolygon >
                                mpB2DPolyPolygon;
    std::optional< tools::PolyPolygon >
                                mpPolyPolygon;
    std::shared_ptr< RegionBand >
                                mpRegionBand;

    bool                        mbIsNull : 1;

    // helpers
    SAL_DLLPRIVATE void ImplCreatePolyPolyRegion( const tools::PolyPolygon& rPolyPoly );
    SAL_DLLPRIVATE void ImplCreatePolyPolyRegion( const basegfx::B2DPolyPolygon& rPolyPoly );

    SAL_DLLPRIVATE tools::PolyPolygon ImplCreatePolyPolygonFromRegionBand() const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon ImplCreateB2DPolyPolygonFromRegionBand() const;

public:

    explicit Region(bool bIsNull = false); // default creates empty region, with true a null region is created
    explicit Region(const tools::Rectangle& rRect);
    explicit Region(const tools::Polygon& rPolygon);
    explicit Region(const tools::PolyPolygon& rPolyPoly);
    explicit Region(const basegfx::B2DPolyPolygon&);
    Region(const vcl::Region& rRegion);
    Region(vcl::Region&& rRegion) noexcept;
    ~Region();

    // direct access to contents
    const std::optional<basegfx::B2DPolyPolygon>& getB2DPolyPolygon() const { return mpB2DPolyPolygon; }
    const std::optional<tools::PolyPolygon>& getPolyPolygon() const { return mpPolyPolygon; }
    const RegionBand* getRegionBand() const { return mpRegionBand.get(); }

    // access with converters, the asked data will be created from the most
    // valuable data, buffered and returned
    tools::PolyPolygon GetAsPolyPolygon() const;
    basegfx::B2DPolyPolygon GetAsB2DPolyPolygon() const;
    const RegionBand* GetAsRegionBand() const;

    // manipulators
    void Move( tools::Long nHorzMove, tools::Long nVertMove );
    void Scale( double fScaleX, double fScaleY );
    void Union( const tools::Rectangle& rRegion );
    void Intersect( const tools::Rectangle& rRegion );
    void Exclude( const tools::Rectangle& rRegion );
    void XOr( const tools::Rectangle& rRegion );
    void Union( const vcl::Region& rRegion );
    void Intersect( const vcl::Region& rRegion );
    void Exclude( const vcl::Region& rRegion );
    bool XOr( const vcl::Region& rRegion );

    bool IsEmpty() const;
    bool IsNull() const { return mbIsNull;}

    void SetEmpty();
    void SetNull();

    bool IsRectangle() const;

    tools::Rectangle GetBoundRect() const;
    bool HasPolyPolygonOrB2DPolyPolygon() const { return (getB2DPolyPolygon() || getPolyPolygon()); }
    void GetRegionRectangles(RectangleVector& rTarget) const;

    class Iterator
    {
    private:
        std::shared_ptr<RectangleVector> mpRects;
        size_t mnIndex;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = tools::Rectangle;
        using difference_type = std::ptrdiff_t;
        using pointer = const tools::Rectangle*;
        using reference = const tools::Rectangle&;

        Iterator() : mnIndex(0) {}
        explicit Iterator(const Region* pRegion);

        reference operator*() const { return (*mpRects)[mnIndex]; }
        pointer operator->() const { return &(*mpRects)[mnIndex]; }

        Iterator& operator++() { ++mnIndex; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

        bool operator==(const Iterator& rOther) const {
            bool bThisEnd = !mpRects || mnIndex >= mpRects->size();
            bool bOtherEnd = !rOther.mpRects || rOther.mnIndex >= rOther.mpRects->size();
            if (bThisEnd && bOtherEnd) return true;
            if (bThisEnd != bOtherEnd) return false;
            return mpRects == rOther.mpRects && mnIndex == rOther.mnIndex;
        }
        bool operator!=(const Iterator& rOther) const { return !(*this == rOther); }
    };

    Iterator begin() const { return Iterator(this); }
    Iterator end() const { return Iterator(); }

    bool Contains( const Point& rPoint ) const;
    bool Overlaps( const tools::Rectangle& rRect ) const;

    vcl::Region& operator=( const vcl::Region& rRegion );
    vcl::Region& operator=( vcl::Region&& rRegion ) noexcept;
    vcl::Region& operator=( const tools::Rectangle& rRect );

    bool operator==( const vcl::Region& rRegion ) const;
    bool operator!=( const vcl::Region& rRegion ) const { return !(Region::operator==( rRegion )); }

    friend VCL_DLLPUBLIC SvStream& ReadRegion( SvStream& rIStm, vcl::Region& rRegion );
    friend SvStream& WriteRegion( SvStream& rOStm, const vcl::Region& rRegion );

    /* workaround: faster conversion for PolyPolygons
     * if half of the Polygons contained in rPolyPoly are actually
     * rectangles, then the returned vcl::Region will be constructed by
     * XOr'ing the contained Polygons together; in the case of
     * only Rectangles this can be up to eight times faster than
     * Region( const tools::PolyPolygon& ).
     * Caution: this is only useful if the vcl::Region is known to be
     * changed to rectangles; e.g. if being set as clip region
     */
    static vcl::Region GetRegionFromPolyPolygon( const tools::PolyPolygon& rPolyPoly );
};

template< typename charT, typename traits >
inline std::basic_ostream<charT, traits> & operator <<(
    std::basic_ostream<charT, traits> & stream, const Region& rRegion)
{
    if (rRegion.IsEmpty())
        return stream << "EMPTY";
    if (rRegion.getB2DPolyPolygon())
        return stream << "B2DPolyPolygon("
                      << *rRegion.getB2DPolyPolygon()
                      << ")";
    if (rRegion.getPolyPolygon())
        return stream << "PolyPolygon("
                      << *rRegion.getPolyPolygon()
                      << ")";
    if (rRegion.getRegionBand())
    {   // inlined because RegionBand is private to vcl
        stream << "RegionBand(";
        RectangleVector rects;
        rRegion.GetRegionRectangles(rects);
        if (rects.empty())
            stream << "EMPTY";
        for (size_t i = 0; i < rects.size(); ++i)
            stream << "[" << i << "] " << rects[i];
        stream << ")";
    }
    return stream;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
