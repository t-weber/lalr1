/**
 * lalr(1) collection
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 */

#include "collection.h"

#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <boost/functional/hash.hpp>


// ----------------------------------------------------------------------------
/**
 * hash a transition element
 */
std::size_t Collection::HashTransition::operator()(
	const t_transition& trans) const
{
	std::size_t hashFrom = std::get<0>(trans)->hash(true);
	std::size_t hashTo = std::get<1>(trans)->hash(true);
	std::size_t hashSym = std::get<2>(trans)->hash();

	boost::hash_combine(hashFrom, hashTo);
	boost::hash_combine(hashFrom, hashSym);

	return hashFrom;
}


/**
 * compare two transition elements for equality
 */
bool Collection::CompareTransitionsEqual::operator()(
	const t_transition& tr1, const t_transition& tr2) const
{
	return HashTransition{}(tr1) == HashTransition{}(tr2);
}


/**
 * compare two transition elements based on their order
 */
bool Collection::CompareTransitionsLess::operator()(
	const t_transition& tr1, const t_transition& tr2) const
{
	const ClosurePtr& from1 = std::get<0>(tr1);
	const ClosurePtr& from2 = std::get<0>(tr2);
	const ClosurePtr& to1 = std::get<1>(tr1);
	const ClosurePtr& to2 = std::get<1>(tr2);
	const SymbolPtr& sym1 = std::get<2>(tr1);
	const SymbolPtr& sym2 = std::get<2>(tr2);

	if(from1->GetId() < from2->GetId())
		return true;
	if(from1->GetId() == from2->GetId())
		return to1->GetId() < to2->GetId();
	if(from1->GetId() == from2->GetId() && to1->GetId() == to2->GetId())
		return sym1->GetId() < sym2->GetId();
	return false;
}
// ----------------------------------------------------------------------------


Collection::Collection(const ClosurePtr& closure)
	: std::enable_shared_from_this<Collection>{}
{
	m_collection.push_back(closure);
}


Collection::Collection() : std::enable_shared_from_this<Collection>{}
{
}


void Collection::SetProgressObserver(std::function<void(const std::string&, bool)> func)
{
	m_progress_observer = func;
}


void Collection::ReportProgress(const std::string& msg, bool finished)
{
	if(m_progress_observer)
		m_progress_observer(msg, finished);
}


/**
 * get terminal or non-terminal transitions originating from the given closure
 */
Collection::t_transitions Collection::GetTransitions(
	const ClosurePtr& closure, bool term) const
{
	t_transitions transitions;

	for(const t_transition& transition : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		if(sym->IsEps())
			continue;

		// only consider transitions from the given closure
		if(closure_from->hash() != closure->hash())
			continue;

		if(sym->IsTerminal() == term)
			transitions.insert(transition);
	}

	return transitions;
}


/**
 * get terminals leading to the given closure
 */
Terminal::t_terminalset Collection::GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	m_seen_closures = std::make_shared<std::unordered_set<std::size_t>>();
	return _GetLookbackTerminals(closure);
}


Terminal::t_terminalset Collection::_GetLookbackTerminals(
	const ClosurePtr& closure) const
{
	Terminal::t_terminalset terms;

	for(const t_transition& transition : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(transition);
		const ClosurePtr& closure_to = std::get<1>(transition);
		const SymbolPtr& sym = std::get<2>(transition);

		// only consider transitions to the given closure
		if(closure_to->hash() != closure->hash())
			continue;

		if(sym->IsTerminal())
		{
			const TerminalPtr& term =
				std::dynamic_pointer_cast<Terminal>(sym);
			terms.emplace(std::move(term));
		}
		else if(closure_from)
		{
			// closure not yet seen?
			std::size_t hash = closure_from->hash();
			if(!m_seen_closures->contains(hash))
			{
				m_seen_closures->insert(hash);

				// get terminals from previous closure
				Terminal::t_terminalset _terms =
					_GetLookbackTerminals(closure_from);
				terms.merge(_terms);
			}
		}
	}

	return terms;
}


