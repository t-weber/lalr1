/*
 * recursive-ascent parser generator
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 13-nov-2022
 * @license see 'LICENSE' file
 *
 * References:
 *      - https://doi.org/10.1016/0020-0190(88)90061-0
 *      - "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *      - "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

use std::collections::HashMap;
use std::fs::File;
use std::io::Write;

mod expr;
mod types;
mod idents;

use types::*;


// generate code for partial semantic rule matches
const GEN_PARTIALS : bool = true;


const CODE_APPLY_PARTIAL_RULE : &str =
	r#"fn apply_partial_rule(&mut self, rule_id : TSemanticId, arg_len : TIndex, before_shift : bool)
	{
		let mut rule_len = arg_len;
		if before_shift
		{
			rule_len += 1;
		}

		//println!("Active rules: {:?}", self.active_rules);
		let mut already_seen_active_rule : bool = false;
		let mut insert_new_active_rule : bool = false;
		let mut seen_tokens_old : isize = -1;

		let mut rulestack : Option<&mut Vec<ActiveRule>> = self.active_rules.get_mut(&rule_id);
		if rulestack.is_some()
		{
			if !rulestack.as_ref().unwrap().is_empty()
			{
				let active_rule = rulestack.as_mut().unwrap().last_mut().unwrap();
				seen_tokens_old = active_rule.seen_tokens as isize;

				if before_shift
				{
					if active_rule.seen_tokens < rule_len
					{
						active_rule.seen_tokens = rule_len;
					}
					else
					{
						insert_new_active_rule = true;
					}

				}
				else  // before jump
				{
					if active_rule.seen_tokens == rule_len
					{
						already_seen_active_rule = true;
					}
					else
					{
						active_rule.seen_tokens = rule_len;
					}
				}
			}
			else
			{
				insert_new_active_rule = true;
			}
		}
		else
		{
			self.active_rules.insert(rule_id, Vec::<ActiveRule>::new());
			rulestack = self.active_rules.get_mut(&rule_id);
			insert_new_active_rule = true;
		}

		if insert_new_active_rule
		{
			seen_tokens_old = -1;

			let mut active_rule = ActiveRule::new();
			active_rule.seen_tokens = rule_len;
			active_rule.handle = self.cur_rule_handle;
			self.cur_rule_handle += 1;

			rulestack.as_mut().unwrap().push(active_rule);
		}

		if !already_seen_active_rule
		{
			let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
			if semantics == None
			{
				self.error_semantics(rule_id);
				return;
			}

			let active_rule = rulestack.as_mut().unwrap().last_mut().unwrap();

			let mut args : Vec<Symbol> = Vec::<Symbol>::new();
			args.reserve(rule_len);

			for _i in 0..arg_len
			{
				args.insert(0, self.symbol[self.symbol.len() - arg_len + _i].clone());
			}

			if !before_shift || seen_tokens_old < (rule_len as isize - 1)
			{
				if self.debug
				{
					println!("Partially applying rule {} with {} arguments (handle {}). Before shift: {}.",
						rule_id, arg_len, active_rule.handle, before_shift);
				}

				active_rule.retval = (*semantics.unwrap())(
					args.clone(), false, active_rule.retval as TLVal);
			}

			if before_shift
			{
				args.push(self.lookahead.as_ref().unwrap().clone());

				if self.debug
				{
					println!("Partially applying rule {} with {} arguments (handle {}). Before shift: {}.",
						rule_id, rule_len, active_rule.handle, before_shift);
				}

				active_rule.retval = (*semantics.unwrap())(
					args, false, active_rule.retval as TLVal);
			}
		}
	}"#;


const CODE_APPLY_RULE_PARTIALS : &str =
		r#"if self.use_partials
		{
			let rulestack : Option<&mut Vec<ActiveRule>> = self.active_rules.get_mut(&rule_id);
			if rulestack.is_some() && !rulestack.as_ref().unwrap().is_empty()
			{
				let active_rule = rulestack.unwrap().pop();
				retval = active_rule.as_ref().unwrap().retval;
				handle = active_rule.as_ref().unwrap().handle as isize;
			}
		}"#;


const CODE_ACTIVE_RULES_DECL : &str =
	r#"active_rules : HashMap<TSemanticId, Vec<ActiveRule>>,
	cur_rule_handle : isize,"#;


const CODE_ACTIVE_RULES_NEW : &str =
	r#"active_rules : HashMap::<TSemanticId, Vec<ActiveRule>>::new(),
			cur_rule_handle : 0,"#;


const CODE_ACTIVE_RULES_RESET : &str =
	r#"self.active_rules.clear();
		self.cur_rule_handle = 0;"#;


const CODE : &str = r#"/*
 * Parser created using liblalr1 by Tobias Weber, 2020-2024.
 * DOI: https://doi.org/10.5281/zenodo.6987396
 */

