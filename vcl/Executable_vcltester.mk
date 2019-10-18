# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Executable_Executable,vcltester))

$(eval $(call gb_Executable_use_api,vcltester,\
    offapi \
    udkapi \
))

$(eval $(call gb_Executable_use_externals,vcltester,\
	boost_headers \
	glm_headers \
	harfbuzz \
))
ifeq ($(DISABLE_GUI),)
$(eval $(call gb_Executable_use_externals,vcltester,\
    epoxy \
))
endif

$(eval $(call gb_Executable_set_include,vcltester,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_Executable_use_libraries,vcltester,\
	basegfx \
    comphelper \
    cppu \
    cppuhelper \
    tl \
    sal \
	salhelper \
    vcl \
))

$(eval $(call gb_Executable_add_exception_objects,vcltester,\
    vcl/workben/vcltester \
))

$(eval $(call gb_Executable_use_static_libraries,vcltester,\
    vclmain \
))

ifeq ($(OS), $(filter LINUX %BSD SOLARIS, $(OS)))
$(eval $(call gb_Executable_add_libs,vcltester,\
	-lm $(DLOPEN_LIBS) \
    -lX11 \
))

$(eval $(call gb_Executable_use_static_libraries,vcltester,\
	glxtest \
))
endif

# vim: set noet sw=4 ts=4:
