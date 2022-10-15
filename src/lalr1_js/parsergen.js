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

	// special values
	const acc_token = tables.consts.acc;
	const err_token = tables.consts.err;
	const end_token = tables.consts.end;

	const num_states = shift_tab.length;
	if(num_states == 0)
		return false;
	const num_terms = shift_tab[0].length;
	const num_nonterms = jump_tab[0].length;

	// output file
	const fs = require("fs");
	const cb = (err, data) =>
	{
		if(err)
			throw new Error("err: " + err + ", data: " + data);
	};

	fs.writeFileSync(outfile, 
		"/*\n * Parser created using liblalr1 by Tobias Weber, 2020-2022.\n" +
		" * DOI: https://doi.org/10.5281/zenodo.6987396\n */\n\n",
		{"flag":"w"}, cb);

	fs.writeFileSync(outfile, "\"use strict\";\n\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "class Parser\n{\n", {"flag":"a"}, cb);

	// constructor
	fs.writeFileSync(outfile, "\tconstructor()\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.input_tokens = [ ];\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.semantics = null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.reset();\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of constructor

	// reset function
	fs.writeFileSync(outfile, "\treset()\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.input_index = -1;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.lookahead = null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.lookahead_idx = null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.accepted = false;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.symbols = [ ];\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.dist_to_jump = 0;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of reset()

	// set_input function
	fs.writeFileSync(outfile, "\tset_input(input)\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.input_tokens = input;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of set_input()

	// set_semantics function
	fs.writeFileSync(outfile, "\tset_semantics(sem)\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.semantics = sem;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of set_semantics()

	// get_next_lookahead function
	fs.writeFileSync(outfile, "\tget_next_lookahead()\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\t++this.input_index;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tlet tok = this.input_tokens[this.input_index];\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tlet tok_lval = null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tif(tok.length > 1)\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\t\ttok_lval = tok[1];\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.lookahead = { \"is_term\" : true, \"id\" : tok[0], \"val\" : tok_lval };\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.lookahead_idx = get_table_index(this.termidx_tab, this.lookahead[\"id\"]);\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of get_next_lookahead()

	// push_lookahead function
	fs.writeFileSync(outfile, "\tpush_lookahead()\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.symbols.push(this.lookahead);\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.get_next_lookahead();\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of push_lookahead()

	// apply_rule function
	fs.writeFileSync(outfile, "\tapply_rule(rule_id, num_rhs, lhs_id)\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.dist_to_jump = num_rhs;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tconst args = this.symbols.slice(this.symbols.length-num_rhs, this.symbols.length);\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.symbols = this.symbols.slice(0, this.symbols.length - num_rhs);\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tlet rule_ret = null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tif(this.semantics != null && self.semantics.has(rule_id))\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\t\trule_ret = this.semantics[rule_id](args);\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.symbols.push({ \"is_term\" : false, \"id\" : lhs_id, \"val\" : rule_ret });\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of apply_rule()

	// parse function
	fs.writeFileSync(outfile, "\tparse()\n\t{\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.reset();\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.get_next_lookahead();\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tthis.state_0();\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\tif(this.symbols.length < 1 || !this.accepted)\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\t\treturn null;\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t\treturn this.symbols[this.symbols.length - 1];\n", {"flag":"a"}, cb);
	fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of parse()

	// closures
	for(let state_idx=0; state_idx<num_states; ++state_idx)
	{
		const has_shift_entry = has_table_entry(shift_tab, state_idx, err_token);
		const has_jump_entry = has_table_entry(jump_tab, state_idx, err_token);

		// closure function
		fs.writeFileSync(outfile, "\tstate_" + state_idx + "()\n\t{\n", {"flag":"a"}, cb);

		if(has_shift_entry)
			fs.writeFileSync(outfile, "\t\tlet next_state = null;\n", {"flag":"a"}, cb);

		fs.writeFileSync(outfile, "\t\tswitch(this.lookahead[\"id\"])\n\t\t{\n", {"flag":"a"}, cb);

		let rules_term_id = {};
		let acc_term_id = [];
		for(let term_idx = 0; term_idx < num_terms; ++term_idx)
		{
			const term_id = get_table_id(termidx_tab, term_idx);

			const newstate_idx = shift_tab[state_idx][term_idx];
			const rule_idx = reduce_tab[state_idx][term_idx];

			if(newstate_idx != err_token)
			{
				const term_id_str = id_to_str(term_id, end_token);
				fs.writeFileSync(outfile, "\t\t\tcase " + term_id_str + ":\n", {"flag":"a"}, cb);
				fs.writeFileSync(outfile, "\t\t\t\tnext_state = this.state_" + newstate_idx + ";\n", {"flag":"a"}, cb);
				fs.writeFileSync(outfile, "\t\t\t\tbreak;\n", {"flag":"a"}, cb);
			}
			else if(rule_idx != err_token)
			{
				if(rule_idx == acc_token)
				{
					acc_term_id.push(term_id);
				}
				else
				{
					if(!rules_term_id.hasOwnProperty(rule_idx))
						rules_term_id[rule_idx] = [];
					rules_term_id[rule_idx].push(term_id);
				}
			}
		}

		// TODO

		fs.writeFileSync(outfile, "\t\t}\n", {"flag":"a"}, cb);  // end of switch

		fs.writeFileSync(outfile, "\t}\n\n", {"flag":"a"}, cb);  // end of closure function
	}

	fs.writeFileSync(outfile, "};\n\n", {"flag":"a"}, cb);  // end of Parser class
	fs.writeFileSync(outfile, "module.exports = { \"Parser\" : Parser };\n", {"flag":"a"}, cb);

	return true;
}


/**
 * load tables
 */
if(process.argv.length < 3)
{
	console.error("Please give a LR(1) json table file.");
	process.exit(-1);
}

const tables_file = process.argv[2];
const ext_idx = tables_file.lastIndexOf(".");
const parser_file = tables_file.substr(0, ext_idx) + "_parser.js";

console.log("Creating parser \"" + tables_file + "\" -> \"" + parser_file + "\".");
const tables = require(tables_file);
console.log(tables.infos);

create_parser(tables, parser_file);
