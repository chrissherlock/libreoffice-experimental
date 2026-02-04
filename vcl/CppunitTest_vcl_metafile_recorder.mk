# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_metafile_recorder))

$(eval $(call gb_CppunitTest_set_include,vcl_metafile_recorder,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
    -I$(SRCDIR)/vcl/source/window \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_metafile_recorder, \
    vcl/qa/cppunit/MetafileRecorderTest \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_metafile_recorder,boost_headers))

$(eval $(call gb_CppunitTest_use_libraries,vcl_metafile_recorder, \
    basegfx \
    comphelper \
    cppu \
    cppuhelper \
    sal \
    salhelper \
    svt \
    test \
    tl \
    unotest \
    vcl \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_metafile_recorder))

$(eval $(call gb_CppunitTest_use_ure,vcl_metafile_recorder))
$(eval $(call gb_CppunitTest_use_vcl,vcl_metafile_recorder))

$(eval $(call gb_CppunitTest_use_components,vcl_metafile_recorder,\
    configmgr/source/configmgr \
    i18npool/util/i18npool \
    ucb/source/core/ucb1 \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_metafile_recorder))

# vim: set noet sw=4 ts=4:
