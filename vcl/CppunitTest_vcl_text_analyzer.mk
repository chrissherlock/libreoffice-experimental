# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_text_analyzer))

$(eval $(call gb_CppunitTest_set_include,vcl_text_analyzer,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_text_analyzer, \
    vcl/qa/cppunit/text/TextAnalyzerTest \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_text_analyzer,\
    boost_headers \
    harfbuzz \
))

ifeq ($(SYSTEM_ICU),TRUE)
$(eval $(call gb_CppunitTest_use_externals,vcl_text_analyzer,\
    icuuc \
))
else
$(eval $(call gb_CppunitTest_use_externals,vcl_text_analyzer,\
    icu_headers \
))
endif

$(eval $(call gb_CppunitTest_use_libraries,vcl_text_analyzer, \
    comphelper \
    cppu \
    cppuhelper \
    i18nlangtag \
    i18nutil \
    sal \
    svt \
    test \
    tl \
    unotest \
    vcl \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_text_analyzer))

$(eval $(call gb_CppunitTest_use_ure,vcl_text_analyzer))
$(eval $(call gb_CppunitTest_use_vcl,vcl_text_analyzer))

$(eval $(call gb_CppunitTest_use_components,vcl_text_analyzer,\
    configmgr/source/configmgr \
    i18npool/util/i18npool \
    ucb/source/core/ucb1 \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_text_analyzer))

$(eval $(call gb_CppunitTest_use_more_fonts,vcl_text_analyzer))

# vim: set noet sw=4 ts=4:
