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
#include <vcl/bitmap.hxx>

class OutputDevice;
class GDIMetaFile;
class Point;
class Size;

namespace vcl
{
// Facade class for recording high-level OutputDevice operations
// into a GDIMetaFile. Hides the complexity of MetaAction construction.
class MetafileRecorder
{
private:
    GDIMetaFile* mpMetaFile;

public:
    explicit MetafileRecorder(OutputDevice& rDev);

    bool IsActive() const;

    // --- Bitmap Actions (MetaBmpAction) ---
    void RecordBitmap(const Point& rPos, const Bitmap& rBitmap);
    void RecordBitmapScale(const Point& rPos, const Size& rSz, const Bitmap& rBitmap);
    void RecordBitmapScalePart(const Point& rDestPos, const Size& rDestSz, const Point& rSrcPos,
                               const Size& rSrcSz, const Bitmap& rBitmap);

    // --- Bitmap with Alpha Actions (MetaBmpExAction) ---
    // Note: Takes Bitmap, as BitmapEx was removed/merged.
    void RecordBitmapEx(const Point& rPos, const Bitmap& rBitmap);
    void RecordBitmapExScale(const Point& rPos, const Size& rSz, const Bitmap& rBitmap);
    void RecordBitmapExScalePart(const Point& rDestPos, const Size& rDestSz, const Point& rSrcPos,
                                 const Size& rSrcSz, const Bitmap& rBitmap);
};

} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
