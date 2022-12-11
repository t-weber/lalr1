#
# liblalr1
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date dec-2022
# @license see 'LICENSE' file
#

find_library(LibLalr1_LIBRARY
	NAMES lalr1
	HINTS /usr/local/lib
	DOC "lalr1 library"
)


find_path(LibLalr1_INCLUDE_DIR
	NAMES lalr1/symbol.h lalr1/element.h lalr1/closure.h lalr1/collection.h
	HINTS /usr/local/include
	DOC "lalr1 library"
)


find_package_handle_standard_args(LibLalr1
	FOUND_VAR LibLalr1_FOUND
	REQUIRED_VARS LibLalr1_LIBRARY LibLalr1_INCLUDE_DIR
)


if(LibLalr1_FOUND)
	set(LibLalr1_LIBRARIES ${LibLalr1_LIBRARY})
	set(LibLalr1_INCLUDE_DIRS ${LibLalr1_INCLUDE_DIR})

	message("liblalr1 library: ${LibLalr1_LIBRARIES}.")
	message("liblalr1 include directory: ${LibLalr1_INCLUDE_DIRS}.")
else()
	message("liblalr1 was not found.")
endif()