/**
 * perform all possible lalr(1) transitions from all closures
 */
void Collection::DoTransitions(const ClosurePtr& closure_from)
{
	if(!m_closure_cache)
	{
		m_closure_cache = std::make_shared<std::unordered_map<std::size_t, ClosurePtr>>();
		m_closure_cache->emplace(std::make_pair(closure_from->hash(true), closure_from));
	}

	const Closure::t_transitions& transitions = closure_from->DoTransitions();

	// no more transitions?
	if(transitions.size() == 0)
		return;

	for(const Closure::t_transition& tup : transitions)
	{
		const SymbolPtr& trans_sym = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		std::size_t hash_to = closure_to->hash(true);
		auto cacheIter = m_closure_cache->find(hash_to);
		bool new_closure = (cacheIter == m_closure_cache->end());

		std::ostringstream ostrMsg;
		ostrMsg << "Calculating " << (new_closure ? "new " : "") <<  "transition "
			<< closure_from->GetId() << " -> " << closure_to->GetId()
			<< ". Total closures: " << m_collection.size()
			<< ", total transitions: " << m_transitions.size()
			<< ".";
		ReportProgress(ostrMsg.str(), false);

		if(new_closure)
		{
			// new unique closure
			m_closure_cache->emplace(std::make_pair(hash_to, closure_to));
			m_collection.push_back(closure_to);
			m_transitions.emplace(std::make_tuple(closure_from, closure_to, trans_sym));

			DoTransitions(closure_to);
		}
		else
		{
			// reuse closure with the same core that has already been seen
			const ClosurePtr& closure_to_existing = cacheIter->second;

			// unite lookaheads
			closure_to_existing->AddLookaheadDependencies(closure_to);

			// add the transition from the closure
			m_transitions.emplace(std::make_tuple(closure_from, closure_to_existing, trans_sym));
		}
	}
}


void Collection::DoTransitions()
{
	m_closure_cache = nullptr;
	DoTransitions(*m_collection.begin());
	ReportProgress("Transitions calculated.", true);

	for(ClosurePtr& closure : m_collection)
	{
		std::ostringstream ostrMsg;
		ostrMsg << "Calculating lookaheads for closure " << closure->GetId() << ".";
		ReportProgress(ostrMsg.str(), false);
		closure->ResolveLookaheads();
	}
	ReportProgress("Lookaheads calculated.", true);

	Simplify();
	ReportProgress("Simplified transitions.", true);

	CreateTableIndices();
	ReportProgress("Created table indices.", true);
}


void Collection::Simplify()
{
	// sort rules
	m_collection.sort(
		[](const ClosurePtr& closure1, const ClosurePtr& closure2) -> bool
		{
			return closure1->GetId() < closure2->GetId();
		});

	// cleanup closure ids
	std::unordered_map<std::size_t, std::size_t> idmap;
	std::unordered_set<std::size_t> already_seen;
	std::size_t newid = 0;

	for(const ClosurePtr& closure : m_collection)
	{
		std::size_t oldid = closure->GetId();
		std::size_t hash = closure->hash();

		if(already_seen.contains(hash))
			continue;

		auto iditer = idmap.find(oldid);
		if(iditer == idmap.end())
			iditer = idmap.emplace(
				std::make_pair(oldid, newid++)).first;

		closure->SetId(iditer->second);
		already_seen.insert(hash);
	}
}


/**
 * creates indices for the parse tables from the symbol ids
 */
void Collection::CreateTableIndices()
{
	// generate table indices for terminals
	m_mapTermIdx.clear();
	std::size_t curTermIdx = 0;

	for(const t_transition& tup : m_transitions)
	{
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(symTrans->IsEps() || !symTrans->IsTerminal())
			continue;

		m_mapTermIdx.try_emplace(symTrans->GetId(), curTermIdx++);
	}

	// add end symbol
	m_mapTermIdx.try_emplace(g_end->GetId(), curTermIdx++);


	// generate able indices for non-terminals
	m_mapNonTermIdx.clear();
	std::size_t curNonTermIdx = 0;

	for(const ClosurePtr& closure : m_collection)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			std::size_t sym_id = elem->GetLhs()->GetId();
			m_mapNonTermIdx.try_emplace(sym_id, curNonTermIdx++);
		}
	}
}


/**
 * translates symbol id to table index
 */
std::size_t Collection::GetTableIndex(std::size_t id, bool is_term) const
{
	const t_mapIdIdx* map = is_term ? &m_mapTermIdx : &m_mapNonTermIdx;

	auto iter = map->find(id);
	if(iter == map->end())
	{
		std::ostringstream ostrErr;
		ostrErr << "No table index is available for ";
		if(is_term)
			ostrErr << "terminal";
		else
			ostrErr << "non-terminal";
		ostrErr << " with id " << id << ".";
		throw std::runtime_error(ostrErr.str());
	}

	return iter->second;
}


/**
 * stop table/code generation on shift/reduce conflicts
 */
void Collection::SetStopOnConflicts(bool b)
{
	m_stopOnConflicts = b;
}


/**
 * try to solve a shift/reduce conflict
 */
bool Collection::SolveConflict(
	const SymbolPtr& sym_at_cursor, const Terminal::t_terminalset& lookbacks,
	std::size_t* shiftEntry, std::size_t* reduceEntry) const
{
	// no conflict?
	if(*shiftEntry==ERROR_VAL || *reduceEntry==ERROR_VAL)
		return true;

	bool solution_found = false;

	// try to resolve conflict using operator precedences/associativities
	if(sym_at_cursor && sym_at_cursor->IsTerminal())
	{
		const TerminalPtr& term_at_cursor =
			std::dynamic_pointer_cast<Terminal>(sym_at_cursor);

		for(const TerminalPtr& lookback : lookbacks)
		{
			auto prec_lhs = lookback->GetPrecedence();
			auto prec_rhs = term_at_cursor->GetPrecedence();

			// both terminals have a precedence
			if(prec_lhs && prec_rhs)
			{
				if(*prec_lhs < *prec_rhs)       // shift
				{
					*reduceEntry = ERROR_VAL;
					solution_found = true;
				}
				else if(*prec_lhs > *prec_rhs)  // reduce
				{
					*shiftEntry = ERROR_VAL;
					solution_found = true;
				}

				// same precedence -> use associativity
			}

			if(!solution_found)
			{
				auto assoc_lhs = lookback->GetAssociativity();
				auto assoc_rhs = term_at_cursor->GetAssociativity();

				// both terminals have an associativity
				if(assoc_lhs && assoc_rhs &&
					*assoc_lhs == *assoc_rhs)
				{
					if(*assoc_lhs == 'r')      // shift
					{
						*reduceEntry = ERROR_VAL;
						solution_found = true;
					}
					else if(*assoc_lhs == 'l') // reduce
					{
						*shiftEntry = ERROR_VAL;
						solution_found = true;
					}
				}
			}

			if(solution_found)
				break;
		}
	}

	return solution_found;
}


/**
 * write out the transitions graph
 */
bool Collection::SaveGraph(const std::string& file, bool write_full_coll) const
{
	std::string outfile_graph = file + ".graph";
	std::string outfile_svg = file + ".svg";

	std::ofstream ofstr{outfile_graph};
	if(!ofstr)
		return false;

	ofstr << "digraph G_lr1\n{\n";

	// write states
	for(const ClosurePtr& closure : m_collection)
	{
		ofstr << "\t" << closure->GetId() << " [label=\"";
		if(write_full_coll)
			ofstr << *closure;
		else
			ofstr << closure->GetId();
		ofstr << "\"];\n";
	}

	// write transitions
	ofstr << "\n";
	for(const t_transition& tup : m_transitions)
	{
		const ClosurePtr& closure_from = std::get<0>(tup);
		const ClosurePtr& closure_to = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		if(symTrans->IsEps())
			continue;

		ofstr << "\t" << closure_from->GetId() << " -> " << closure_to->GetId()
			<< " [label=\"" << symTrans->GetStrId() << "\"];\n";
	}

	ofstr << "}" << std::endl;
	ofstr.close();

	return std::system(("dot -Tsvg " + outfile_graph + " -o " + outfile_svg).c_str()) == 0;
}


std::ostream& operator<<(std::ostream& ostr, const Collection& coll)
{
	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Collection\n";
	ostr << "--------------------------------------------------------------------------------\n";
	for(const ClosurePtr& closure : coll.m_collection)
	{
		ostr << *closure;

		Terminal::t_terminalset lookbacks = coll.GetLookbackTerminals(closure);
		if(lookbacks.size())
			ostr << "Lookback terminals: ";
		for(const TerminalPtr& lookback : lookbacks)
			ostr << lookback->GetStrId() << " ";
		if(lookbacks.size())
			ostr << "\n";
		ostr << "\n";
	}
	ostr << "\n";


	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Transitions\n";
	ostr << "--------------------------------------------------------------------------------\n";
	for(const Collection::t_transition& tup : coll.m_transitions)
	{
		ostr << "closure " << std::get<0>(tup)->GetId()
			<< " -> closure " << std::get<1>(tup)->GetId()
			<< " via " << std::get<2>(tup)->GetStrId()
			<< "\n";
	}
	ostr << "\n\n";


	ostr << "--------------------------------------------------------------------------------\n";
	ostr << "Tables\n";
	ostr << "--------------------------------------------------------------------------------\n";
	// sort transitions
	std::vector<Collection::t_transition> transitions;
	transitions.reserve(coll.m_transitions.size());
	std::copy(coll.m_transitions.begin(), coll.m_transitions.end(), std::back_inserter(transitions));
	std::stable_sort(transitions.begin(), transitions.end(), Collection::CompareTransitionsLess{});

	std::ostringstream ostrActionShift, ostrActionReduce, ostrJump;
	for(const Collection::t_transition& tup : transitions)
	{
		const ClosurePtr& stateFrom = std::get<0>(tup);
		const ClosurePtr& stateTo = std::get<1>(tup);
		const SymbolPtr& symTrans = std::get<2>(tup);

		bool symIsTerm = symTrans->IsTerminal();
		bool symIsEps = symTrans->IsEps();

		if(symIsEps)
			continue;

		if(symIsTerm)
		{
			ostrActionShift
				<< "action_shift[ closure "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = closure "
				<< stateTo->GetId() << "\n";
		}
		else
		{
			ostrJump
				<< "jump[ closure "
				<< stateFrom->GetId() << ", "
				<< symTrans->GetStrId() << " ] = closure "
				<< stateTo->GetId() << "\n";
		}
	}

	for(const ClosurePtr& closure : coll.m_collection)
	{
		for(const ElementPtr& elem : closure->GetElements())
		{
			if(!elem->IsCursorAtEnd())
				continue;

			ostrActionReduce << "action_reduce[ closure " << closure->GetId() << ", ";
			for(const auto& la : elem->GetLookaheads())
				ostrActionReduce << la->GetStrId() << " ";
			ostrActionReduce << "] = ";
			if(elem->GetSemanticRule())
			{
				ostrActionReduce << "[rule "
					<< *elem->GetSemanticRule() << "] ";
			}
			ostrActionReduce << elem->GetLhs()->GetStrId()
				<< " -> " << *elem->GetRhs();
			ostrActionReduce << "\n";
		}
	}

	ostr << ostrActionShift.str() << "\n"
		<< ostrActionReduce.str() << "\n"
		<< ostrJump.str() << "\n";
	return ostr;
}
