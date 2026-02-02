/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>

namespace vcl
{
// RAII helper to wrap a sequence of meta-actions in a Group
class ScopedMetaGroup
{
    GDIMetaFile* mpMetaFile;
    OString maEndComment;

public:
    // Semantic Tagging (Legacy behavior: "BeginGroup: Name" ... "EndGroup")
    ScopedMetaGroup(GDIMetaFile* pMetaFile, const OString& rGroupName)
        : mpMetaFile(pMetaFile)
        , maEndComment("EndGroup")
    {
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaCommentAction("BeginGroup: " + rGroupName));
    }

    // Explicit Tagging (New: "START_TAG" ... "END_TAG")
    ScopedMetaGroup(GDIMetaFile* pMetaFile, const OString& rStart, const OString& rEnd)
        : mpMetaFile(pMetaFile)
        , maEndComment(rEnd)
    {
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaCommentAction(rStart));
    }

    ~ScopedMetaGroup()
    {
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaCommentAction(maEndComment));
    }

    ScopedMetaGroup(const ScopedMetaGroup&) = delete;
    ScopedMetaGroup& operator=(const ScopedMetaGroup&) = delete;
};

} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
