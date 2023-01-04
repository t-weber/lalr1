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

	// partial rules
	protected HashMap<Integer /*semantic id*/,
		Stack<ActiveRule<t_lval>>> m_active_rules;
	protected int m_cur_rule_handle;

	protected int m_lookahead_index;           // lookahead index
	protected Symbol<t_lval> m_lookahead;      // lookahead token

	protected int m_input_index;               // current input token
	protected Vector<Symbol<t_lval>> m_input;  // input tokens

	protected boolean m_debug;                 // enable debug output
	protected boolean m_use_partials;          // enable partial rules
	protected boolean m_accepted;              // input accepted?


	public Parser(ParsingTableInterface tables)
	{
		m_tables = tables;

		// create terminal map from id to index
		m_map_term_index = new HashMap<Integer, Integer>();
		final int[][] term_idx = m_tables.GetTermIndexMap();
		for(int i=0; i<term_idx.length; ++i)
			m_map_term_index.put(term_idx[i][0], term_idx[i][1]);

		// create nonterminal map from index to id
		m_map_nonterm_id = new HashMap<Integer, Integer>();
		final int[][] nonterm_idx = m_tables.GetNontermIndexMap();
		for(int i=0; i<nonterm_idx.length; ++i)
			m_map_nonterm_id.put(nonterm_idx[i][1], nonterm_idx[i][0]);

		// create semantic rule map from index to id
		m_map_semantic_id = new HashMap<Integer, Integer>();
		final int[][] semantic_idx = m_tables.GetSemanticIndexMap();
		for(int i=0; i<semantic_idx.length; ++i)
			m_map_semantic_id.put(semantic_idx[i][1], semantic_idx[i][0]);

		m_debug = false;
		m_use_partials = true;
		m_input = new Vector<Symbol<t_lval>>();

		m_symbols = new Stack<Symbol<t_lval>>();
		m_states = new Stack<Integer>();

		m_active_rules = new HashMap<Integer, Stack<ActiveRule<t_lval>>>();

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

		m_active_rules.clear();
		m_cur_rule_handle = 0;
	}


	@Override
	public void SetDebug(boolean debug)
	{
		m_debug = debug;
	}


	/**
	 * enable partial application of semantic rules
	 */
	@Override
	public void SetPartials(boolean use_partials)
	{
		m_use_partials = use_partials;
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
		// remove fully reduced rule from active rule stack and get return value
		t_lval retval = null;
		int handle = -1;
		if(m_use_partials)
		{
			Stack<ActiveRule<t_lval>> rulestack = m_active_rules.get(rule_id);
			if(rulestack != null && !rulestack.empty())
			{
				ActiveRule<t_lval> active_rule = rulestack.peek();
				retval = active_rule.retval;
				handle = active_rule.handle;
				rulestack.pop();
			}
		}

		if(m_debug)
		{
			System.out.print("Applying rule " + rule_id +
				" with " + num_rhs + " arguments");
			if(handle >= 0)
				System.out.print(" (handle " + handle + ")");
			System.out.println(".");
		}

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
		if(m_semantics != null && m_semantics.containsKey(rule_id))
			retval = m_semantics.get(rule_id).call(args, true, null);
		else
			System.err.println("Semantic rule " + rule_id + " is not defined.");

		// push result
		m_symbols.push(new Symbol<t_lval>(false, lhs_id, retval));
	}


	/**
	 * apply a partial semantic rule with given id
	 */
	protected void ApplyPartialRule(int rule_id, int rule_len, boolean before_shift)
	{
		int arg_len = rule_len;
		if(before_shift)
		{
			// directly count the following lookahead terminal
			++rule_len;
		}

		boolean already_seen_active_rule = false;
		boolean insert_new_active_rule = false;
		int seen_tokens_old = -1;

		Stack<ActiveRule<t_lval>> rulestack = m_active_rules.get(rule_id);
		if(rulestack != null)
		{
			if(!rulestack.empty())
			{
				ActiveRule<t_lval> active_rule = rulestack.peek();
				seen_tokens_old = active_rule.seen_tokens;

				if(before_shift)
				{
					if(active_rule.seen_tokens < rule_len)
						active_rule.seen_tokens = rule_len;
					else
						insert_new_active_rule = true;
				}
				else // before jump
				{
					if(active_rule.seen_tokens == rule_len)
						already_seen_active_rule = true;
					else
						active_rule.seen_tokens = rule_len;
				}
			}
			else
			{
				// no active rule yet
				insert_new_active_rule = true;
			}
		}
		else
		{
			// no active rule yet
			rulestack = new Stack<ActiveRule<t_lval>>();
			m_active_rules.put(rule_id, rulestack);
			insert_new_active_rule = true;
		}

		if(insert_new_active_rule)
		{
			seen_tokens_old = -1;

			ActiveRule<t_lval> active_rule = new ActiveRule<t_lval>();
			active_rule.seen_tokens = rule_len;
			active_rule.handle = m_cur_rule_handle++;

			rulestack.push(active_rule);
		}

		if(!already_seen_active_rule)
		{
			// partially apply semantic rule if available
			if(m_semantics == null || !m_semantics.containsKey(rule_id))
			{
				System.err.println("Semantic rule " + rule_id + " is not defined.");
				return;
			}

			ActiveRule<t_lval> active_rule = rulestack.peek();

			// get arguments for semantic rule
			Vector<Symbol<t_lval>> args = new Vector<Symbol<t_lval>>();
			for(int i=0; i<arg_len; ++i)
				args.add(m_symbols.elementAt(m_symbols.size() - arg_len + i));

			if(!before_shift || seen_tokens_old < rule_len - 1)
			{
				// run the semantic rule
				if(m_debug)
				{
					System.out.println("Applying partial rule " + rule_id + 
						" with " + arg_len + " arguments" +
						" (handle " + active_rule.handle + ")" +
						" Before shift: " + before_shift + ".");
				}
				active_rule.retval = m_semantics.get(rule_id).call(
					args, false, active_rule.retval);
			}

			if(before_shift)
			{
				// since we already know the next terminal in a shift, include it directly
				args.add(m_lookahead);

				// run the semantic rule again
				if(m_debug)
				{
					System.out.println("Applying partial rule " + rule_id + 
						" with " + rule_len + " arguments" +
						" (handle " + active_rule.handle + ")" +
						" Before shift: " + before_shift + ".");
				}
				active_rule.retval = m_semantics.get(rule_id).call(
					args, false, active_rule.retval);
			}
		}
	}


	/**
	 * parse the input
	 */
	@Override
	public Symbol<t_lval> Parse()
	{
		// main parsing tables
		final int[][] shift = m_tables.GetShiftTab();
		final int[][] reduce = m_tables.GetReduceTab();
		final int[][] jump = m_tables.GetJumpTab();

		// partial rule tables
		final int[][] part_term = m_tables.GetPartialsRuleTerm();
		final int[][] part_nonterm = m_tables.GetPartialsRuleNonterm();
		final int[][] part_term_len = m_tables.GetPartialsMatchLengthTerm();
		final int[][] part_nonterm_len = m_tables.GetPartialsMatchLengthNonterm();

		// constants
		final int err = m_tables.GetErrConst();
		final int acc = m_tables.GetAccConst();

		final int[] num_rhs = m_tables.GetNumRhsSymbols();
		final int[] lhs_indices = m_tables.GetLhsIndices();

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
				if(m_debug)
					System.out.println("Accepted.");

				if(m_symbols.size() >= 1)
					return m_symbols.peek();
				return null;
			}

			// shift
			if(new_state != err)
			{
				// partial rules
				if(m_use_partials)
				{
					int partial_idx = part_term[top_state][m_lookahead_index];
					if(partial_idx != err)
					{
						int partial_id = GetSemanticTableId(partial_idx);
						int partial_len = part_term_len[top_state][m_lookahead_index];

						ApplyPartialRule(partial_id, partial_len, true);
					}
				}

				m_states.push(new_state);
				PushLookahead();
			}
	
			// reduce
			else if(rule_index != err)
			{
				int num_syms = num_rhs[rule_index];
				int lhs_index = lhs_indices[rule_index];
				int rule_id = GetSemanticTableId(rule_index);
				int lhs_id = GetNontermTableId(lhs_index);

				ApplyRule(rule_id, num_syms, lhs_id);
				top_state = m_states.peek();

				// partial rules
				if(m_use_partials && m_symbols.size() > 0)
				{
					int partial_idx = part_nonterm[top_state][lhs_index];
					if(partial_idx != err)
					{
						int partial_id = GetSemanticTableId(partial_idx);
						int partial_len = part_nonterm_len[top_state][lhs_index];

						ApplyPartialRule(partial_id, partial_len, false);
					}
				}

				int jump_state = jump[top_state][lhs_index];
				m_states.push(jump_state);
			}
		}

		//m_accepted = false;
		//return null;
	}
}
