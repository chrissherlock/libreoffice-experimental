/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/region.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <salobj.hxx>

namespace vcl::clipping
{
bool initChildRegion(WindowImpl& rImpl)
{
    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion([&rImpl]() { rImpl.mbInitChildRegion = false; });

    if (!rImpl.mpFirstChild)
    {
        rImpl.mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!rImpl.mpChildClipRegion)
        rImpl.mpChildClipRegion.reset(new vcl::Region(rImpl.maWinClipRegion));
    else
        *rImpl.mpChildClipRegion = rImpl.maWinClipRegion;

    return true; // Context contains children; signal the window to clip them
}

bool syncNativeWindow(WindowImpl& rImpl, vcl::Region& rWinChildClipRegion,
                      const vcl::Region* pOldRegion, bool& rOutUpdate)
{
    if (!rImpl.mpSysObj)
    {
        rOutUpdate = true;
        return true;
    }

    if (!rImpl.mbReallyVisible || rWinChildClipRegion.IsEmpty())
    {
        rImpl.mpSysObj->Show(false);
        rOutUpdate = true;
        return true;
    }

    rOutUpdate = true;
    if (pOldRegion)
    {
        vcl::Region aNewRegion = rWinChildClipRegion;
        rWinChildClipRegion.Intersect(*pOldRegion);
        rOutUpdate = (aNewRegion == rWinChildClipRegion);
    }

    return false; // Signal that downstream native updates are required
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
