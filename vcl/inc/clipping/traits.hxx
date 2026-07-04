/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <type_traits>

namespace
{
// Primary template: Assume no capability
template <typename T, typename = void> struct has_hierarchical_clipping : std::false_type
{
};

// Specialization: Look for the 'static constexpr bool' property
template <typename T>
struct has_hierarchical_clipping<T, std::void_t<decltype(T::is_hierarchical_clipping)>>
    : std::bool_constant<T::is_hierarchical_clipping>
{
};

// Helper variable template
template <typename T>
inline constexpr bool has_hierarchical_clipping_v = has_hierarchical_clipping<T>::value;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
