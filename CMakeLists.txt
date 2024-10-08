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
#cmake_policy(SET CMP0167 NEW)

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
	${PROJECT_SOURCE_DIR}/src/core
	${PROJECT_SOURCE_DIR}/examples
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}/..
)


# -----------------------------------------------------------------------------
# lalr(1) parser creation library
add_library(lalr1 STATIC)

target_sources(lalr1
	PUBLIC
		${PROJECT_SOURCE_DIR}/src/core/symbol.h
		${PROJECT_SOURCE_DIR}/src/core/collection.h
		${PROJECT_SOURCE_DIR}/src/core/tablegen.h
		${PROJECT_SOURCE_DIR}/src/core/parsergen.h
		${PROJECT_SOURCE_DIR}/src/core/options.h
		${PROJECT_SOURCE_DIR}/src/core/genoptions.h
		${PROJECT_SOURCE_DIR}/src/core/ast.h
		${PROJECT_SOURCE_DIR}/src/core/types.h
		${PROJECT_SOURCE_DIR}/src/core/element.h
		${PROJECT_SOURCE_DIR}/src/core/closure.h
		${PROJECT_SOURCE_DIR}/src/core/table.h
		${PROJECT_SOURCE_DIR}/src/core/common.h
		${PROJECT_SOURCE_DIR}/src/core/timer.h
		${PROJECT_SOURCE_DIR}/src/core/stack.h
		${PROJECT_SOURCE_DIR}/src/core/conflicts.h
		${PROJECT_SOURCE_DIR}/src/export/tableexport.h
		${PROJECT_SOURCE_DIR}/src/export/tableexport_java.h
		${PROJECT_SOURCE_DIR}/src/export/tableexport_rs.h
		${PROJECT_SOURCE_DIR}/src/export/tableexport_json.h
		${PROJECT_SOURCE_DIR}/src/export/tableexport_toml.h

	PRIVATE
		${PROJECT_SOURCE_DIR}/src/core/symbol.cpp
		${PROJECT_SOURCE_DIR}/src/core/element.cpp
		${PROJECT_SOURCE_DIR}/src/core/closure.cpp
		${PROJECT_SOURCE_DIR}/src/core/collection.cpp
		${PROJECT_SOURCE_DIR}/src/core/options.cpp
		${PROJECT_SOURCE_DIR}/src/core/genoptions.cpp
		${PROJECT_SOURCE_DIR}/src/core/parsergen.cpp
		${PROJECT_SOURCE_DIR}/src/core/tablegen.cpp
		${PROJECT_SOURCE_DIR}/src/core/conflicts.cpp
		${PROJECT_SOURCE_DIR}/src/export/tableexport.cpp
		${PROJECT_SOURCE_DIR}/src/export/tableexport_java.cpp
		${PROJECT_SOURCE_DIR}/src/export/tableexport_rs.cpp
		${PROJECT_SOURCE_DIR}/src/export/tableexport_json.cpp
		${PROJECT_SOURCE_DIR}/src/export/tableexport_toml.cpp
)

set_target_properties(lalr1 PROPERTIES PUBLIC_HEADER
	"${PROJECT_SOURCE_DIR}/src/core/symbol.h;\
${PROJECT_SOURCE_DIR}/src/core/element.h;\
${PROJECT_SOURCE_DIR}/src/core/closure.h;\
${PROJECT_SOURCE_DIR}/src/core/collection.h;\
${PROJECT_SOURCE_DIR}/src/core/options.h;\
${PROJECT_SOURCE_DIR}/src/core/genoptions.h;\
${PROJECT_SOURCE_DIR}/src/core/tablegen.h;\
${PROJECT_SOURCE_DIR}/src/core/parsergen.h;\
${PROJECT_SOURCE_DIR}/src/core/table.h;\
${PROJECT_SOURCE_DIR}/src/core/ast.h;\
${PROJECT_SOURCE_DIR}/src/core/common.h;\
${PROJECT_SOURCE_DIR}/src/core/types.h;\
${PROJECT_SOURCE_DIR}/src/core/stack.h;\
${PROJECT_SOURCE_DIR}/src/core/conflicts.h;\
${PROJECT_SOURCE_DIR}/src/core/timer.h;\
${PROJECT_SOURCE_DIR}/src/export/tableexport.h;\
${PROJECT_SOURCE_DIR}/src/export/tableexport_java.h;\
${PROJECT_SOURCE_DIR}/src/export/tableexport_rs.h;\
${PROJECT_SOURCE_DIR}/src/export/tableexport_json.h;\
${PROJECT_SOURCE_DIR}/src/export/tableexport_toml.h"
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
		${PROJECT_SOURCE_DIR}/src/core/parser.h
		${PROJECT_SOURCE_DIR}/src/core/types.h
		${PROJECT_SOURCE_DIR}/src/core/ast.h
		${PROJECT_SOURCE_DIR}/src/core/table.h
		${PROJECT_SOURCE_DIR}/src/core/common.h
		${PROJECT_SOURCE_DIR}/src/core/stack.h
		${PROJECT_SOURCE_DIR}/src/core/conflicts.h

	PRIVATE
		${PROJECT_SOURCE_DIR}/src/core/parser.cpp
		${PROJECT_SOURCE_DIR}/src/core/conflicts.cpp
)

