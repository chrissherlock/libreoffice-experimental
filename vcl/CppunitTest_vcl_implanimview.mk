# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_implanimview))

$(eval $(call gb_CppunitTest_set_include,vcl_implanimview,\
    -I$(SRCDIR)/vcl/inc \
    $$(INCLUDE) \
))
$(eval $(call gb_CppunitTest_add_exception_objects,vcl_implanimview, \
	vcl/qa/cppunit/implanimview \
))

$(eval $(call gb_CppunitTest_use_libraries,vcl_implanimview, \
	test \
	unotest \
	vcl \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_implanimview, \
	boost_headers \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_implanimview))

$(eval $(call gb_CppunitTest_use_ure,vcl_implanimview))
$(eval $(call gb_CppunitTest_use_vcl,vcl_implanimview))

$(eval $(call gb_CppunitTest_use_components,vcl_implanimview,\
	configmgr/source/configmgr \
	i18npool/util/i18npool \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_implanimview))

# vim: set noet sw=4 ts=4:
