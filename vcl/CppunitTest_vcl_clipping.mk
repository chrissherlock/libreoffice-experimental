# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_clipping))

$(eval $(call gb_CppunitTest_set_include,vcl_clipping,\
    -I$(SRCDIR)/vcl/inc \
    $$(INCLUDE) \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_clipping, \
	vcl/qa/cppunit/clipping/vclclippingtest \
))

$(eval $(call gb_CppunitTest_use_libraries,vcl_clipping, \
	test \
	sal \
	tl \
	unotest \
	vcl \
	basegfx \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_clipping, \
	boost_headers \
	harfbuzz \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_clipping))

$(eval $(call gb_CppunitTest_use_ure,vcl_clipping))
$(eval $(call gb_CppunitTest_use_vcl,vcl_clipping))

$(eval $(call gb_CppunitTest_use_rdb,vcl_clipping,services))

$(eval $(call gb_CppunitTest_use_configuration,vcl_clipping))

# vim: set noet sw=4 ts=4:
