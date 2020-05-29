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

#ifndef VCL_INC_IMPLPRINTERCONTROLLERCACHE_HXX
#define VCL_INC_IMPLPRINTERCONTROLLERCACHE_HXX

#include <printdlg.hxx>

using namespace vcl;

namespace vcl
{
class PrintProgressDialog;
}

class vcl::ImplPrinterControllerData
{
public:
    struct ControlDependency
    {
        OUString maDependsOnName;
        sal_Int32 mnDependsOnEntry;

        ControlDependency()
            : mnDependsOnEntry(-1)
        {
        }
    };

    typedef std::unordered_map<OUString, size_t> PropertyToIndexMap;
    typedef std::unordered_map<OUString, ControlDependency> ControlDependencyMap;
    typedef std::unordered_map<OUString, css::uno::Sequence<sal_Bool>> ChoiceDisableMap;

    VclPtr<Printer> mxPrinter;
    weld::Window* mpWindow;
    css::uno::Sequence<css::beans::PropertyValue> maUIOptions;
    std::vector<css::beans::PropertyValue> maUIProperties;
    std::vector<bool> maUIPropertyEnabled;
    PropertyToIndexMap maPropertyToIndex;
    ControlDependencyMap maControlDependencies;
    ChoiceDisableMap maChoiceDisableMap;
    bool mbFirstPage;
    bool mbLastPage;
    bool mbReversePageOrder;
    bool mbPapersizeFromSetup;
    bool mbPapersizeFromUser;
    bool mbPrinterModified;
    css::view::PrintableState meJobState;

    vcl::PrinterController::MultiPageSetup maMultiPage;

    std::shared_ptr<vcl::PrintProgressDialog> mxProgress;

    ImplPageCache maPageCache;

    // set by user through printer properties subdialog of printer settings dialog
    Size maDefaultPageSize;
    // set by user through print dialog
    Size maUserPageSize;
    // set by user through printer properties subdialog of printer settings dialog
    sal_Int32 mnDefaultPaperBin;
    // Set by user through printer properties subdialog of print dialog.
    // Overrides application-set tray for a page.
    sal_Int32 mnFixedPaperBin;

    // N.B. Apparently we have three levels of paper tray settings
    // (latter overrides former):
    // 1. default tray
    // 2. tray set for a concrete page by an application, e.g., writer
    //    allows setting a printer tray (for the default printer) for a
    //    page style. This setting can be overridden by user by selecting
    //    "Use only paper tray from printer preferences" on the Options
    //    page in the print dialog, in which case the default tray is
    //    used for all pages.
    // 3. tray set in printer properties the printer dialog
    // I'm not quite sure why 1. and 3. are distinct, but the commit
    // history suggests this is intentional...

    ImplPrinterControllerData()
        : mpWindow(nullptr)
        , mbFirstPage(true)
        , mbLastPage(false)
        , mbReversePageOrder(false)
        , mbPapersizeFromSetup(false)
        , mbPapersizeFromUser(false)
        , mbPrinterModified(false)
        , meJobState(css::view::PrintableState_JOB_STARTED)
        , mnDefaultPaperBin(-1)
        , mnFixedPaperBin(-1)
    {
    }

    ~ImplPrinterControllerData()
    {
        if (mxProgress)
        {
            mxProgress->response(RET_CANCEL);
            mxProgress.reset();
        }
    }

    const Size& getRealPaperSize(const Size& i_rPageSize, bool bNoNUP) const
    {
        if (mbPapersizeFromUser)
            return maUserPageSize;
        if (mbPapersizeFromSetup)
            return maDefaultPageSize;
        if (maMultiPage.nRows * maMultiPage.nColumns > 1 && !bNoNUP)
            return maMultiPage.aPaperSize;
        return i_rPageSize;
    }
    bool isFixedPageSize() const { return mbPapersizeFromSetup; }
    PrinterController::PageSize
    modifyJobSetup(const css::uno::Sequence<css::beans::PropertyValue>& i_rProps);
    void resetPaperToLastConfigured();
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
