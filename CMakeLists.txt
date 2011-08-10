cmake_minimum_required (VERSION 2.6)
project (vconf)
add_library (vconf vconf.c)

add_definitions (-Wall -Werror -D_GNU_SOURCE)

install (TARGETS vconf ARCHIVE DESTINATION lib)
install (FILES vconf.h DESTINATION include/vconf)
