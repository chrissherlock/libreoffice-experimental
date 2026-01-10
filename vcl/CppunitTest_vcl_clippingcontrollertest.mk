# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,vcl_clippingcontrollertest))

$(eval $(call gb_CppunitTest_set_include,vcl_clippingcontrollertest,\
    $$(INCLUDE) \
    -I$(SRCDIR)/vcl/inc \
    -I$(SRCDIR)/vcl/source/window \
))

$(eval $(call gb_CppunitTest_add_exception_objects,vcl_clippingcontrollertest, \
	vcl/qa/cppunit/clippingcontrollertest \
))

$(eval $(call gb_CppunitTest_use_externals,vcl_clippingcontrollertest,boost_headers))

$(eval $(call gb_CppunitTest_use_libraries,vcl_clippingcontrollertest, \
	basegfx \
	comphelper \
	cppu \
	cppuhelper \
	sal \
	svt \
	test \
	tl \
	unotest \
	vcl \
))

$(eval $(call gb_CppunitTest_use_sdk_api,vcl_clippingcontrollertest))

$(eval $(call gb_CppunitTest_use_ure,vcl_clippingcontrollertest))
$(eval $(call gb_CppunitTest_use_vcl,vcl_clippingcontrollertest))

$(eval $(call gb_CppunitTest_use_components,vcl_clippingcontrollertest,\
	configmgr/source/configmgr \
	i18npool/util/i18npool \
	ucb/source/core/ucb1 \
))

$(eval $(call gb_CppunitTest_use_configuration,vcl_clippingcontrollertest))

# vim: set noet sw=4 ts=4:
