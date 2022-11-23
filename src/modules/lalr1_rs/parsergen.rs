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


const CODE : &str = r#"/*
 * Parser created using liblalr1 by Tobias Weber, 2020-2022.
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

	lookahead : Option<Symbol>,

	input : Vec<Symbol>,
	next_input_index : usize,

	semantics : HashMap<TSemanticId, TSemantics>,

	debug : bool,
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

			lookahead : None,

			semantics : HashMap::<TSemanticId, TSemantics>::new(),
			input : Vec::<Symbol>::new(),
			next_input_index : 0,

			debug : false,
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
		if self.debug
		{
			println!("Applying rule {} with {} arguments.", rule_id, num_rhs);
		}

		self.dist_to_jump = num_rhs;

		let mut args : Vec<Symbol> = Vec::<Symbol>::new();
		args.reserve(num_rhs);

		for _i in 0..num_rhs
		{
			args.insert(0, self.symbol.pop().unwrap());
		}

		let mut retval : TLVal = 0 as TLVal;
		let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
		if semantics != None
		{
			retval = (*semantics.unwrap())(args);
		}

		self.symbol.push(Symbol{
			is_term : false,
			id : lhs_id,
			val : retval,
			strval : None,
		});
        }

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

%%STATES%%
}

impl Parsable for Parser
{
	fn set_debug(&mut self, debug : bool)
	{
		self.debug = debug;
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
		let shift = &lalr1_tables::SHIFT[state_idx];
		let reduce = &lalr1_tables::REDUCE[state_idx];
		let jump = &lalr1_tables::JUMP[state_idx];

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
				states += &format!("\t\t\t{term_id} => next_state = Some(Parser::state_{newstate_idx}), // {term_str}\n");
			}
			else if rule_idx != lalr1_tables::ERR
			{
				if rule_idx == lalr1_tables::ACC
				{
					acc_term_id.push((term_id, term_str));
				}
				else
				{
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
			let cases : String = sym_ids.iter().map(|elem| elem.0.to_string()).
				collect::<Vec<String>>().join(" | ");
			let comment : String = sym_ids.iter().map(|elem| elem.1.clone()).
				collect::<Vec<String>>().join(" | ");

			let rule_id : TSemanticId = get_semantic_table_id(
				&lalr1_tables::SEMANTIC_IDX, *rule_idx);
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
			states += "\t\tif next_state.is_some()\n\t\t{\n";
			states += "\t\t\tself.push_lookahead();\n";
			states += "\t\t\tnext_state.unwrap()(self);\n";
			states += "\t\t}\n";
		}

		if has_jump_entry
		{
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

					states += &format!("\t\t\t\t{nonterm_id} => self.state_{jump_state_idx}(), // {nonterm_str}\n");
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

	let outfilename : &str = &"generated_parser.rs";
	let mut outfile = File::create(outfilename).expect("Cannot create file.");
	match outfile.write(code.as_bytes())
	{
		Ok(res) => println!("Successfully wrote parser \"{outfilename}\" with {res:?} bytes."),
		Err(res) => println!("Failed to write parser \"{outfilename}\": {res:?}."),
	}
}
