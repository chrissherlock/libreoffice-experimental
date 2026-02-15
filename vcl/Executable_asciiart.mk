$(eval $(call gb_Executable_Executable,asciiart))

$(eval $(call gb_Executable_use_api,asciiart,\
    offapi \
    udkapi \
))

$(eval $(call gb_Executable_use_libraries,asciiart,\
    comphelper \
    cppu \
    cppuhelper \
    sal \
    tl \
    vcl \
))

$(eval $(call gb_Executable_add_exception_objects,asciiart,\
    vcl/workben/asciiart \
))

# vim: set noet sw=4 ts=4:
