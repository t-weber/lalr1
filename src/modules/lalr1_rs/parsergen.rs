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

// TODO
//mod types;
//mod common;
//mod expr;

use types::{*};
use common::{*};

pub struct Parser
{
	symbol : Vec<Symbol>,

	dist_to_jump : usize,
	accepted : bool,

	lookahead : Option<Symbol>,

	input : Vec<Symbol>,
	next_input_index : usize,

	semantics : HashMap<TSemanticId, TSemantics>,

	debug : bool,
}

impl Parser
{
	pub fn new() -> Parser
	{
		let mut parser : Parser = Parser
		{
			symbol : Vec::<Symbol>::new(),
			dist_to_jump : 0,
			accepted : false,

			lookahead : None,

			semantics : HashMap::<TSemanticId, TSemantics>::new(),
			input : Vec::<Symbol>::new(),
			next_input_index : 0,

			debug : false,
		};

		parser.reset();
		parser
	}

	pub fn set_debug(&mut self, debug : bool)
	{
		self.debug = debug;
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

		let mut retval : TLVal = 0;
		let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
		if semantics != None
		{
			retval = (*semantics.unwrap())(args);
		}

		self.symbol.push(Symbol{is_term:false, id:lhs_id, val:retval});
        }

%%STATES%%
}

impl Parsable for Parser
{
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
		self.accepted = false;
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
		let num_numterms = jump.len();

		let has_shift_entry = has_table_entry(shift);
		let has_jump_entry = has_table_entry(jump);

		states += &format!("\tfn state_{state_idx}(&mut self)\n");
		states += "\t{\n";

		if has_shift_entry
		{
			states += "\t\tlet mut next_state : Option<fn(&mut Parser)> = None;\n"
		}

		states += "\t\tmatch self.lookahead.unwrap().id\n\t\t{\n";

		let mut rules_term_id : HashMap<TIndex, Vec<TSymbolId>> = HashMap::<TIndex, Vec<TSymbolId>>::new();
		let mut acc_term_id : Vec<TSymbolId> = Vec::<TSymbolId>::new();

		for term_idx in 0..num_terms
		{
			let newstate_idx = shift[term_idx];
			let rule_idx = reduce[term_idx];
			let term_id = get_table_id(&lalr1_tables::TERM_IDX, term_idx);

			if newstate_idx != lalr1_tables::ERR
			{
				states += &format!("\t\t\t{term_id} => next_state = Some(Parser::state_{newstate_idx}),\n");
			}
			else if rule_idx != lalr1_tables::ERR
			{
				if rule_idx == lalr1_tables::ACC
				{
					acc_term_id.push(term_id);
				}
				else
				{
					let mut elem = rules_term_id.get_mut(&rule_idx);
					if elem.is_some()
					{
						elem.unwrap().push(term_id);
					}
					else
					{
						rules_term_id.insert(rule_idx, [term_id].to_vec());
					}
				}
			}
		}

		for (rule_idx, sym_ids) in &rules_term_id
		{
			let cases : String = sym_ids.iter().map(|elem| elem.to_string()).
				collect::<Vec<String>>().join(" | ");

			// TODO
			let rule_id : TSemanticId = 0;
			let num_rhs : TIndex = 0;
			let lhs_id : TSymbolId = 0;

			states += &format!("\t\t\t{cases} => self.apply_rule({rule_id}, {num_rhs}, {lhs_id}),\n");
		}

		if acc_term_id.len() > 0
		{
			let acc_cases : String = acc_term_id.iter().map(|elem| elem.to_string()).
				collect::<Vec<String>>().join(" | ");

			states += &format!("\t\t\t{acc_cases} => self.accepted = true,\n");
		}

		states += "\t\t\t_ => println!(\"Invalid terminal transition.\"),\n";
		states += "\t\t}\n";  // end match

		// TODO

		states += "\t}\n";  // end state function
		if state_idx < num_states-1 { states += "\n"; }
	}

	states
}


fn main()
{
	let mut code = CODE.to_string();
	let states : String = create_states();
	code = code.replace("%%STATES%%", &states);

	let mut outfile = File::create("generated_parser.rs").expect("Cannot create file.");
	outfile.write(code.as_bytes());
}