set_target_properties(lalr1-parser PROPERTIES PUBLIC_HEADER
	${PROJECT_SOURCE_DIR}/src/core/parser.h
)

target_link_libraries(lalr1-parser ${Boost_LIBRARIES})

install(TARGETS lalr1-parser
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lalr1
)


# cmake file
install(FILES ${PROJECT_SOURCE_DIR}/FindLibLalr1.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake
)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# external bindings
# see: https://cmake.org/cmake/help/latest/module/UseSWIG.html
find_package(SWIG COMPONENTS python java)
find_package(Python COMPONENTS Development Interpreter)
find_package(Java COMPONENTS Development Runtime)
find_package(JNI)

set(USE_SWIG_PY TRUE)
set(USE_SWIG_JAVA FALSE)

if(SWIG_FOUND)
	cmake_policy(SET CMP0078 NEW)
	cmake_policy(SET CMP0086 NEW)
	include(${SWIG_USE_FILE})

	set_property(SOURCE ${PROJECT_SOURCE_DIR}/src/modules/lalr1.i PROPERTY CPLUSPLUS TRUE)

	if(SWIG_python_FOUND AND Python_FOUND AND USE_SWIG_PY)
		swig_add_library(lalr1_py TYPE MODULE LANGUAGE python SOURCES ${PROJECT_SOURCE_DIR}/src/modules/lalr1.i)
		target_link_libraries(lalr1_py lalr1 Python::Python)

		set_source_files_properties(
			"${PROJECT_BINARY_DIR}/CMakeFiles/lalr1_py.dir/lalr1PYTHON_wrap.cxx"
			PROPERTIES COMPILE_OPTIONS "-Wno-extra;-Wno-effc++")

		get_property(LALR1_PY_WRAPPER TARGET lalr1_py PROPERTY SWIG_SUPPORT_FILES)
		message("Generating python wrapper file(s): ${LALR1_PY_WRAPPER}.")

		#add_custom_command(TARGET lalr1_py POST_BUILD
		#	COMMAND echo -e "Generated python native library: $<TARGET_FILE:lalr1_py>.")

		message("Python install dirs: \"${Python_SITEARCH}\", \"${Python_SITELIB}\".")
		install(TARGETS lalr1_py DESTINATION ${Python_SITEARCH})
		install(FILES ${LALR1_PY_WRAPPER} DESTINATION ${Python_SITEARCH})
	endif()

	if(SWIG_java_FOUND AND Java_FOUND AND JNI_FOUND AND USE_SWIG_JAVA)
		swig_add_library(lalr1_java TYPE MODULE LANGUAGE java SOURCES ${PROJECT_SOURCE_DIR}/src/modules/lalr1.i)
		target_include_directories(lalr1_java PUBLIC ${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2} ${JNI_INCLUDE_DIRS})
		target_link_libraries(lalr1_java lalr1 ${JNI_LIBRARIES})

		set_source_files_properties(
			"${PROJECT_BINARY_DIR}/CMakeFiles/lalr1_java.dir/lalr1JAVA_wrap.cxx"
			PROPERTIES COMPILE_OPTIONS "-Wno-extra;-Wno-effc++;-Wno-strict-aliasing")

		get_property(LALR1_JAVA_WRAPPER TARGET lalr1_java PROPERTY SWIG_SUPPORT_FILES)
		message("Generating java wrapper file(s): ${LALR1_JAVA_WRAPPER}.")

		#add_custom_command(TARGET lalr1_java POST_BUILD
		#	COMMAND echo -e "Generated java native library: $<TARGET_FILE:lalr1_java>.")
	endif()
else()
	message("Disabling Swig bindings.")
endif()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# example compilers
if(BUILD_EXAMPLES)
	# script compiler library
	add_library(script STATIC
		src/core/symbol.cpp src/core/symbol.h
		src/core/options.cpp src/core/options.h
		src/core/genoptions.cpp src/core/genoptions.h
		examples/script/lexer.cpp examples/script/lexer.h
		examples/script/ast_printer.cpp examples/script/ast_printer.h
		examples/script/ast_asm.cpp examples/script/ast_asm.h
		examples/script/ast_opt.cpp examples/script/ast_opt.h
		examples/script_vm/opcodes.h
		examples/script_vm/types.h
		examples/script/sym.cpp examples/script/sym.h
		examples/script/ast.h
	)

	target_link_libraries(script ${Boost_LIBRARIES})


	# script vm library
	add_library(script-vm STATIC
		examples/script_vm/vm.cpp
		examples/script_vm/vm.h
		examples/script_vm/vm_extfuncs.cpp
		examples/script_vm/vm_memdump.cpp
		examples/script_vm/opcodes.h
		examples/script_vm/helpers.h
	)

	target_link_libraries(script-vm ${Boost_LIBRARIES}
		$<$<TARGET_EXISTS:Threads::Threads>:Threads::Threads>
	)


	# vm
	add_executable(script_vm examples/script_vm/main.cpp)
	target_link_libraries(script_vm script-vm)


	# script compiler generator
	add_executable(script_parsergen
		examples/script/parsergen.cpp
		examples/script/grammar.cpp examples/script/grammar.h
		examples/script/grammar_common.cpp examples/script/grammar_common.h
	)
	target_link_libraries(script_parsergen lalr1)

	# script compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		add_executable(script_compiler
			examples/script/script.cpp
			examples/script/grammar.cpp examples/script/grammar.h
			examples/script/grammar_common.cpp examples/script/grammar_common.h
		)
	endif()

	if(EXISTS "${CMAKE_BINARY_DIR}/script_parser.cpp")
		target_link_libraries(script_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/script.tab")
		target_link_libraries(script_compiler script lalr1-parser script-vm)
	endif()


	# expression compiler generator
	add_executable(expr_parsergen examples/expr/parsergen.cpp
		examples/expr/grammar.cpp examples/expr/grammar.h
		examples/script/grammar_common.cpp examples/script/grammar_common.h
	)
	target_link_libraries(expr_parsergen lalr1)

	# expression compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		add_executable(expr_compiler examples/expr/expr.cpp
			examples/expr/grammar.cpp examples/expr/grammar.h
			examples/expr/printer.cpp examples/expr/printer.h
			examples/script/grammar_common.cpp examples/script/grammar_common.h
		)
	endif()
	if(EXISTS "${CMAKE_BINARY_DIR}/expr_parser.cpp")
		target_link_libraries(expr_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/expr.tab")
		target_link_libraries(expr_compiler script lalr1-parser script-vm)
	endif()


	# differentiating expression compiler generator
	add_executable(diff_parsergen examples/diff/parsergen.cpp
		examples/diff/grammar.cpp examples/diff/grammar.h
		examples/script/grammar_common.cpp examples/script/grammar_common.h
	)
	target_link_libraries(diff_parsergen lalr1)

	# differentiating expression compiler
	if(EXISTS "${CMAKE_BINARY_DIR}/diff_parser.cpp" OR EXISTS "${CMAKE_BINARY_DIR}/diff.tab")
		add_executable(diff_compiler examples/diff/diff.cpp
			examples/diff/grammar.cpp examples/diff/grammar.h
			examples/expr/printer.cpp examples/expr/printer.h
			examples/script/grammar_common.cpp examples/script/grammar_common.h
		)
	endif()
	if(EXISTS "${CMAKE_BINARY_DIR}/diff_parser.cpp")
		target_link_libraries(diff_compiler script script-vm)
	elseif(EXISTS "${CMAKE_BINARY_DIR}/diff.tab")
		target_link_libraries(diff_compiler script lalr1-parser script-vm)
	endif()


	# simple expression compiler generator
	add_executable(expr_simple_parsergen examples/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_parsergen PUBLIC -DCREATE_PARSER)
	target_link_libraries(expr_simple_parsergen lalr1)

	# simple expression compiler
	add_executable(expr_simple_compiler examples/expr/expr_simple.cpp)
	target_compile_definitions(expr_simple_compiler PUBLIC -DRUN_PARSER)
	target_link_libraries(expr_simple_compiler script lalr1-parser script-vm)
endif()
# -----------------------------------------------------------------------------
