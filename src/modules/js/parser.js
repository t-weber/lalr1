/**
 * lr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *       - "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *       - "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

"use strict";


/**
 * get the internal table index of a token or nonterminal id
 */
function get_table_index(idx_tab, id)
{
	for(const [arridx, entry] of idx_tab.entries())
	{
		const theid = entry[0];
		const theidx = entry[1];

		if(theid == id)
			return theidx;
	}

	throw new Error("No table index for id " + id + ".");
	return null;
}


/**
 * get the token or terminal id of an internal table index
 */
function get_table_id(idx_tab, idx)
{
	for(const [arridx, entry] of idx_tab.entries())
	{
		const theid = entry[0];
		const theidx = entry[1];

		if(theidx == idx)
			return theid;
	}

	throw new Error("No id for table index " + idx + ".");
	return null;
}


class Parser
{
	constructor(tables)
	{
		// tables
		this.shift_tab = tables.shift.elems;
		this.reduce_tab = tables.reduce.elems;
		this.jump_tab = tables.jump.elems;
		this.termidx_tab = tables.term_idx;
		this.nontermidx_tab = tables.nonterm_idx;
		this.semanticidx_tab = tables.semantic_idx;
		this.numrhs_tab = tables.num_rhs_syms;
		this.lhsidx_tab = tables.lhs_idx;

		// special values
		this.acc_token = tables.consts.acc;
		this.err_token = tables.consts.err;
		this.end_token = tables.consts.end;

		this.input_tokens = [ ];
		this.semantics = null;
		this.reset();
	}


	reset()
	{
		this.input_index = -1;
		this.lookahead = null;
		this.lookahead_idx = null;
		this.accepted = false;
		this.states = [ 0 ];    // parser states
		this.symbols = [ ];     // symbol stack
	}


	/**
	 * set the input tokens
	 */
	set_input(input)
	{
		this.input_tokens = input;
	}


	/**
	 * set the semantic rule functions
	 */
	set_semantics(sem)
	{
		this.semantics = sem;
	}


	get_end_token()
	{
		return this.end_token;
	}


	/**
	 * get the next lookahead token and its table index
	 */
	get_next_lookahead()
	{
		++this.input_index;
		let tok = this.input_tokens[this.input_index];
		let tok_lval = null;
		if(tok.length > 1)
			tok_lval = tok[1];
		this.lookahead = { "is_term" : true, "id" : tok[0], "val" : tok_lval };
		this.lookahead_idx = get_table_index(this.termidx_tab, this.lookahead["id"]);
	}


	/**
	 * push the current lookahead token onto the symbol stack
	 * and get the next lookahead
	 */
	push_lookahead()
	{
		this.symbols.push(this.lookahead);
		this.get_next_lookahead();
	}


	/**
	 * reduce using a semantic rule
	 */
	apply_rule(rule_id, num_rhs, lhs_id)
	{
		const args = this.symbols.slice(this.symbols.length - num_rhs, this.symbols.length);
		this.symbols = this.symbols.slice(0, this.symbols.length - num_rhs);
		this.states = this.states.slice(0, this.states.length - num_rhs);

		// apply semantic rule if available
		let rule_ret = null;
		if(this.semantics != null && rule_id in this.semantics)
			rule_ret = this.semantics[rule_id].apply(this, args);
		else
			console.error("Semantic rule " + rule_id + " is not defined.");

		// push reduced nonterminal symbol
		this.symbols.push({ "is_term" : false, "id" : lhs_id, "val" : rule_ret });
	}


	get_top_state()
	{
		return this.states[this.states.length - 1];
	}


	parse()
	{
		this.reset();
		this.get_next_lookahead();

		while(true)
		{
			const top_state = this.get_top_state();
			const new_state = this.shift_tab[top_state][this.lookahead_idx];
			const rule_idx = this.reduce_tab[top_state][this.lookahead_idx];

			/*console.log("\nLookahead: " + this.lookahead.id + " (index: " + this.lookahead_idx + ")");
			console.log("States: " + this.states);
			console.log("Symbols: ", [...this.symbols.values()]);
			console.log("New state: " + new_state + ", rule: " + rule_idx);*/

			if(new_state == this.err_token && rule_idx == this.err_token)
				throw new Error("No shift or reduce action defined.");
			else if(new_state != this.err_token && rule_idx != this.err_token)
				throw new Error("Shift/reduce conflict.");
			else if(rule_idx == this.acc_token)
			{
				// accept
				//console.log("Accepted.");
				this.accepted = true;
				if(this.symbols.length >= 1)
					return this.symbols[this.symbols.length - 1];
				return null;
			}

			if(new_state != this.err_token)
			{
				// shift
				this.states.push(new_state);
				this.push_lookahead();
			}
			else if(rule_idx != this.err_token)
			{
				// reduce
				const rule_id = get_table_id(this.semanticidx_tab, rule_idx);
				const num_syms = this.numrhs_tab[rule_idx];
				const lhs_idx = this.lhsidx_tab[rule_idx];
				const lhs_id = get_table_id(this.nontermidx_tab, lhs_idx);

				this.apply_rule(rule_id, num_syms, lhs_id);
				this.states.push(this.jump_tab[this.get_top_state()][lhs_idx]);
			}
		}

		this.accepted = false;
		return null;
	}
};


module.exports = { "Parser" : Parser };
