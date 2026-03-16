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

#include <vcl/dllapi.h>

#include <tools/color.hxx>
#include <tools/long.hxx>

class GDIMetaFile;
class OutputDevice;

namespace vcl::metafile
{
/**
 * Bundles the configuration parameters required for flattening
 * transparent vector operations into raster bands.
 */
struct FlatteningOptions
{
    tools::Long nMaxBmpDPIX = 0;
    tools::Long nMaxBmpDPIY = 0;
    bool bReduceTransparency = false;
    bool bTransparencyAutoMode = false;
    bool bDownsampleBitmaps = false;
    Color aBackground = COL_TRANSPARENT;
};

/**
 * A VCL-internal utility to process and optimize metafiles.
 * * This class is specifically designed to isolate the complex geometric
 * intersection algorithms required to emulate transparency on devices
 * that lack native alpha-blending capabilities (e.g., legacy GDI printers).
 */
class VCL_DLLPUBLIC TransparencyFlattener
{
public:
    /**
     * Scans the input metafile for transparent objects and generates a new metafile
     * where intersecting transparent areas are flattened into opaque raster bands.
     *
     * @param rInput The original metafile containing transparent actions.
     * @param rOutput The resulting metafile with transparent areas flattened.
     * @param rRefDevice Used strictly to provide DPI, MapMode, and boundary context.
     * @param rOptions Settings governing DPI limits and background color substitution.
     * @return true if transparencies were found and processed; false otherwise.
     */
    static bool Flatten(const GDIMetaFile& rInput, GDIMetaFile& rOutput,
                        const OutputDevice& rRefDevice, const FlatteningOptions& rOptions);
};

} // namespace vcl::metafile

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
