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
// RAII helper to wrap a sequence of meta-actions in a Semantic Group
//
class ScopedMetaGroup
{
    GDIMetaFile* mpMetaFile;

public:
    ScopedMetaGroup(GDIMetaFile* pMetaFile, const OString& rGroupName)
        : mpMetaFile(pMetaFile)
    {
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaCommentAction("BeginGroup: " + rGroupName));
    }

    ~ScopedMetaGroup()
    {
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaCommentAction("EndGroup"));
    }

    ScopedMetaGroup(const ScopedMetaGroup&) = delete;
    ScopedMetaGroup& operator=(const ScopedMetaGroup&) = delete;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