use std::collections::HashMap;
use std::mem::take;

use types::{*};
use common::{*};

pub struct Parser
{
	symbol : Vec<Symbol>,

	dist_to_jump : usize,

	failed : bool,
	accepted : bool,

	%%ACTIVE_RULES_DECL%%

	lookahead : Option<Symbol>,

	input : Vec<Symbol>,
	next_input_index : usize,

	semantics : HashMap<TSemanticId, TSemantics>,

	debug : bool,
	use_partials : bool,
	end : TSymbolId,
}

impl Parser
{
	pub fn new() -> Parser
	{
		let mut parser : Parser = Parser
		{
			symbol : Vec::<Symbol>::new(),
			dist_to_jump : 0,

			failed : false,
			accepted : false,

			%%ACTIVE_RULES_NEW%%

			lookahead : None,

			semantics : HashMap::<TSemanticId, TSemantics>::new(),
			input : Vec::<Symbol>::new(),
			next_input_index : 0,

			debug : false,
			use_partials : true,
			end : lalr1_tables::END,
		};

		parser.reset();
		parser
	}

	fn next_lookahead(&mut self)
	{
		self.lookahead = Some(self.input[self.next_input_index].clone());

		if self.debug
		{
			println!("Lookahead: {:?}, input index: {}.",
				self.lookahead, self.next_input_index);
		}

		self.next_input_index += 1;
        }

	fn push_lookahead(&mut self)
	{
		self.symbol.push(take(&mut self.lookahead).unwrap());
		self.next_lookahead();
	}

	fn apply_rule(&mut self, rule_id : TSemanticId, num_rhs : TIndex, lhs_id : TSymbolId)
	{
		let mut retval : TLVal = 0 as TLVal;
		let mut handle : isize = -1;

		%%APPLY_RULE_PARTIALS%%

		if self.debug
		{
			println!("Applying rule {} with {} arguments (handle {}).", rule_id, num_rhs, handle);
		}

		self.dist_to_jump = num_rhs;

		let mut args : Vec<Symbol> = Vec::<Symbol>::new();
		args.reserve(num_rhs);

		for _i in 0..num_rhs
		{
			args.insert(0, self.symbol.pop().unwrap());
		}

		let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
		if semantics != None
		{
			retval = (*semantics.unwrap())(args, true, retval);
		}

		self.symbol.push(Symbol{
			is_term : false,
			id : lhs_id,
			val : retval,
			strval : None,
		});
	}

	%%APPLY_PARTIAL_RULE%%

	fn error_term(&mut self, state_idx : usize, sym_id : TSymbolId)
	{
		println!("Error: Invalid terminal transition {sym_id} in state {state_idx}.");
		self.failed = true;
	}

	fn error_nonterm(&mut self, state_idx : usize, sym_id : TSymbolId)
	{
		println!("Error: Invalid non-terminal transition {sym_id} in state {state_idx}.");
		self.failed = true;
	}

	fn error_semantics(&mut self, rule_id : TSemanticId)
	{
		println!("Semantic rule {0} is not defined.", rule_id);
		self.failed = true;
	}


%%STATES%%
}

impl Parsable for Parser
{
	fn set_debug(&mut self, debug : bool)
	{
		self.debug = debug;
	}

	fn set_partials(&mut self, use_partials : bool)
	{
		self.use_partials = use_partials;
	}

