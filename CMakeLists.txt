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
#   valgrind --tool=callgrind --callgrind-out-file=expr.callgrind ./expr_create
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


include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/src
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}/..
)


# -----------------------------------------------------------------------------
# lalr(1) parser creation library
add_library(lalr1 STATIC
	src/lalr1/symbol.cpp src/lalr1/symbol.h
	src/lalr1/element.cpp src/lalr1/element.h
	src/lalr1/closure.cpp src/lalr1/closure.h
	src/lalr1/collection.cpp src/lalr1/collection.h
	src/lalr1/table.h src/lalr1/common.h
)

target_link_libraries(lalr1 ${Boost_LIBRARIES})


# lalr(1) parser
add_library(lalr1-parser STATIC
	src/lalr1/parser.cpp src/lalr1/parser.h
	src/lalr1/table.h src/lalr1/common.h src/lalr1/ast.h
)

target_link_libraries(lalr1-parser ${Boost_LIBRARIES})
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# example compilers
if(BUILD_EXAMPLES)
	# script compiler library
	add_library(script STATIC
		src/lalr1/symbol.cpp src/lalr1/symbol.h
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


	# script compiler
	add_executable(script_create src/script/script.cpp)
	target_compile_definitions(script_create PUBLIC -DCREATE_PARSER)
	target_link_libraries(script_create lalr1)

	add_executable(script_run src/script/script.cpp)
	target_compile_definitions(script_run PUBLIC -DRUN_PARSER)
	target_link_libraries(script_run script lalr1-parser script-vm)


	# expression compiler
	add_executable(expr_create src/script/expr.cpp)
	target_compile_definitions(expr_create PUBLIC -DCREATE_PARSER)
	target_link_libraries(expr_create lalr1)

	add_executable(expr_run src/script/expr.cpp)
	target_compile_definitions(expr_run PUBLIC -DRUN_PARSER)
	target_link_libraries(expr_run script lalr1-parser script-vm)


	# simple expression compiler
	add_executable(expr_simple_create src/script/expr_simple.cpp)
	target_compile_definitions(expr_simple_create PUBLIC -DCREATE_PARSER)
	target_link_libraries(expr_simple_create lalr1)

	add_executable(expr_simple_run src/script/expr_simple.cpp)
	target_compile_definitions(expr_simple_run PUBLIC -DRUN_PARSER)
	target_link_libraries(expr_simple_run script lalr1-parser script-vm)
endif()
# -----------------------------------------------------------------------------
