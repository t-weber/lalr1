/**
 * swig native interface
 * @author Tobias Weber
 * @date oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *	* http://www.swig.org/tutorial.html
 *	* http://www.swig.org/Doc4.0/SWIGPlus.html#SWIGPlus
 *
 * swig -v -c++ -python lalr1.i
 * g++ -std=c++20 -I../../ -I/usr/include/python3.10 -o _lalr1.so -shared -fpic lalr1_wrap.cxx
 */

%module lalr1
%{
	#include "src/lalr1/symbol.h"
	#include "src/lalr1/element.h"
	#include "src/lalr1/closure.h"
	#include "src/lalr1/collection.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_deque.i>
%include <std_unordered_map.i>
%include <std_unordered_set.i>
%include <std_map.i>
%include <std_set.i>

#include "src/lalr1/symbol.h"
#include "src/lalr1/element.h"
#include "src/lalr1/closure.h"
#include "src/lalr1/collection.h"