	fn get_end_id(&self) -> TSymbolId
	{
		self.end
	}

	fn set_input(&mut self, input: &[Symbol])
	{
		self.input = (*input).to_vec();
	}

	fn set_semantics(&mut self, sema : &[(TSemanticId, TSemantics)])
	{
		self.semantics.clear();

		for _i in 0..(*sema).len()
		{
			self.semantics.insert((*sema)[_i].0, (*sema)[_i].1);
		}
	}

	fn get_top_symbol(&self) -> Option<&Symbol>
	{
		if self.symbol.len() == 0
		{
			None
		}
		else
		{
			self.symbol.last()
		}
	}

	fn reset(&mut self)
	{
		self.next_input_index = 0;
		self.lookahead = None;
		self.symbol.clear();
		self.dist_to_jump = 0;
		%%ACTIVE_RULES_RESET%%

		self.failed = false;
		self.accepted = false;
	}

	fn parse(&mut self) -> bool
	{
		self.reset();
		self.next_lookahead();
		self.state_%%START_IDX%%();

		self.accepted
	}
}
"#;


fn has_table_entry(tab : &[TIndex]) -> bool
{
	for iter in tab.iter()
	{
		if *iter != lalr1_tables::ERR
		{
			return true;
		}
	}

	false
}


fn get_table_id(tab : &[(TSymbolId, TIndex, &str)], idx : TIndex) -> TSymbolId
{
	for entry in tab
	{
		if entry.1 == idx
		{
			return entry.0;
		}
	}

	println!("Error: Table index {idx} was not found.");
	lalr1_tables::ERR
}


fn get_table_index(tab : &[(TSymbolId, TIndex, &str)], id : TSymbolId) -> TIndex
{
	for entry in tab
	{
		if entry.0 == id
		{
			return entry.1;
		}
	}

	println!("Error: Table id {id} was not found.");
	lalr1_tables::ERR
}


fn get_table_id_str(tab : &[(TSymbolId, TIndex, &str)], idx : TIndex) -> (TSymbolId, String)
{
	for entry in tab
	{
		if entry.1 == idx
		{
			return (entry.0, entry.2.to_string());
		}
	}

	println!("Error: Table index {idx} was not found.");
	(lalr1_tables::ERR, "err".to_string())
}


fn get_semantic_table_id(tab : &[(TSemanticId, TIndex)], idx : TIndex) -> TSemanticId
{
	for entry in tab
	{
		if entry.1 == idx
		{
			return entry.0;
		}
	}

	println!("Error: Semantic table index {idx} was not found.");
	lalr1_tables::ERR
}


fn create_states() -> String
{
	let mut states : String = String::new();
	let num_states = lalr1_tables::SHIFT.len();

	for state_idx in 0..num_states
	{
		// main parsing tables
		let shift = &lalr1_tables::SHIFT[state_idx];
		let reduce = &lalr1_tables::REDUCE[state_idx];
		let jump = &lalr1_tables::JUMP[state_idx];

		// partial rule tables
		let part_term = &lalr1_tables::PARTIALS_RULE_TERM[state_idx];
		let part_nonterm = &lalr1_tables::PARTIALS_RULE_NONTERM[state_idx];
		let part_term_len = &lalr1_tables::PARTIALS_MATCHLEN_TERM[state_idx];
		let part_nonterm_len = &lalr1_tables::PARTIALS_MATCHLEN_NONTERM[state_idx];
		let part_nonterm_lhs = &lalr1_tables::PARTIALS_LHS_NONTERM[state_idx];

		let num_terms = shift.len();
		let num_nonterms = jump.len();

		let has_shift_entry = has_table_entry(shift);
		let has_jump_entry = has_table_entry(jump);

		states += &format!("\tfn state_{state_idx}(&mut self)\n");
		states += "\t{\n";

		if has_shift_entry
		{
			states += "\t\tlet mut next_state : Option<fn(&mut Parser)> = None;\n"
		}

		states += "\t\tlet sym_id : TSymbolId = self.lookahead.as_ref().unwrap().id;\n";
		states += "\t\tmatch sym_id\n\t\t{\n";

		let mut rules_term_id : HashMap<TIndex, Vec<(TSymbolId, String)>>
			= HashMap::<TIndex, Vec<(TSymbolId, String)>>::new();
		let mut acc_term_id : Vec<(TSymbolId, String)> = Vec::<(TSymbolId, String)>::new();

		for term_idx in 0..num_terms
		{
			let newstate_idx = shift[term_idx];
			let rule_idx = reduce[term_idx];
			let (term_id, term_str) : (TSymbolId, String) = get_table_id_str(&lalr1_tables::TERM_IDX, term_idx);

			if newstate_idx != lalr1_tables::ERR
			{
				// shift
				states += &format!("\t\t\t{term_id} => // {term_str}\n");
				states += &format!("\t\t\t{{\n");
				states += &format!("\t\t\t\tnext_state = Some(Parser::state_{newstate_idx});\n");

				// partial rules
				if GEN_PARTIALS
				{
					let partial_idx = part_term[term_idx];
					if partial_idx != lalr1_tables::ERR
					{
						let partial_id = get_semantic_table_id(&lalr1_tables::SEMANTIC_IDX, partial_idx);
						let partial_len = part_term_len[term_idx];

						states += &format!("\t\t\t\tif self.use_partials\n");
						states += &format!("\t\t\t\t{{\n");
						states += &format!("\t\t\t\t\tself.apply_partial_rule({partial_id}, {partial_len}, true);\n");
						states += &format!("\t\t\t\t}}\n");
					}
				}

				states += &format!("\t\t\t}},\n");
			}
			else if rule_idx != lalr1_tables::ERR
			{
				if rule_idx == lalr1_tables::ACC
				{
					// accept
					acc_term_id.push((term_id, term_str));
				}
				else
				{
					// reduce
					let elem = rules_term_id.get_mut(&rule_idx);
					if elem.is_some()
					{
						elem.unwrap().push((term_id, term_str));
					}
					else
					{
						rules_term_id.insert(rule_idx, [(term_id, term_str)].to_vec());
					}
				}
			}
		}

		for (rule_idx, sym_ids) in &rules_term_id
		{
			// reduce
			let cases : String = sym_ids.iter().map(|elem| elem.0.to_string()).
				collect::<Vec<String>>().join(" | ");
			let comment : String = sym_ids.iter().map(|elem| elem.1.clone()).
				collect::<Vec<String>>().join(" | ");

			let rule_id : TSemanticId = get_semantic_table_id(&lalr1_tables::SEMANTIC_IDX, *rule_idx);
			let num_rhs : TIndex = lalr1_tables::NUM_RHS_SYMS[*rule_idx];
			let lhs_id : TSymbolId = get_table_id(
				&lalr1_tables::NONTERM_IDX, lalr1_tables::LHS_IDX[*rule_idx]);

			states += &format!("\t\t\t// {comment}\n");
			states += &format!("\t\t\t{cases} => self.apply_rule({rule_id}, {num_rhs}, {lhs_id}),\n");
		}

		if acc_term_id.len() > 0
		{
			let acc_cases : String = acc_term_id.iter().map(|elem| elem.0.to_string()).
				collect::<Vec<String>>().join(" | ");
			let acc_comment : String = acc_term_id.iter().map(|elem| elem.1.clone()).
				collect::<Vec<String>>().join(" | ");

			states += &format!("\t\t\t// {acc_comment}\n");
			states += &format!("\t\t\t{acc_cases} => self.accepted = true,\n");
		}
		states += &format!("\t\t\t_ => self.error_term({}, sym_id),\n", state_idx);
		states += "\t\t}\n";  // end match

		if has_shift_entry
		{
			// shift
			states += "\t\tif next_state.is_some()\n\t\t{\n";
			states += "\t\t\tself.push_lookahead();\n";
			states += "\t\t\tnext_state.unwrap()(self);\n";
			states += "\t\t}\n";
		}

		if has_jump_entry
		{
			// jump
			states += "\t\twhile self.dist_to_jump == 0 && self.symbol.len() > 0 && !self.accepted && !self.failed\n\t\t{\n";

			states += "\t\t\tlet top_sym : &Symbol = self.get_top_symbol().unwrap();\n";
			states += "\t\t\tif top_sym.is_term\n\t\t\t{\n";
			states += "\t\t\t\tbreak;\n";
			states += "\t\t\t}\n";  // end if

			states += "\t\t\tmatch top_sym.id\n\t\t\t{\n";

			for nonterm_idx in 0..num_nonterms
			{
				let jump_state_idx = jump[nonterm_idx];
				if jump_state_idx != lalr1_tables::ERR
				{
					let (nonterm_id, nonterm_str) : (TSymbolId, String) = get_table_id_str(
						&lalr1_tables::NONTERM_IDX, nonterm_idx);

					states += &format!("\t\t\t\t{nonterm_id} => // {nonterm_str}\n");
					states += &format!("\t\t\t\t{{\n");

					// partial rules
					if GEN_PARTIALS
					{
						let lhs_id = part_nonterm_lhs[nonterm_idx];
						if lhs_id != lalr1_tables::ERR
						{
							let lhs_idx = get_table_index(&lalr1_tables::NONTERM_IDX, lhs_id);
							let partial_idx = part_nonterm[lhs_idx];
							if partial_idx != lalr1_tables::ERR
							{
								let partial_id = get_semantic_table_id(&lalr1_tables::SEMANTIC_IDX, partial_idx);
								let partial_len = part_nonterm_len[lhs_idx];

								states += &format!("\t\t\t\t\tif self.use_partials\n");
								states += &format!("\t\t\t\t\t{{\n");
								states += &format!("\t\t\t\t\t\tself.apply_partial_rule({partial_id}, {partial_len}, false);\n");
								states += &format!("\t\t\t\t\t}}\n");
							}
						}
					}

					states += &format!("\t\t\t\t\tself.state_{jump_state_idx}();\n");
					states += &format!("\t\t\t\t}},\n");
				}
			}

			states += &format!("\t\t\t\t_ => self.error_nonterm({}, top_sym.id),\n", state_idx);

			states += "\t\t\t}\n";  // end match
			states += "\t\t}\n";  // end while
		}

		states += "\t\tif !self.accepted && !self.failed\n\t\t{\n";
		states += "\t\t\tself.dist_to_jump -= 1;\n";
		states += "\t\t}\n";  // end if

		states += "\t}\n";  // end state function
		if state_idx < num_states-1 { states += "\n"; }
	}

	states
}


fn main()
{
	let mut code = CODE.to_string();
	let states : String = create_states();
	code = code
		.replace("%%STATES%%", &states)
		.replace("%%START_IDX%%", &lalr1_tables::START.to_string());

	if GEN_PARTIALS
	{
		// insert code blocks for partial rules
		code = code
			.replace("%%APPLY_PARTIAL_RULE%%", &CODE_APPLY_PARTIAL_RULE)
			.replace("%%APPLY_RULE_PARTIALS%%", &CODE_APPLY_RULE_PARTIALS)
			.replace("%%ACTIVE_RULES_DECL%%", &CODE_ACTIVE_RULES_DECL)
			.replace("%%ACTIVE_RULES_NEW%%", &CODE_ACTIVE_RULES_NEW)
			.replace("%%ACTIVE_RULES_RESET%%", &CODE_ACTIVE_RULES_RESET);
	}
	else
	{
		// remove markers for partial rules
		code = code
			.replace("%%APPLY_PARTIAL_RULE%%", &"")
			.replace("%%APPLY_RULE_PARTIALS%%", &"")
			.replace("%%ACTIVE_RULES_DECL%%", &"")
			.replace("%%ACTIVE_RULES_NEW%%", &"")
			.replace("%%ACTIVE_RULES_RESET%%", &"");
	}

	let outfilename : &str = &"generated_parser.rs";
	let mut outfile = File::create(outfilename).expect("Cannot create file.");
	match outfile.write(code.as_bytes())
	{
		Ok(res) => println!("Successfully wrote parser \"{outfilename}\" with {res:?} bytes."),
		Err(res) => println!("Failed to write parser \"{outfilename}\": {res:?}."),
	}
}
