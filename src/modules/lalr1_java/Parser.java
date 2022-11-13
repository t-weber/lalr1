/**
 * LALR(1) parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 23-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 * 	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 * 	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

package lalr1_java;

import java.util.Vector;
import java.util.Stack;
import java.util.HashMap;


public class Parser<t_lval> implements ParserInterface<t_lval>
{
	protected ParsingTableInterface m_tables;  // parsing tables

	// table index and id maps
	protected HashMap<Integer, Integer> m_map_term_index;
	protected HashMap<Integer, Integer> m_map_nonterm_id;
	protected HashMap<Integer, Integer> m_map_semantic_id;

	protected Stack<Symbol<t_lval>> m_symbols; // symbol stack
	protected Stack<Integer> m_states;         // state stack
	protected HashMap<Integer, SemanticRuleInterface<t_lval>> m_semantics;

	protected int m_lookahead_index;           // lookahead index
	protected Symbol<t_lval> m_lookahead;      // lookahead token

	protected int m_input_index;               // current input token
	protected Vector<Symbol<t_lval>> m_input;  // input tokens

	protected boolean m_accepted;              // input accepted?


	public Parser(ParsingTableInterface tables)
	{
		m_tables = tables;

		// create terminal map from id to index
		m_map_term_index = new HashMap<Integer, Integer>();
		int[][] term_idx = m_tables.GetTermIndexMap();
		for(int i=0; i<term_idx.length; ++i)
			m_map_term_index.put(term_idx[i][0], term_idx[i][1]);

		// create nonterminal map from index to id
		m_map_nonterm_id = new HashMap<Integer, Integer>();
		int[][] nonterm_idx = m_tables.GetNontermIndexMap();
		for(int i=0; i<nonterm_idx.length; ++i)
			m_map_nonterm_id.put(nonterm_idx[i][1], nonterm_idx[i][0]);

		// create semantic rule map from index to id
		m_map_semantic_id = new HashMap<Integer, Integer>();
		int[][] semantic_idx = m_tables.GetSemanticIndexMap();
		for(int i=0; i<semantic_idx.length; ++i)
			m_map_semantic_id.put(semantic_idx[i][1], semantic_idx[i][0]);

		m_input = new Vector<Symbol<t_lval>>();

		m_symbols = new Stack<Symbol<t_lval>>();
		m_states = new Stack<Integer>();

		Reset();
	}


	@Override
	public void Reset()
	{
		m_accepted = false;
		m_input_index = -1;

		m_lookahead = null;
		m_lookahead_index = -1;

		m_symbols.clear();

		m_states.clear();
		m_states.add(m_tables.GetStartConst());   // start state
	}


	/**
	 * set the input tokens
	 */
	@Override
	public void SetInput(Vector<Symbol<t_lval>> input)
	{
		m_input = input;
	}


	/**
	 * set the semantic rule functions
	 */
	@Override
	public void SetSemantics(HashMap<Integer,
		SemanticRuleInterface<t_lval>> semantics)
	{
		m_semantics = semantics;
	}


	/**
	 * has the input been accepted?
	 */
	@Override
	public boolean GetAccepted()
	{
		return m_accepted;
	}


	/**
	 * get the token id representing the end of the input
	 */
	@Override
	public int GetEndConst()
	{
		return m_tables.GetEndConst();
	}


	/**
	 * get the terminal table index from its id
	 */
	protected int GetTermTableIndex(int id)
	{
		HashMap<Integer, Integer> map = m_map_term_index;
		Integer idx = map.get(id);

		if(idx == null)
			throw new RuntimeException("Invalid terminal id: " + id + ".");
		return idx;
	}


	/**
	 * get the table id from its index
	 */
	protected Integer GetTableId(HashMap<Integer, Integer> map, int idx)
	{
		return map.get(idx);
	}


	/**
	 * get the semantic table id from its index
	 */
	protected int GetSemanticTableId(int idx)
	{
		Integer id = GetTableId(m_map_semantic_id, idx);

		if(id == null)
			throw new RuntimeException("Invalid semantic index: " + idx + ".");
		return id;
	}


	/**
	 * get the nonterminal table id from its index
	 */
	protected int GetNontermTableId(int idx)
	{
		Integer id = GetTableId(m_map_nonterm_id, idx);

		if(id == null)
			throw new RuntimeException("Invalid nonterminal index: " + idx + ".");
		return id;
	}


	/**
	 * get the next lookahead token and its table index
	 */
	protected void NextLookahead()
	{
		++m_input_index;
		m_lookahead = m_input.elementAt(m_input_index);
		m_lookahead_index = GetTermTableIndex(m_lookahead.GetId());
	}


	/**
	 * push the current lookahead token onto the symbol stack
	 * and get the next lookahead
	 */
	protected void PushLookahead()
	{
		m_symbols.push(m_lookahead);
		NextLookahead();
	}


	/**
	 * reduce using a semantic rule with given id
	 */
	protected void ApplyRule(int rule_id, int num_rhs, int lhs_id)
	{
		//System.out.println("Applying rule " + rule_id + " with " + num_rhs + " arguments.");

		// get arguments for semantic rule
		Vector<Symbol<t_lval>> args = new Vector<Symbol<t_lval>>();
		for(int i=0; i<num_rhs; ++i)
			args.add(m_symbols.elementAt(m_symbols.size() - num_rhs + i));

		for(int i=0; i<num_rhs; ++i)
		{
			m_symbols.pop();
			m_states.pop();
		}

		// apply semantic rule if available
		t_lval retval = null;
		if(m_semantics != null && m_semantics.containsKey(rule_id))
			retval = m_semantics.get(rule_id).call(args);
		else
			System.err.println("Semantic rule " + rule_id + " is not defined.");

		// push result
		m_symbols.push(new Symbol<t_lval>(false, lhs_id, retval));
	}


	/**
	 * parse the input
	 */
	@Override
	public Symbol<t_lval> Parse()
	{
		int[][] shift = m_tables.GetShiftTab();
		int[][] reduce = m_tables.GetReduceTab();
		int[][] jump = m_tables.GetJumpTab();

		int err = m_tables.GetErrConst();
		int acc = m_tables.GetAccConst();

		int[] num_rhs = m_tables.GetNumRhsSymbols();
		int[] lhs_indices = m_tables.GetLhsIndices();

		Reset();
		NextLookahead();

		while(true)
		{
			int top_state = m_states.peek();
			int new_state = shift[top_state][m_lookahead_index];
			int rule_index = reduce[top_state][m_lookahead_index];

			if(new_state == err && rule_index == err)
				throw new RuntimeException("No shift or reduce action defined.");
			else if(new_state != err && rule_index != err)
				throw new RuntimeException("Shift/reduce conflict.");
			else if(rule_index == acc)
			{
				m_accepted = true;
				//System.out.println("accepted");

				if(m_symbols.size() >= 1)
					return m_symbols.peek();
				return null;
			}

			if(new_state != err)
			{
				// shift
				m_states.push(new_state);
				PushLookahead();
			}
			else if(rule_index != err)
			{
				// reduce
				int num_syms = num_rhs[rule_index];
				int lhs_index = lhs_indices[rule_index];
				int rule_id = GetSemanticTableId(rule_index);
				int lhs_id = GetNontermTableId(lhs_index);

				ApplyRule(rule_id, num_syms, lhs_id);
				int jump_state = jump[m_states.peek()][lhs_index];
				m_states.push(jump_state);
			}
		}

		//m_accepted = false;
		//return null;
	}
}
