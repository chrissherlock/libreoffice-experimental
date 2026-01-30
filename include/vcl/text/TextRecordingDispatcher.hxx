/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#pragma once
#include <rtl/ustring.hxx>
#include <vcl/dllapi.h>
#include <tools/gen.hxx>

class OutputDevice;
class SalLayout;

namespace vcl::text
{
class TextRecordingState;

class VCL_DLLPUBLIC TextRecordingDispatcher
{
public:
    static void Dispatch(const TextRecordingState& rState, OutputDevice& rDev, const OUString& rStr,
                         sal_Int32 nIndex, sal_Int32 nLen, const SalLayout* pLayout,
                         bool bLineStart = false);
};
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
