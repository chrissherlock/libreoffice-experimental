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

#ifndef VCL_INC_IMPLPAGECACHE_HXX
#define VCL_INC_IMPLPAGECACHE_HXX

#include <vcl/PrinterController.hxx>

using namespace vcl;

class ImplPageCache
{
    struct CacheEntry
    {
        GDIMetaFile aPage;
        PrinterController::PageSize aSize;
    };

    std::vector<CacheEntry> maPages;
    std::vector<sal_Int32> maPageNumbers;
    std::vector<sal_Int32> maCacheRanking;

    static const sal_Int32 nCacheSize = 6;

    void updateRanking(sal_Int32 nLastHit)
    {
        if (maCacheRanking[0] != nLastHit)
        {
            for (sal_Int32 i = nCacheSize - 1; i > 0; i--)
                maCacheRanking[i] = maCacheRanking[i - 1];
            maCacheRanking[0] = nLastHit;
        }
    }

public:
    ImplPageCache()
        : maPages(nCacheSize)
        , maPageNumbers(nCacheSize, -1)
        , maCacheRanking(nCacheSize)
    {
        for (sal_Int32 i = 0; i < nCacheSize; i++)
            maCacheRanking[i] = nCacheSize - i - 1;
    }

    // caution: does not ensure uniqueness
    void insert(sal_Int32 i_nPageNo, const GDIMetaFile& i_rPage,
                const PrinterController::PageSize& i_rSize)
    {
        sal_Int32 nReplacePage = maCacheRanking.back();
        maPages[nReplacePage].aPage = i_rPage;
        maPages[nReplacePage].aSize = i_rSize;
        maPageNumbers[nReplacePage] = i_nPageNo;
        // cache insertion means in our case, the page was just queried
        // so update the ranking
        updateRanking(nReplacePage);
    }

    // caution: bad algorithm; should there ever be reason to increase the cache size beyond 6
    // this needs to be urgently rewritten. However do NOT increase the cache size lightly,
    // whole pages can be rather memory intensive
    bool get(sal_Int32 i_nPageNo, GDIMetaFile& o_rPageFile, PrinterController::PageSize& o_rSize)
    {
        for (sal_Int32 i = 0; i < nCacheSize; ++i)
        {
            if (maPageNumbers[i] == i_nPageNo)
            {
                updateRanking(i);
                o_rPageFile = maPages[i].aPage;
                o_rSize = maPages[i].aSize;
                return true;
            }
        }
        return false;
    }

    void invalidate()
    {
        for (sal_Int32 i = 0; i < nCacheSize; ++i)
        {
            maPageNumbers[i] = -1;
            maPages[i].aPage.Clear();
            maCacheRanking[i] = nCacheSize - i - 1;
        }
    }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
