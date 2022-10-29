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
	#include "src/core/symbol.h"
	#include "src/core/element.h"
	#include "src/core/closure.h"
	#include "src/core/collection.h"
	#include "src/core/tableexport.h"
%}

%shared_ptr(std::enable_shared_from_this<Symbol>);
%shared_ptr(std::enable_shared_from_this<Word>);
%shared_ptr(std::enable_shared_from_this<Element>);
%shared_ptr(std::enable_shared_from_this<Closure>);
%shared_ptr(std::enable_shared_from_this<Collection>);
%shared_ptr(Symbol);
%shared_ptr(Terminal);
%shared_ptr(NonTerminal);
%shared_ptr(Word);
%shared_ptr(Element);
%shared_ptr(Closure);
%shared_ptr(Collection);

%template(SymbolESFT) std::enable_shared_from_this<Symbol>;
%template(WordESFT) std::enable_shared_from_this<Word>;
%template(ElementESFT) std::enable_shared_from_this<Element>;
%template(ClosureESFT) std::enable_shared_from_this<Closure>;
%template(CollectionESFT) std::enable_shared_from_this<Collection>;

typedef std::shared_ptr<Symbol> SymbolPtr;
typedef std::shared_ptr<Word> WordPtr;
typedef std::shared_ptr<Element> ElementPtr;
typedef std::shared_ptr<Closure> ClosurePtr;
typedef std::shared_ptr<Collection> CollectionPtr;

%template(SymbolVec) std::vector<SymbolPtr>;
//%template(ElementLst) std::list<ElementPtr>;
//%template(ClosureLst) std::list<ClosurePtr>;

%include "src/core/types.h"
%include "src/core/symbol.h"
%include "src/core/element.h"
%include "src/core/closure.h"
%include "src/core/collection.h"
%include "src/core/tableexport.h"
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
		if(std::optional<t_semantic_id> sema = start->GetSemanticRule(0); sema)
			coll->SetAcceptingRule(*sema);
		return coll;
	}
%}
// --------------------------------------------------------------------------------
