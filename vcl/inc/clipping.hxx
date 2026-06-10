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

class Window;
class WindowImpl;

namespace vcl
{
class Region;
}

namespace vcl::clipping
{
// Returns true if child clipping needs to be executed by the window
VCL_DLLPUBLIC bool initChildRegion(WindowImpl& rImpl);

// Core visibility synchronization pipeline
VCL_DLLPUBLIC bool syncNativeWindow(WindowImpl& rImpl, vcl::Region& rWinChildClipRegion,
                                    const vcl::Region* pOldRegion, bool& rOutUpdate);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
