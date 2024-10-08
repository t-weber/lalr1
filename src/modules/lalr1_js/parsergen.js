/**
 * lr(1) recursive ascent parser generator
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *       - https://doi.org/10.1016/0020-0190(88)90061-0
 *       - "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *       - "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

"use strict";

const g_gen_partials = true;  // generate code for partial matches


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
 * get the token or terminal string id of an internal table index
 */
function get_table_strid(idx_tab, idx)
{
	for(const [arridx, entry] of idx_tab.entries())
	{
		const thestrid = entry[2];
		const theidx = entry[1];

		if(theidx == idx)
			return thestrid;
	}

	throw new Error("No string id for table index " + idx + ".");
	return null;
}


/**
 * get the token or terminal string id from its id
 */
function get_table_strid_from_id(idx_tab, id)
{
	for(const [arridx, entry] of idx_tab.entries())
	{
		const thestrid = entry[2];
		const theid = entry[0];

		if(theid == id)
			return thestrid;
	}

	throw new Error("No string id for id " + id + ".");
	return null;
}


/**
 * check if the given table row has at least one non-error entry
 */
function has_table_entry(tab, idx1, err_token)
{
	const row = tab[idx1];
	for(const elem of row)
	{
		if(elem != err_token)
			return true;
	}
	return false;
}


function id_to_str(id, end_token)
{
	if(typeof id == "string")
		return "\"" + id + "\"";

	return id.toString();
}


/**
 * create the recursive ascent parser based on the tables
 */
function create_parser(tables, outfile)
{
	// tables
	const shift_tab = tables.shift.elems;
	const reduce_tab = tables.reduce.elems;
	const jump_tab = tables.jump.elems;
	const termidx_tab = tables.term_idx;
	const nontermidx_tab = tables.nonterm_idx;
	const semanticidx_tab = tables.semantic_idx;
	const numrhs_tab = tables.num_rhs_syms;
	const lhsidx_tab = tables.lhs_idx;

	// partial rule tables
	const part_term = tables.partials_rule_term.elems;
	const part_nonterm = tables.partials_rule_nonterm.elems;
	const part_term_len = tables.partials_matchlen_term.elems;
	const part_nonterm_len = tables.partials_matchlen_nonterm.elems;
	const part_nonterm_lhs = tables.partials_lhs_nonterm.elems;

	// special values
	const acc_token = tables.consts.acc;
	const err_token = tables.consts.err;
	const end_token = tables.consts.end;
	const start_idx = tables.consts.start;

	const num_states = shift_tab.length;
	if(num_states == 0)
		return false;
	const num_terms = shift_tab[0].length;
	const num_nonterms = jump_tab[0].length;

	// output file
	const fs = require("fs");

	fs.writeFileSync(outfile,
		"/*\n * Parser created using liblalr1 by Tobias Weber, 2020-2024.\n" +
		" * DOI: https://doi.org/10.5281/zenodo.6987396\n */\n\n",
		{"flag":"w"});

	fs.writeFileSync(outfile, "\"use strict\";\n\n", {"flag":"a"});
	fs.writeFileSync(outfile, "class Parser\n{\n", {"flag":"a"});

	// constructor
	fs.writeFileSync(outfile, "\tconstructor()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.end_token = " + end_token + ";\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.input_tokens = [ ];\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.semantics = null;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.debug = false;\n", {"flag":"a"});
	if(g_gen_partials)  // partial rules
		fs.writeFileSync(outfile, "\t\tthis.use_partials = true;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.reset();\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of constructor

	// reset function
	fs.writeFileSync(outfile, "\treset()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.input_index = -1;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.lookahead = null;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.accepted = false;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.symbols = [ ];\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.dist_to_jump = 0;\n", {"flag":"a"});
	if(g_gen_partials)  // partial rules
	{
		fs.writeFileSync(outfile, "\t\tthis.active_rules = { };\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tthis.cur_rule_handle = 0;\n", {"flag":"a"});
	}
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of reset()

	// set_input function
	fs.writeFileSync(outfile, "\tset_input(input)\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.input_tokens = input;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of set_input()

	// set_semantics function
	fs.writeFileSync(outfile, "\tset_semantics(sem)\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.semantics = sem;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of set_semantics()

	// get_end_token function
	fs.writeFileSync(outfile, "\tget_end_token()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\treturn this.end_token;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of get_end_token()

	// get_next_lookahead function
	fs.writeFileSync(outfile, "\tget_next_lookahead()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\t++this.input_index;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tlet tok = this.input_tokens[this.input_index];\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tlet tok_lval = null;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tif(tok.length > 1)\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\t\ttok_lval = tok[1];\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.lookahead = { \"is_term\" : true, \"id\" : tok[0], \"val\" : tok_lval };\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of get_next_lookahead()

	// push_lookahead function
	fs.writeFileSync(outfile, "\tpush_lookahead()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.symbols.push(this.lookahead);\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.get_next_lookahead();\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of push_lookahead()

	if(g_gen_partials)  // partial rules
	{
		// apply_partial_rule function
		fs.writeFileSync(outfile, "\tapply_partial_rule(rule_id, rule_len, before_shift)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tlet arg_len = rule_len;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(before_shift)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t++rule_len;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tlet already_seen_active_rule = false;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tlet insert_new_active_rule = false;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tlet seen_tokens_old = -1;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tlet rulestack = null;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(rule_id in this.active_rules)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\trulestack = this.active_rules[rule_id];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(rulestack != null)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(rulestack.length > 0)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tlet active_rule = rulestack[rulestack.length - 1];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tseen_tokens_old = active_rule.seen_tokens;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tif(before_shift)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\tif(active_rule.seen_tokens < rule_len)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\tactive_rule.seen_tokens = rule_len;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\telse\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\tinsert_new_active_rule = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\telse\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\tif(active_rule.seen_tokens == rule_len)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\talready_seen_active_rule = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\telse\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\tactive_rule.seen_tokens = rule_len;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\trulestack[rulestack.length - 1] = active_rule;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tthis.active_rules[rule_id] = rulestack;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\telse\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tinsert_new_active_rule = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\telse\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\trulestack = [];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tthis.active_rules[rule_id] = rulestack;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tinsert_new_active_rule = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(insert_new_active_rule)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tseen_tokens_old = -1;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tlet active_rule = {};\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tactive_rule[\"seen_tokens\"] = rule_len;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tactive_rule[\"retval\"] = null;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tactive_rule[\"handle\"] = this.cur_rule_handle;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t++this.cur_rule_handle;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\trulestack.push(active_rule);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tthis.active_rules[rule_id] = rulestack;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(!already_seen_active_rule)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(this.semantics == null || !(rule_id in this.semantics))\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\treturn;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tlet active_rule = rulestack[rulestack.length - 1];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tlet args = this.symbols.slice(this.symbols.length - arg_len, this.symbols.length);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tlet save_back = false;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(!before_shift || seen_tokens_old < rule_len - 1)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tif(this.debug)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\tconsole.log(\"Applying partial rule \" + rule_id +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" with \" + arg_len + \" symbol(s)\" +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" (handle \" + active_rule.handle + \").\" +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" Before shift: \" + before_shift + \".\");\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tactive_rule.retval = this.semantics[rule_id](args, false, active_rule.retval);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tsave_back = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(before_shift)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\targs.push(this.lookahead);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tif(this.debug)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\tconsole.log(\"Applying partial rule \" + rule_id +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" with \" + rule_len + \" symbol(s)\" +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" (handle \" + active_rule.handle + \").\" +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\t\t\" Before shift: \" + before_shift + \".\");\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tactive_rule.retval = this.semantics[rule_id](args, false, active_rule.retval);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tsave_back = true;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(save_back)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\trulestack[rulestack.length - 1] = active_rule;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tthis.active_rules[rule_id] = rulestack;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}); // end of apply_partial_rule()
	}

	// apply_rule function
	fs.writeFileSync(outfile, "\tapply_rule(rule_id, num_rhs, lhs_id)\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tlet rule_ret = null;\n", {"flag":"a"});
	if(g_gen_partials)  // partial rules
	{
		fs.writeFileSync(outfile, "\t\tlet handle = -1;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(this.use_partials && rule_id in this.active_rules)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tlet rulestack = this.active_rules[rule_id];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tif(rulestack != null && rulestack.length > 0)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t{\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tlet active_rule = rulestack[rulestack.length - 1];\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\trule_ret = active_rule.retval;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\thandle = active_rule.handle;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\trulestack = rulestack.slice(0, rulestack.length - 1);\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tthis.active_rules[rule_id] = rulestack;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\tif(this.debug)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tconsole.log(\"Applying rule \" + rule_id +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\" having \" + num_rhs + \" symbol(s)\" +\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\t\" (handle \" + handle + \").\");\n", {"flag":"a"});
	}
	else
	{
		fs.writeFileSync(outfile, "\t\tif(this.debug)\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\tconsole.log(\"Applying rule \" + rule_id + \" having \" + num_rhs + \" symbol(s).\");\n", {"flag":"a"});
	}
	fs.writeFileSync(outfile, "\t\tthis.dist_to_jump = num_rhs;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tconst args = this.symbols.slice(this.symbols.length - num_rhs, this.symbols.length);\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.symbols = this.symbols.slice(0, this.symbols.length - num_rhs);\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tif(this.semantics != null && rule_id in this.semantics)\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\t\trule_ret = this.semantics[rule_id](args, true, rule_ret);\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\telse\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\t\tthrow new Error(\"Semantic rule \" + rule_id + \" is not defined.\");\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.symbols.push({ \"is_term\" : false, \"id\" : lhs_id, \"val\" : rule_ret });\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of apply_rule()

	// parse function
	fs.writeFileSync(outfile, "\tparse()\n\t{\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.reset();\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.get_next_lookahead();\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tthis.state_" + start_idx + "();\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\tif(this.symbols.length < 1 || !this.accepted)\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\t\treturn null;\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t\treturn this.symbols[this.symbols.length - 1];\n", {"flag":"a"});
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of parse()

	// closures
	for(let state_idx=0; state_idx<num_states; ++state_idx)
	{
		const state_name = "state_" + state_idx;
		const has_shift_entry = has_table_entry(shift_tab, state_idx, err_token);
		const has_jump_entry = has_table_entry(jump_tab, state_idx, err_token);

		// closure function
		fs.writeFileSync(outfile, "\t" + state_name + "()\n\t{\n", {"flag":"a"});

		if(has_shift_entry)
			fs.writeFileSync(outfile, "\t\tlet next_state = null;\n", {"flag":"a"});

		fs.writeFileSync(outfile, "\t\tswitch(this.lookahead[\"id\"])\n\t\t{\n", {"flag":"a"});

		let rules_term_id = {};  // map of rules and their terminal ids
		let acc_term_id = [];    // terminal ids for accepting
		for(let term_idx = 0; term_idx < num_terms; ++term_idx)
		{
			const term_id = get_table_id(termidx_tab, term_idx);
			const newstate_idx = shift_tab[state_idx][term_idx];
			const rule_idx = reduce_tab[state_idx][term_idx];

			// shift
			if(newstate_idx != err_token)
			{
				const term_strid = get_table_strid(termidx_tab, term_idx);
				const term_id_str = id_to_str(term_id, end_token);
				fs.writeFileSync(outfile, "\t\t\tcase " + term_id_str + ": // " + term_strid + "\n", {"flag":"a"});
				fs.writeFileSync(outfile, "\t\t\t\tnext_state = this.state_" + newstate_idx + ";\n", {"flag":"a"});
				if(g_gen_partials)  // partial rules
				{
					const partial_idx = part_term[state_idx][term_idx];
					if(partial_idx != err_token)
					{
						const partial_id = get_table_id(semanticidx_tab, partial_idx);
						const partial_len = part_term_len[state_idx][term_idx];
						fs.writeFileSync(outfile, "\t\t\t\tif(this.use_partials)\n", {"flag":"a"});
						fs.writeFileSync(outfile, "\t\t\t\t\tthis.apply_partial_rule(" +
							partial_id + ", " + partial_len + ", true);\n", {"flag":"a"});
					}
				}
				fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"});
			}
			else if(rule_idx != err_token)
			{
				if(rule_idx == acc_token)
				{
					// accept
					acc_term_id.push(term_id);
				}
				else
				{
					// reduce
					if(!rules_term_id.hasOwnProperty(rule_idx))
						rules_term_id[rule_idx] = [];
					rules_term_id[rule_idx].push(term_id);
				}
			}
		}

		// reduce; create cases for rule application
		for(const rule_idx in rules_term_id)
		{
			const term_ids = rules_term_id[rule_idx];
			const num_rhs = numrhs_tab[rule_idx];
			const lhs_id = get_table_id(nontermidx_tab, lhsidx_tab[rule_idx]);
			const rule_id = get_table_id(semanticidx_tab, rule_idx);

			let created_cases = false;
			for(const term_id of term_ids)
			{
				const term_strid = get_table_strid_from_id(termidx_tab, term_id);
				const term_id_str = id_to_str(term_id);

				fs.writeFileSync(outfile, "\t\t\tcase " + term_id_str + ": // " + term_strid + "\n", {"flag":"a"});
				created_cases = true;
			}

			if(created_cases)
			{
				fs.writeFileSync(outfile, "\t\t\t\tthis.apply_rule(" + rule_id + ", " + num_rhs + ", " + lhs_id + ");\n", {"flag":"a"});
				fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"});
			}
		}

		// create accepting case
		if(acc_term_id.length > 0)
		{
			for(const term_id of acc_term_id)
			{
				const term_id_str = id_to_str(term_id);
				fs.writeFileSync(outfile, "\t\t\tcase " + term_id_str + ":\n", {"flag":"a"});
			}

			fs.writeFileSync(outfile, "\t\t\t\tthis.accepted = true;\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"});
		}

		// default to error
		fs.writeFileSync(outfile, "\t\t\tdefault:\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tthrow new Error(\"Invalid terminal transition from state " + state_idx + ".\");\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});  // end of switch

		// shift
		if(has_shift_entry)
		{
			fs.writeFileSync(outfile, "\t\tif(next_state != null)\n\t\t{\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\tthis.push_lookahead();\n", {"flag":"a"});  // end of closure function
			fs.writeFileSync(outfile, "\t\t\tnext_state.call(this);\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});  // endif
		}

		// jump
		if(has_jump_entry)
		{
			fs.writeFileSync(outfile, "\t\twhile(this.dist_to_jump == 0 && this.symbols.length > 0 && !this.accepted)\n\t\t{\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\tconst topsym = this.symbols[this.symbols.length - 1];\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\tif(topsym[\"is_term\"])\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"});

			fs.writeFileSync(outfile, "\t\t\tswitch(topsym[\"id\"])\n\t\t\t{\n", {"flag":"a"});
			for(let nonterm_idx = 0; nonterm_idx < num_nonterms; ++nonterm_idx)
			{
				const jump_state_idx = jump_tab[state_idx][nonterm_idx];
				if(jump_state_idx != err_token)
				{
					const nonterm_id = get_table_id(nontermidx_tab, nonterm_idx);
					const nonterm_strid = get_table_strid(nontermidx_tab, nonterm_idx);
					fs.writeFileSync(outfile, "\t\t\t\tcase " + nonterm_id + ": // " + nonterm_strid + "\n", {"flag":"a"});
					if(g_gen_partials)  // partial rules
					{
						const lhs_id = part_nonterm_lhs[state_idx][nonterm_idx];
						if(lhs_id != err_token)
						{
							const lhs_idx = get_table_index(nontermidx_tab, lhs_id);
							const partial_idx = part_nonterm[state_idx][lhs_idx];
							if(partial_idx != err_token)
							{
								const partial_id = get_table_id(semanticidx_tab, partial_idx);
								const partial_len = part_nonterm_len[state_idx][lhs_idx];
								fs.writeFileSync(outfile, "\t\t\t\t\tif(this.use_partials)\n", {"flag":"a"});
								fs.writeFileSync(outfile, "\t\t\t\t\t\tthis.apply_partial_rule(" +
									partial_id + ", " + partial_len + ", false);\n", {"flag":"a"});
							}
						}
					}
					fs.writeFileSync(outfile, "\t\t\t\t\tthis.state_" + jump_state_idx + "();\n", {"flag":"a"});
					fs.writeFileSync(outfile, "\t\t\t\t\tbreak;\n", {"flag":"a"});
				}
			}
			fs.writeFileSync(outfile, "\t\t\t\tdefault:\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\t\t\tthrow new Error(\"Invalid nonterminal transition from state " + state_idx + ".\");\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\t\t\tbreak;\n", {"flag":"a"});
			fs.writeFileSync(outfile, "\t\t\t}\n", {"flag":"a"});  // end switch
			fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"});  // end while
		}

		fs.writeFileSync(outfile, "\t\t--this.dist_to_jump;\n", {"flag":"a"});
		fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"});  // end of closure function
	}

	fs.writeFileSync(outfile, "};\n\n", {"flag":"a"});  // end of Parser class
	fs.writeFileSync(outfile, "module.exports = { \"Parser\" : Parser };\n", {"flag":"a"});

	return true;
}


// --------------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------------
/**
 * load tables
 */
if(process.argv.length < 3)
{
	console.error("Please give a json parsing table file.");
	process.exit(-1);
}

try
{
	const tables_file = process.argv[2];
	const ext_idx = tables_file.lastIndexOf(".");
	const parser_file = tables_file.substr(0, ext_idx) + "_parser.js";

	console.log("Creating parser \"" + tables_file + "\" -> \"" + parser_file + "\".");

	const tables = require(tables_file);
	console.log(tables.infos);

	create_parser(tables, parser_file);
}
catch(ex)
{
	console.error("Error code: " + ex.code + ", message: " + ex.message + ".");
}
// --------------------------------------------------------------------------------
