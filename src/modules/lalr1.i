/**
 * swig native interface
 * @author Tobias Weber
 * @date oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *	* http://www.swig.org/tutorial.html
 *	* http://www.swig.org/Doc4.0/SWIGPlus.html#SWIGPlus
 *	* https://cmake.org/cmake/help/latest/module/UseSWIG.html
 */

// --------------------------------------------------------------------------------
// standard library
// --------------------------------------------------------------------------------
%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_vector.i>
%include <std_list.i>
%include <std_deque.i>
%include <std_unordered_map.i>
%include <std_unordered_set.i>
%include <std_map.i>
%include <std_set.i>
//%include <std_iostream.i>

%{
	#include <memory>
%}

namespace std
{
	// see: https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
	template<class Derived> class enable_shared_from_this
	{
		public:
			std::shared_ptr<Derived> shared_from_this();
			std::shared_ptr<const Derived> shared_from_this() const;

			std::weak_ptr<Derived> weak_from_this() noexcept;
			std::weak_ptr<const Derived> weak_from_this() const noexcept;

		protected:
			constexpr enable_shared_from_this() noexcept;
			enable_shared_from_this(const enable_shared_from_this<Derived>&) noexcept;
			~enable_shared_from_this();

			enable_shared_from_this<Derived>&
			operator=(const enable_shared_from_this<Derived>&) noexcept;
	};
}
// --------------------------------------------------------------------------------


// --------------------------------------------------------------------------------
// lalr1 module
// --------------------------------------------------------------------------------
%module lalr1
%{
	#include "src/core/types.h"
	#include "src/core/options.h"
	#include "src/core/genoptions.h"
	#include "src/core/types.h"
	#include "src/core/symbol.h"
	#include "src/core/element.h"
	#include "src/core/closure.h"
	#include "src/core/collection.h"
	#include "src/core/tablegen.h"
	#include "src/core/parsergen.h"

	using namespace lalr1;
%}

typedef lalr1::Symbol Symbol;
typedef lalr1::Word Word;
typedef lalr1::Element Element;
typedef lalr1::Closure Closure;
typedef lalr1::Collection Collection;
typedef lalr1::TableGen TableGen;
typedef lalr1::ParserGen ParserGen;
typedef lalr1::GenOptions GenOptions;

typedef std::shared_ptr<Symbol> SymbolPtr;
typedef std::shared_ptr<Word> WordPtr;
typedef std::shared_ptr<Element> ElementPtr;
typedef std::shared_ptr<Closure> ClosurePtr;
typedef std::shared_ptr<Collection> CollectionPtr;
typedef std::shared_ptr<TableGen> TableGenPtr;
typedef std::shared_ptr<ParserGen> ParserGenPtr;
typedef std::shared_ptr<GenOptions> GenOptionsPtr;

%shared_ptr(std::enable_shared_from_this<Symbol>);
%shared_ptr(std::enable_shared_from_this<Word>);
%shared_ptr(std::enable_shared_from_this<Element>);
%shared_ptr(std::enable_shared_from_this<Closure>);
%shared_ptr(std::enable_shared_from_this<Collection>);

%shared_ptr(lalr1::Symbol);
%shared_ptr(lalr1::Terminal);
%shared_ptr(lalr1::NonTerminal);
%shared_ptr(lalr1::Word);
%shared_ptr(lalr1::Element);
%shared_ptr(lalr1::Closure);
%shared_ptr(lalr1::Collection);
%shared_ptr(lalr1::TableGen);
%shared_ptr(lalr1::ParserGen);
%shared_ptr(lalr1::GenOptions);

%template(SymbolESFT) std::enable_shared_from_this<Symbol>;
%template(WordESFT) std::enable_shared_from_this<Word>;
%template(ElementESFT) std::enable_shared_from_this<Element>;
%template(ClosureESFT) std::enable_shared_from_this<Closure>;
%template(CollectionESFT) std::enable_shared_from_this<Collection>;

%template(SymbolVec) std::vector<SymbolPtr>;
//%template(ElementLst) std::list<ElementPtr>;
//%template(ClosureLst) std::list<ClosurePtr>;

%include "src/core/types.h"
%include "src/core/options.h"
%include "src/core/genoptions.h"
%include "src/core/symbol.h"
%include "src/core/element.h"
%include "src/core/closure.h"
%include "src/core/collection.h"
%include "src/core/tablegen.h"
%include "src/core/parsergen.h"

using namespace lalr1;
// --------------------------------------------------------------------------------


// --------------------------------------------------------------------------------
// helper functions
// --------------------------------------------------------------------------------
%inline
%{
	TerminalPtr make_terminal(std::size_t id, const std::string& strid)
	{
		return std::make_shared<Terminal>(t_symbol_id(id), strid);
	}

	TerminalPtr make_terminal(const std::string& ch, const std::string& strid)
	{
		return std::make_shared<Terminal>(t_symbol_id(ch[0]), strid);
	}

	NonTerminalPtr make_nonterminal(std::size_t id, const std::string& strid)
	{
		return std::make_shared<NonTerminal>(t_symbol_id(id), strid);
	}

	WordPtr make_word(const std::vector<SymbolPtr>& syms)
	{
		WordPtr word = std::make_shared<Word>();
		for(const SymbolPtr& sym : syms)
			word->AddSymbol(sym);
		return word;
	}

	CollectionPtr make_collection(const NonTerminalPtr& start)
	{
		ElementPtr elem = std::make_shared<Element>(
			start, 0, 0, Terminal::t_terminalset{{ g_end }});

		ClosurePtr closure = std::make_shared<Closure>();
		closure->AddElement(elem);

		CollectionPtr coll = std::make_shared<Collection>(closure);
		return coll;
	}

	TableGenPtr make_tablegen(const CollectionPtr& coll, const NonTerminalPtr& start)
	{
		TableGenPtr tablegen = std::make_shared<TableGen>(coll);

		if(std::optional<t_semantic_id> sema = start->GetSemanticRule(0); sema)
			tablegen->SetAcceptingRule(*sema);

		return tablegen;
	}

	ParserGenPtr make_parsergen(const CollectionPtr& coll, const NonTerminalPtr& start)
	{
		ParserGenPtr parsergen = std::make_shared<ParserGen>(coll);

		if(std::optional<t_semantic_id> sema = start->GetSemanticRule(0); sema)
			parsergen->SetAcceptingRule(*sema);

		return parsergen;
	}
%}
// --------------------------------------------------------------------------------
