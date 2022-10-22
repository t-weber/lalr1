#
# liblalr1
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date june-2020
# @license see 'LICENSE' file
#

#
# profiling:
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-pg ..
#   make -j8
#   valgrind --tool=callgrind --callgrind-out-file=expr.callgrind ./expr_parsergen
#   qcachegrind expr.callgrind
#

cmake_minimum_required(VERSION 3.12)

project(lalr1)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_compile_options(-Wall -Wextra -Weffc++)
#add_compile_options(-fPIC)


option(BUILD_EXAMPLES "build example compilers" TRUE)


find_package(Boost REQUIRED COMPONENTS program_options)
add_definitions(${Boost_CXX_FLAGS})

find_package(Threads REQUIRED)


include(GNUInstallDirs)

include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/src/examples
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}/..
)


# -----------------------------------------------------------------------------
# lalr(1) parser creation library
add_library(lalr1 STATIC)

target_sources(lalr1
	PUBLIC
		${PROJECT_SOURCE_DIR}/src/lalr1/symbol.h
		${PROJECT_SOURCE_DIR}/src/lalr1/collection.h
		${PROJECT_SOURCE_DIR}/src/lalr1/options.h
		${PROJECT_SOURCE_DIR}/src/lalr1/ast.h
	PRIVATE
		${PROJECT_SOURCE_DIR}/src/lalr1/symbol.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/element.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/closure.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/collection.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/options.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/tablegen.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/parsergen.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/element.h
		${PROJECT_SOURCE_DIR}/src/lalr1/closure.h
		${PROJECT_SOURCE_DIR}/src/lalr1/table.h
		${PROJECT_SOURCE_DIR}/src/lalr1/common.h
		${PROJECT_SOURCE_DIR}/src/lalr1/timer.h
)

set_target_properties(lalr1 PROPERTIES PUBLIC_HEADER
	"${PROJECT_SOURCE_DIR}/src/lalr1/symbol.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/element.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/closure.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/collection.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/options.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/table.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/ast.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/common.h;\
${PROJECT_SOURCE_DIR}/src/lalr1/timer.h"
)

#get_target_property(LALR1_HEADERS lalr1 PUBLIC_HEADER)
#message("lalr1 headers: ${LALR1_HEADERS}")

target_compile_options(lalr1 PUBLIC -fPIC)
target_link_libraries(lalr1 ${Boost_LIBRARIES})

install(TARGETS lalr1
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lalr1
)


# lalr(1) parser
add_library(lalr1-parser STATIC)

target_sources(lalr1-parser
	PUBLIC
		${PROJECT_SOURCE_DIR}/src/lalr1/parser.h

	PRIVATE
		${PROJECT_SOURCE_DIR}/src/lalr1/parser.cpp
		${PROJECT_SOURCE_DIR}/src/lalr1/table.h
		${PROJECT_SOURCE_DIR}/src/lalr1/common.h
		${PROJECT_SOURCE_DIR}/src/lalr1/ast.h
)

set_target_properties(lalr1-parser PROPERTIES PUBLIC_HEADER
	${PROJECT_SOURCE_DIR}/src/lalr1/parser.h
)

target_link_libraries(lalr1-parser ${Boost_LIBRARIES})

install(TARGETS lalr1-parser
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lalr1
)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# external bindings
# see: https://cmake.org/cmake/help/latest/module/UseSWIG.html
find_package(SWIG REQUIRED COMPONENTS python)
find_package(Python REQUIRED COMPONENTS Development)
if(SWIG_FOUND AND SWIG_python_FOUND AND Python_FOUND)
	cmake_policy(SET CMP0078 NEW)
	cmake_policy(SET CMP0086 NEW)
	include(${SWIG_USE_FILE})

	set_property(SOURCE ${PROJECT_SOURCE_DIR}/src/lalr1_py/lalr1.i PROPERTY CPLUSPLUS TRUE)
	swig_add_library(lalr1_py TYPE MODULE LANGUAGE python SOURCES ${PROJECT_SOURCE_DIR}/src/lalr1_py/lalr1.i)
	target_link_libraries(lalr1_py lalr1 Python::Python)

	set_source_files_properties(
		"${PROJECT_BINARY_DIR}/CMakeFiles/lalr1_py.dir/lalr1PYTHON_wrap.cxx"
		PROPERTIES COMPILE_OPTIONS "-Wno-extra;-Wno-effc++")
else()
	message("Disabling Swig/Python bindings.")
endif()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# example compilers
if(BUILD_EXAMPLES)
	# script compiler library
	add_library(script STATIC
		src/lalr1/symbol.cpp src/lalr1/symbol.h
		src/lalr1/options.cpp src/lalr1/options.h
		src/examples/script/lexer.cpp src/examples/script/lexer.h
		src/examples/script/ast_printer.cpp src/examples/script/ast_printer.h
		src/examples/script/ast_asm.cpp src/examples/script/ast_asm.h
		src/examples/script_vm/opcodes.h
		src/examples/script_vm/types.h
		src/examples/script/sym.h
		src/examples/script/ast.h
	)

	target_link_libraries(script ${Boost_LIBRARIES})


	# script vm library
	add_library(script-vm STATIC
		src/examples/script_vm/vm.cpp
		src/examples/script_vm/vm.h
		src/examples/script_vm/vm_extfuncs.cpp
		src/examples/script_vm/vm_memdump.cpp
		src/examples/script_vm/opcodes.h
		src/examples/script_vm/helpers.h
	)

	target_link_libraries(script-vm ${Boost_LIBRARIES}
		$<$<TARGET_EXISTS:Threads::Threads>:Threads::Threads>
	)


	# vm
	add_executable(script_vm src/examples/script_vm/main.cpp)
	target_link_libraries(script_vm script-vm)


	# script compiler generator
	add_executable(script_parsergen
		src/examples/script/script_parsergen.cpp
		src/examples/script/script_grammar.cpp src/examples/script/script_grammar.h
	)
	target_link_libraries(script_parsergen lalr1)

	# script compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		add_executable(script_compiler
			src/examples/script/script.cpp
			src/examples/script/script_grammar.cpp src/examples/script/script_grammar.h
		)
	endif()

	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp")
		target_link_libraries(script_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		target_link_libraries(script_compiler script lalr1-parser script-vm)
	endif()


	# expression compiler generator
	add_executable(expr_parsergen src/examples/expr/expr_parsergen.cpp
		src/examples/expr/expr_grammar.cpp src/examples/expr/expr_grammar.h
	)
	target_link_libraries(expr_parsergen lalr1)

	# expression compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		add_executable(expr_compiler src/examples/expr/expr.cpp
			src/examples/expr/expr_grammar.cpp src/examples/expr/expr_grammar.h
		)
	endif()
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp")
		target_link_libraries(expr_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		target_link_libraries(expr_compiler script lalr1-parser script-vm)
	endif()


	# simple expression compiler generator
	add_executable(expr_simple_parsergen src/examples/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_parsergen PUBLIC -DCREATE_PARSER)
	target_link_libraries(expr_simple_parsergen lalr1)

	# simple expression compiler
	add_executable(expr_simple_compiler src/examples/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_compiler PUBLIC -DRUN_PARSER)
	target_link_libraries(expr_simple_compiler script lalr1-parser script-vm)
endif()
# -----------------------------------------------------------------------------

