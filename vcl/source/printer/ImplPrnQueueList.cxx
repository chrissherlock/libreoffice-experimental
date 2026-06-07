/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <print.h>
#include <salprn.hxx>

#include <memory>
#include <unordered_map>

ImplPrnQueueList::~ImplPrnQueueList() {}

void ImplPrnQueueList::Add(std::unique_ptr<SalPrinterQueueInfo> pData)
{
    std::unordered_map<OUString, sal_Int32>::iterator it
        = m_aNameToIndex.find(pData->maPrinterName);
    if (it == m_aNameToIndex.end())
    {
        m_aNameToIndex[pData->maPrinterName] = m_aQueueInfos.size();
        m_aPrinterList.push_back(pData->maPrinterName);
        m_aQueueInfos.push_back(ImplPrnQueueData());
        m_aQueueInfos.back().mpQueueInfo = nullptr;
        m_aQueueInfos.back().mpSalQueueInfo = std::move(pData);
    }
    else // this should not happen, but ...
    {
        ImplPrnQueueData& rData = m_aQueueInfos[it->second];
        rData.mpQueueInfo.reset();
        rData.mpSalQueueInfo = std::move(pData);
    }
}

ImplPrnQueueData* ImplPrnQueueList::Get(const OUString& rPrinter)
{
    ImplPrnQueueData* pData = nullptr;
    std::unordered_map<OUString, sal_Int32>::iterator it = m_aNameToIndex.find(rPrinter);
    if (it != m_aNameToIndex.end())
        pData = &m_aQueueInfos[it->second];
    return pData;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
