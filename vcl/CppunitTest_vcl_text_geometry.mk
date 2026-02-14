# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_text_geometry))

$(eval $(call gb_CppunitTest_set_include,vcl_text_geometry,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_text_geometry, \
    vcl/qa/cppunit/text/TextGeometryTest \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_text_geometry,\
    boost_headers \
    harfbuzz \
))

ifeq ($(SYSTEM_ICU),TRUE)
$(eval $(call gb_CppunitTest_use_externals,vcl_text_geometry,\
    icuuc \
))
else
$(eval $(call gb_CppunitTest_use_externals,vcl_text_geometry,\
    icu_headers \
))
endif

$(eval $(call gb_CppunitTest_use_libraries,vcl_text_geometry, \
    basegfx \
    comphelper \
    cppu \
    cppuhelper \
    i18nlangtag \
    i18nutil \
    sal \
    salhelper \
    svt \
    test \
    tl \
    unotest \
    vcl \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_text_geometry))

$(eval $(call gb_CppunitTest_use_ure,vcl_text_geometry))
$(eval $(call gb_CppunitTest_use_vcl,vcl_text_geometry))

$(eval $(call gb_CppunitTest_use_components,vcl_text_geometry,\
    configmgr/source/configmgr \
    i18npool/util/i18npool \
    ucb/source/core/ucb1 \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_text_geometry))

$(eval $(call gb_CppunitTest_use_more_fonts,vcl_text_geometry))

# vim: set noet sw=4 ts=4:
