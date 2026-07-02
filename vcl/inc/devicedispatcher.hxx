/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/outdev.hxx>
#include <vcl/print.hxx>
#include <vcl/virdev.hxx>

#include <WindowOutputDevice.hxx>
#include <pdf/pdfwriter_impl.hxx>

#include <utility>

namespace vcl
{
/**
 * Bridges runtime polymorphic OutputDevice references to compile-time
 * type-safe concept templates.
 */
template <typename Callable, typename... Args>
decltype(auto) DispatchDevice(OutputDevice& rDevice, Callable&& func, Args&&... args)
{
    // CRITICAL ORDERING: PDFWriterImpl MUST be checked before VirtualDevice.
    // Because PDFWriterImpl inherits from VirtualDevice, a dynamic_cast to
    // VirtualDevice* would succeed, routing the PDF to the GPU fast-path!
    if (auto* pPDF = dynamic_cast<PDFWriterImpl*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pPDF, std::forward<Args>(args)...);
    }
    else if (auto* pPrinter = dynamic_cast<Printer*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pPrinter, std::forward<Args>(args)...);
    }
    else if (auto* pWindow = dynamic_cast<WindowOutputDevice*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pWindow, std::forward<Args>(args)...);
    }
    else if (auto* pVirDev = dynamic_cast<VirtualDevice*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pVirDev, std::forward<Args>(args)...);
    }

    // Fallback: If it's a raw OutputDevice or an unknown subclass,
    // just pass the base reference.
    return std::forward<Callable>(func)(rDevice, std::forward<Args>(args)...);
}

/**
 * Const-overload for DispatchDevice.
 * Maintains the same critical ordering as the non-const version to ensure
 * PDFWriterImpl is correctly identified before falling back to VirtualDevice.
 */
template <typename Callable, typename... Args>
decltype(auto) DispatchDevice(const OutputDevice& rDevice, Callable&& func, Args&&... args)
{
    if (auto* pPDF = dynamic_cast<const PDFWriterImpl*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pPDF, std::forward<Args>(args)...);
    }
    else if (auto* pPrinter = dynamic_cast<const Printer*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pPrinter, std::forward<Args>(args)...);
    }
    else if (auto* pWindow = dynamic_cast<const WindowOutputDevice*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pWindow, std::forward<Args>(args)...);
    }
    else if (auto* pVirDev = dynamic_cast<const VirtualDevice*>(&rDevice))
    {
        return std::forward<Callable>(func)(*pVirDev, std::forward<Args>(args)...);
    }

    return std::forward<Callable>(func)(rDevice, std::forward<Args>(args)...);
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
