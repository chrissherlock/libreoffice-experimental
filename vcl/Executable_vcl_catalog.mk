# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Executable_Executable,vcl_catalog))

$(eval $(call gb_Executable_use_api,vcl_catalog,\
    offapi \
    udkapi \
))

$(eval $(call gb_Executable_add_defs,vcl_catalog,\
    -DVCL_INTERNALS \
))

$(eval $(call gb_Executable_set_include,vcl_catalog,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_Executable_use_libraries,vcl_catalog,\
    basegfx \
    comphelper \
    cppu \
    cppuhelper \
    tl \
    sal \
    salhelper \
    fwk \
    i18nlangtag \
    vcl \
))

$(eval $(call gb_Executable_use_vclmain,vcl_catalog))

$(eval $(call gb_Executable_add_exception_objects,vcl_catalog,\
    vcl/workben/vcl_catalog \
))

# vim: set noet sw=4 ts=4:
