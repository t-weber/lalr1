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

		// partial rule tables
		this.part_term = tables.partials_rule_term.elems;
		this.part_nonterm = tables.partials_rule_nonterm.elems;
		this.part_term_len = tables.partials_matchlen_term.elems;
		this.part_nonterm_len = tables.partials_matchlen_nonterm.elems;

		// special values
		this.acc_token = tables.consts.acc;
		this.err_token = tables.consts.err;
		this.end_token = tables.consts.end;
		this.start_idx = tables.consts.start;

		this.input_tokens = [ ];
		this.semantics = null;

		// options
		this.use_partials = true;
		this.debug = false;

		this.reset();
	}


	reset()
	{
		this.input_index = -1;            // index into the input token array
		this.lookahead = null;            // current lookahead terminal...
		this.lookahead_idx = null;        // ... and its index
		this.accepted = false;            // input has been accepted?
		this.states = [ this.start_idx ]; // parser states
		this.symbols = [ ];               // symbol stack
		this.active_rules = { };          // active partial rules
		this.cur_rule_handle = 0;         // global rule counter
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
		// remove fully reduced rule from active rule stack and get return value
		let rule_ret = null;
		let handle = -1;
		if(this.use_partials && rule_id in this.active_rules)
		{
			let rulestack = this.active_rules[rule_id];
			if(rulestack != null && rulestack.length > 0)
			{
				let active_rule = rulestack[rulestack.length - 1];
				rule_ret = active_rule.retval;
				handle = active_rule.handle;

				// pop active rule
				rulestack = rulestack.slice(0, rulestack.length - 1);
				this.active_rules[rule_id] = rulestack;
			}
		}

		if(this.debug)
		{
			console.log("Applying rule " + rule_id +
				" having " + num_rhs + " symbol(s)" +
				" (handle " + handle + ").");
		}

		const args = this.symbols.slice(this.symbols.length - num_rhs, this.symbols.length);
		this.symbols = this.symbols.slice(0, this.symbols.length - num_rhs);
		this.states = this.states.slice(0, this.states.length - num_rhs);

		// apply semantic rule if available
		if(this.semantics != null && rule_id in this.semantics)
			rule_ret = this.semantics[rule_id](args, true, rule_ret);
		else
			console.error("Semantic rule " + rule_id + " is not defined.");

		// push reduced nonterminal symbol
		this.symbols.push({ "is_term" : false, "id" : lhs_id, "val" : rule_ret });
	}


	/**
	 * partially apply a semantic rule
	 */
	apply_partial_rule(rule_id, rule_len, before_shift)
	{
		let arg_len = rule_len;

		if(before_shift)
		{
			// directly count the following lookahead terminal
			++rule_len;
		}

		let already_seen_active_rule = false;
		let insert_new_active_rule = false;
		let seen_tokens_old = -1;

		let rulestack = null;
		if(rule_id in this.active_rules)
			rulestack = this.active_rules[rule_id];
		if(rulestack != null)
		{
			if(rulestack.length > 0)
			{
				let active_rule = rulestack[rulestack.length - 1];
				seen_tokens_old = active_rule.seen_tokens;

				if(before_shift)
				{
					if(active_rule.seen_tokens < rule_len)
						active_rule.seen_tokens = rule_len;
					else
						insert_new_active_rule = true;
				}
				else  // before jump
				{
					if(active_rule.seen_tokens == rule_len)
						already_seen_active_rule = true;
					else
						active_rule.seen_tokens = rule_len;
				}

				// save changed values
				rulestack[rulestack.length - 1] = active_rule;
				this.active_rules[rule_id] = rulestack;
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
			rulestack = [];
			this.active_rules[rule_id] = rulestack;
			insert_new_active_rule = true;
		}

		if(insert_new_active_rule)
		{
			seen_tokens_old = -1;

			let active_rule = {};
			active_rule["seen_tokens"] = rule_len;
			active_rule["retval"] = null;
			active_rule["handle"] = this.cur_rule_handle;
                        ++this.cur_rule_handle;

			// save changed values
			rulestack.push(active_rule);
			this.active_rules[rule_id] = rulestack;
		}

		if(!already_seen_active_rule)
		{
			// partially apply semantic rule if available
			if(this.semantics == null || !(rule_id in this.semantics))
				return;

			let active_rule = rulestack[rulestack.length - 1];

			// get arguments for semantic rule
			let args = this.symbols.slice(
				this.symbols.length - arg_len,
				this.symbols.length);
			let save_back = false;

			if(!before_shift || seen_tokens_old < rule_len - 1)
			{
				if(this.debug)
				{
					console.log("Applying partial rule " + rule_id +
						" with " + arg_len + " symbol(s)" +
						" (handle " + active_rule.handle + ")." +
						" Before shift: " + before_shift + ".");
				}

				// run the semantic rule
				active_rule.retval = this.semantics[rule_id](
					args, false, active_rule.retval);
				save_back = true;
			}

			if(before_shift)
			{
				args.push(this.lookahead);

				if(this.debug)
				{
					console.log("Applying partial rule " + rule_id +
						" with " + rule_len + " symbol(s)" +
						" (handle " + active_rule.handle + ")." +
						" Before shift: " + before_shift + ".");
				}

				// run the semantic rule again
				active_rule.retval = this.semantics[rule_id](
					args, false, active_rule.retval);
				save_back = true;
			}

			// save changed values
			if(save_back)
			{
				rulestack[rulestack.length - 1] = active_rule;
				this.active_rules[rule_id] = rulestack;
			}
		}
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

			if(this.debug)
			{
				console.log("\nLookahead: " + this.lookahead.id + " (index: " + this.lookahead_idx + ")");
				console.log("States: " + this.states);
				console.log("Symbols: ", [...this.symbols.values()]);
				console.log("New state: " + new_state + ", rule: " + rule_idx);
			}

			if(new_state == this.err_token && rule_idx == this.err_token)
				throw new Error("No shift or reduce action defined.");
			else if(new_state != this.err_token && rule_idx != this.err_token)
				throw new Error("Shift/reduce conflict.");

			// accept
			else if(rule_idx == this.acc_token)
			{
				if(this.debug)
					console.log("Accepted.");
				this.accepted = true;
				if(this.symbols.length >= 1)
					return this.symbols[this.symbols.length - 1];
				return null;
			}

			// shift
			if(new_state != this.err_token)
			{
				// partial rules
				if(this.use_partials)
				{
					const partial_idx = this.part_term[top_state][this.lookahead_idx];
					if(partial_idx != this.err_token)
					{
						const partial_id = get_table_id(this.semanticidx_tab, partial_idx);
						const partial_len = this.part_term_len[top_state][this.lookahead_idx];
						this.apply_partial_rule(partial_id, partial_len, true);
					}
				}

				this.states.push(new_state);
				this.push_lookahead();
			}

			// reduce
			else if(rule_idx != this.err_token)
			{
				const rule_id = get_table_id(this.semanticidx_tab, rule_idx);
				const num_syms = this.numrhs_tab[rule_idx];
				const lhs_idx = this.lhsidx_tab[rule_idx];
				const lhs_id = get_table_id(this.nontermidx_tab, lhs_idx);

				this.apply_rule(rule_id, num_syms, lhs_id);
				const new_top_state = this.get_top_state();

				// partial rules
				if(this.use_partials && this.symbols.length > 0)
				{
					const partial_idx = this.part_nonterm[top_state][lhs_idx];
					if(partial_idx != this.err_token)
					{
						const partial_id = get_table_id(this.semanticidx_tab, partial_idx);
						const partial_len = this.part_nonterm_len[top_state][lhs_idx];
						this.apply_partial_rule(partial_id, partial_len, false);
					}
				}

				this.states.push(this.jump_tab[new_top_state][lhs_idx]);
			}
		}

		this.accepted = false;
		return null;
	}
};


module.exports = { "Parser" : Parser };
