$(eval $(call gb_CppunitTest_CppunitTest,i18nutil_digitlocalization))

$(eval $(call gb_CppunitTest_add_exception_objects,i18nutil_digitlocalization, \
    i18nutil/qa/cppunit/test_digitlocalization \
))

$(eval $(call gb_CppunitTest_use_libraries,i18nutil_digitlocalization, \
    cppu \
    cppuhelper \
    sal \
    i18nutil \
    i18nlangtag \
))
