#
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

add_definitions(-Wall -Wextra -Weffc++)


option(BUILD_EXAMPLES "build example compilers" TRUE)


find_package(Boost REQUIRED COMPONENTS program_options)
add_definitions(${Boost_CXX_FLAGS})

find_package(Threads REQUIRED)


include(GNUInstallDirs)

include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/src
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
# example compilers
if(BUILD_EXAMPLES)
	# script compiler library
	add_library(script STATIC
		src/lalr1/symbol.cpp src/lalr1/symbol.h
		src/lalr1/options.cpp src/lalr1/options.h
		src/script/lexer.cpp src/script/lexer.h
		src/script/sym.h src/script/ast.h
		src/script/ast_printer.cpp src/script/ast_printer.h
		src/script/ast_asm.cpp src/script/ast_asm.h
		src/script_vm/opcodes.h src/script_vm/types.h
	)

	target_link_libraries(script ${Boost_LIBRARIES})


	# script vm library
	add_library(script-vm STATIC
		src/script_vm/vm.cpp src/script_vm/vm.h
		src/script_vm/vm_extfuncs.cpp src/script_vm/vm_memdump.cpp
		src/script_vm/opcodes.h src/script_vm/helpers.h
	)

	target_link_libraries(script-vm ${Boost_LIBRARIES}
		$<$<TARGET_EXISTS:Threads::Threads>:Threads::Threads>
	)


	# vm
	add_executable(script_vm src/script_vm/main.cpp)
	target_link_libraries(script_vm script-vm)


	# script compiler generator
	add_executable(script_parsergen
		src/script/script_parsergen.cpp
		src/script/script_grammar.cpp src/script/script_grammar.h
	)
	target_link_libraries(script_parsergen lalr1)

	# script compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		add_executable(script_compiler
			src/script/script.cpp
			src/script/script_grammar.cpp src/script/script_grammar.h
		)
	endif()

	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp")
		target_link_libraries(script_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		target_link_libraries(script_compiler script lalr1-parser script-vm)
	endif()


	# expression compiler generator
	add_executable(expr_parsergen src/expr/expr_parsergen.cpp
		src/expr/expr_grammar.cpp src/expr/expr_grammar.h
	)
	target_link_libraries(expr_parsergen lalr1)

	# expression compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		add_executable(expr_compiler src/expr/expr.cpp
			src/expr/expr_grammar.cpp src/expr/expr_grammar.h
		)
	endif()
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp")
		target_link_libraries(expr_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		target_link_libraries(expr_compiler script lalr1-parser script-vm)
	endif()


	# simple expression compiler generator
	add_executable(expr_simple_parsergen src/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_parsergen PUBLIC -DCREATE_PARSER)
	target_link_libraries(expr_simple_parsergen lalr1)

	# simple expression compiler
	add_executable(expr_simple_compiler src/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_compiler PUBLIC -DRUN_PARSER)
	target_link_libraries(expr_simple_compiler script lalr1-parser script-vm)
endif()
# -----------------------------------------------------------------------------
