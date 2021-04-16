/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <sal/log.hxx>

#include <makefraction.hxx>

/*
Reduces accuracy until it is a fraction (should become
ctor fraction once); we could also do this with BigInts
*/

Fraction ImplMakeFraction(tools::Long nN1, tools::Long nN2, tools::Long nD1, tools::Long nD2)
{
    if (nD1 == 0
        || nD2 == 0) //under these bad circumstances the following while loop will be endless
    {
        SAL_WARN("vcl.gdi", "Invalid parameter for ImplMakeFraction");
        return Fraction(1, 1);
    }

    tools::Long i = 1;

    if (nN1 < 0)
    {
        i = -i;
        nN1 = -nN1;
    }
    if (nN2 < 0)
    {
        i = -i;
        nN2 = -nN2;
    }
    if (nD1 < 0)
    {
        i = -i;
        nD1 = -nD1;
    }
    if (nD2 < 0)
    {
        i = -i;
        nD2 = -nD2;
    }
    // all positive; i sign

    Fraction aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);

    while (!aF.IsValid())
    {
        if (nN1 > nN2)
            nN1 = (nN1 + 1) / 2;
        else
            nN2 = (nN2 + 1) / 2;
        if (nD1 > nD2)
            nD1 = (nD1 + 1) / 2;
        else
            nD2 = (nD2 + 1) / 2;

        aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);
    }

    aF.ReduceInaccurate(32);
    return aF;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
