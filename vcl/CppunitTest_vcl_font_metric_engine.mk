# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_text_font_metric_engine))

$(eval $(call gb_CppunitTest_set_include,vcl_text_font_metric_engine,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_text_font_metric_engine, \
    vcl/qa/cppunit/text/FontMetricEngineTest \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_text_font_metric_engine,\
    boost_headers \
    harfbuzz \
))

ifeq ($(SYSTEM_ICU),TRUE)
$(eval $(call gb_CppunitTest_use_externals,vcl_text_font_metric_engine,\
    icuuc \
))
else
$(eval $(call gb_CppunitTest_use_externals,vcl_text_font_metric_engine,\
    icu_headers \
))
endif

$(eval $(call gb_CppunitTest_use_libraries,vcl_text_font_metric_engine, \
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

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_text_font_metric_engine))

$(eval $(call gb_CppunitTest_use_ure,vcl_text_font_metric_engine))
$(eval $(call gb_CppunitTest_use_vcl,vcl_text_font_metric_engine))

$(eval $(call gb_CppunitTest_use_components,vcl_text_font_metric_engine,\
    configmgr/source/configmgr \
    i18npool/util/i18npool \
    ucb/source/core/ucb1 \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_text_font_metric_engine))

$(eval $(call gb_CppunitTest_use_more_fonts,vcl_text_font_metric_engine))

# vim: set noet sw=4 ts=4:
