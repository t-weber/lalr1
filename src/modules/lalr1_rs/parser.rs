/*
 * LALR(1) parser
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 12-nov-2022
 * @license see 'LICENSE' file
 *
 * References:
 *      - "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *      - "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */


use std::mem::take;
use std::collections::HashMap;

use types::{*};
use common::{*};


pub struct Parser
{
	// parser stacks
	state : Vec<TIndex>,
	symbol : Vec<Symbol>,

	// index maps
	map_term_idx : HashMap<TSymbolId, TIndex>,
	map_nonterm_id : HashMap<TIndex, TSymbolId>,
	map_semantic_id : HashMap<TIndex, TSemanticId>,

	// lookahead
	lookahead : Option<Symbol>,
	lookahead_index : TIndex,

	// input tokens
	input : Vec<Symbol>,
	next_input_index : usize,

	// semantic functions
	semantics : HashMap<TSemanticId, TSemantics>,

	debug : bool,
}


impl Parser
{
	/*
	 * creates a new parser object
	 */
	pub fn new() -> Parser
	{
		let mut parser : Parser = Parser
		{
			state : Vec::<TIndex>::new(),
			symbol : Vec::<Symbol>::new(),

			map_term_idx : HashMap::<TSymbolId, TIndex>::new(),
			map_nonterm_id : HashMap::<TIndex, TSymbolId>::new(),
			map_semantic_id : HashMap::<TIndex, TSemanticId>::new(),

			lookahead : None,
			lookahead_index : 0,

			semantics : HashMap::<TSemanticId, TSemantics>::new(),
			input : Vec::<Symbol>::new(),
			next_input_index : 0,

			debug : false,
		};

		for term_idx in lalr1_tables::TERM_IDX
		{
			parser.map_term_idx.insert(term_idx.0, term_idx.1);
		}

		for nonterm_idx in lalr1_tables::NONTERM_IDX
		{
			parser.map_nonterm_id.insert(nonterm_idx.1, nonterm_idx.0);
		}

		for semantic_idx in lalr1_tables::SEMANTIC_IDX
		{
			parser.map_semantic_id.insert(semantic_idx.1, semantic_idx.0);
		}

		parser.reset();
		parser
	}


	/*
	 * get the terminal table index from its id
	 */
	fn get_term_table_index(&self, id : TSymbolId) -> TIndex
	{
		let idx : Option<&TIndex> = self.map_term_idx.get(&id);
		//println!("Terminal id={:?} -> idx={:?}", id, idx);
		*idx.unwrap()
	}


	/**
	 * get the table id from its index
	 */
	fn get_table_id(map : &HashMap::<usize, usize>, idx : TIndex) -> Option<&TSymbolId>
	{
		let id : Option<&TSymbolId> = map.get(&idx);
		id
	}


	/*
	 * get the semantic table id from its index
	 */
	fn get_semantic_table_id(&self, idx : TIndex) -> TSemanticId
	{
		let id : Option<&TSemanticId> = Self::get_table_id(&self.map_semantic_id, idx);
		*id.unwrap()
	}


	/*
	 * get the nonterminal table id from its index
	 */
	fn get_nonterm_table_id(&self, idx : TIndex) -> TSymbolId
	{
		let id : Option<&TSymbolId> = Self::get_table_id(&self.map_nonterm_id, idx);
		*id.unwrap()
	}


	/*
	 * get the next lookahead token and its table index
	 */
        fn next_lookahead(&mut self)
        {
		self.lookahead = Some(self.input[self.next_input_index].clone());
		self.lookahead_index = self.get_term_table_index(
			self.lookahead.as_ref().unwrap().id);

		if self.debug
		{
			println!("Lookahead: {:?}, input index: {}.",
				self.lookahead, self.next_input_index);
		}

		self.next_input_index += 1;
	}


	/*
	 * push the current lookahead token onto the symbol stack
	 * and get the next lookahead
	 */
        fn push_lookahead(&mut self)
        {
		self.symbol.push(take(&mut self.lookahead).unwrap());
		self.next_lookahead();
	}


	/*
	 * reduce using a semantic rule with given id
	 */
	fn apply_rule(&mut self, rule_id : TSemanticId, num_rhs : TIndex, lhs_id : TSymbolId)
	{
		if self.debug
		{
			println!("Applying rule {} with {} arguments.", rule_id, num_rhs);
		}

		// get arguments
		let mut args : Vec<Symbol> = Vec::<Symbol>::new();
		args.reserve(num_rhs);

		for _i in 0..num_rhs
		{
			args.insert(0, self.symbol.pop().unwrap());
			self.state.pop();
		}

		// call semantic function
		let mut retval : TLVal = 0 as TLVal;
		let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
		if semantics != None
		{
			retval = (*semantics.unwrap())(args);
		}

		// push result
		self.symbol.push(Symbol{
			is_term : false,
			id : lhs_id,
			val : retval,
			strval : None,
		});
	}

	fn error(&mut self, str : &str)
	{
		println!("Error: {}", str);
	}
}


impl Parsable for Parser
{
	/*
	 * set the input tokens
	 */
	fn set_input(&mut self, input: &[Symbol])
	{
		self.input = (*input).to_vec();
	}


	/*
	 * enable debugging
	 */
	fn set_debug(&mut self, debug : bool)
	{
		self.debug = debug;
	}


	/*
	 * set the semantic functions for the rules
	 */
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


	fn get_end_id(&self) -> TSymbolId
	{
		lalr1_tables::END
	}


	fn reset(&mut self)
	{
		self.next_input_index = 0;

		self.lookahead = None;
		self.lookahead_index = 0;

		self.symbol.clear();
		self.state.clear();
		self.state.push(lalr1_tables::START);
	}


	fn parse(&mut self) -> bool
	{
		self.reset();
		self.next_lookahead();

		while self.next_input_index <= self.input.len()
		{
			let top_state : TIndex = *self.state.last().unwrap();

			let shift = &lalr1_tables::SHIFT[top_state];
			let reduce = &lalr1_tables::REDUCE[top_state];

			let new_state : TIndex = shift[self.lookahead_index];
			let rule_index : TIndex = reduce[self.lookahead_index];

			if self.debug
			{
				println!("Top state {}, new state {}, rule index {}, lookahead index {}.",
					top_state, new_state, rule_index, self.lookahead_index);
			}

			if new_state == lalr1_tables::ERR && rule_index == lalr1_tables::ERR
			{
				self.error(&format!("No shift or reduce action defined for state {0} and lookahead {1}.",
					top_state, self.lookahead_index));
				return false;
			}
			else if new_state != lalr1_tables::ERR && rule_index != lalr1_tables::ERR
			{
				self.error(&format!("Shift/reduce conflict for state {0} and lookahead {1}.",
					top_state, self.lookahead_index));
				return false;
			}
			else if rule_index == lalr1_tables::ACC
			{
				if self.debug
				{
					println!("Accepted.");
				}
				return true;
			}

			if new_state != lalr1_tables::ERR
			{
				// shift
				self.state.push(new_state);
				self.push_lookahead();
			}
			else if rule_index != lalr1_tables::ERR
			{
				// reduce
				let num_syms = lalr1_tables::NUM_RHS_SYMS[rule_index];
				let lhs_index = lalr1_tables::LHS_IDX[rule_index];
				let rule_id = self.get_semantic_table_id(rule_index);
				let lhs_id = self.get_nonterm_table_id(lhs_index);

				self.apply_rule(rule_id, num_syms, lhs_id);

				let new_top_state = *self.state.last().unwrap();
				let jump = &lalr1_tables::JUMP[new_top_state];
				let jump_state : TIndex = jump[lhs_index];
				self.state.push(jump_state);
			}
		}

		false
	}
}
