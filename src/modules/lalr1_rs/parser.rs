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

	// partial rules
	active_rules : HashMap<TSemanticId, Vec<ActiveRule>>,
	cur_rule_handle : isize,

	// lookahead
	lookahead : Option<Symbol>,
	lookahead_index : TIndex,

	// input tokens
	input : Vec<Symbol>,
	next_input_index : usize,

	// semantic functions
	semantics : HashMap<TSemanticId, TSemantics>,

	debug : bool,
	use_partials : bool,
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

			active_rules : HashMap::<TSemanticId, Vec<ActiveRule>>::new(),
			cur_rule_handle : 0,

			lookahead : None,
			lookahead_index : 0,

			semantics : HashMap::<TSemanticId, TSemantics>::new(),
			input : Vec::<Symbol>::new(),
			next_input_index : 0,

			debug : false,
			use_partials : true,
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
		// remove fully reduced rule from active rule stack and get return value
		let mut retval : TLVal = 0 as TLVal;
		let mut handle : isize = -1;
		if self.use_partials
		{
			let rulestack : Option<&mut Vec<ActiveRule>> = self.active_rules.get_mut(&rule_id);
			if rulestack.is_some() && !rulestack.as_ref().unwrap().is_empty()
			{
				let active_rule = rulestack.unwrap().pop();
				retval = active_rule.as_ref().unwrap().retval;
				handle = active_rule.as_ref().unwrap().handle as isize;
			}
		}

		if self.debug
		{
			print!("Applying rule {} with {} arguments", rule_id, num_rhs);
			if handle >= 0
			{
				print!(" (handle {})", handle);
			}
			println!(".");
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
		let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
		if semantics != None
		{
			retval = (*semantics.unwrap())(args, true, retval as TLVal);
		}

		// push result
		self.symbol.push(Symbol{
			is_term : false,
			id : lhs_id,
			val : retval,
			strval : None,
		});
	}


	/*
	 * partially apply a semantic rule with given id
	 */
	fn apply_partial_rule(&mut self, rule_id : TSemanticId, arg_len : TIndex, before_shift : bool)
	{
		let mut rule_len = arg_len;
		if before_shift
		{
			// directly count the following lookahead terminal
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
				// no active rule yet
				insert_new_active_rule = true;
			}
		}
		else
		{
			// no active rule yet
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
			// get semantic function
			let semantics : Option<&TSemantics> = self.semantics.get(&rule_id);
			if semantics == None
			{
				self.error(&format!("Semantic rule {0} is not defined.", rule_id));
				return;
			}

			let active_rule = rulestack.as_mut().unwrap().last_mut().unwrap();

			// get arguments for semantic rule
			let mut args : Vec<Symbol> = Vec::<Symbol>::new();
			args.reserve(rule_len);

			for _i in 0..arg_len
			{
				args.insert(0, self.symbol[self.symbol.len() - arg_len + _i].clone());
			}

			if !before_shift || seen_tokens_old < (rule_len as isize - 1)
			{
				// run the semantic rule
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
				// since we already know the next terminal in a shift, include it directly
				args.push(self.lookahead.as_ref().unwrap().clone());

				// run the semantic rule again
				if self.debug
				{
					println!("Partially applying rule {} with {} arguments (handle {}). Before shift: {}.",
						rule_id, rule_len, active_rule.handle, before_shift);
				}

				active_rule.retval = (*semantics.unwrap())(
					args, false, active_rule.retval as TLVal);
			}
		}
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


	/**
	 * enable application of partial semantic rules
	 */
	fn set_partials(&mut self, use_partials : bool)
	{
		self.use_partials = use_partials;
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

		self.active_rules.clear();
		self.cur_rule_handle = 0;

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

			// main parsing tables
			let shift = &lalr1_tables::SHIFT[top_state];
			let reduce = &lalr1_tables::REDUCE[top_state];

			// partial rule tables
			let part_term = &lalr1_tables::PARTIALS_RULE_TERM;
			let part_nonterm = &lalr1_tables::PARTIALS_RULE_NONTERM;
			let part_term_len = &lalr1_tables::PARTIALS_MATCHLEN_TERM;
			let part_nonterm_len = &lalr1_tables::PARTIALS_MATCHLEN_NONTERM;

			// constants
			let err = lalr1_tables::ERR;
			let acc = lalr1_tables::ACC;

			let new_state : TIndex = shift[self.lookahead_index];
			let rule_index : TIndex = reduce[self.lookahead_index];

			if self.debug
			{
				println!("Top state {}, new state {}, rule index {}, lookahead index {}.",
					top_state, new_state, rule_index, self.lookahead_index);
			}

			if new_state == err && rule_index == err
			{
				self.error(&format!("No shift or reduce action defined for state {0} and lookahead {1}.",
					top_state, self.lookahead_index));
				return false;
			}
			else if new_state != err && rule_index != err
			{
				self.error(&format!("Shift/reduce conflict for state {0} and lookahead {1}.",
					top_state, self.lookahead_index));
				return false;
			}

			// accept
			else if rule_index == acc
			{
				if self.debug
				{
					println!("Accepted.");
				}
				return true;
			}

			// shift
			if new_state != err
			{
				// partial rules
				if self.use_partials
				{
					let partial_idx = part_term[top_state][self.lookahead_index];
					if partial_idx != err
					{
						let partial_id = self.get_semantic_table_id(partial_idx);
						let partial_len = part_term_len[top_state][self.lookahead_index];

						self.apply_partial_rule(partial_id, partial_len, true);
					}
				}

				self.state.push(new_state);
				self.push_lookahead();
			}

			// reduce
			else if rule_index != err
			{
				let num_syms = lalr1_tables::NUM_RHS_SYMS[rule_index];
				let lhs_index = lalr1_tables::LHS_IDX[rule_index];
				let rule_id = self.get_semantic_table_id(rule_index);
				let lhs_id = self.get_nonterm_table_id(lhs_index);

				self.apply_rule(rule_id, num_syms, lhs_id);
				let new_top_state = *self.state.last().unwrap();

				// partial rules
				if self.use_partials && self.symbol.len() > 0
				{
					let partial_idx = part_nonterm[new_top_state][lhs_index];
					if partial_idx != err
					{
						let partial_id = self.get_semantic_table_id(partial_idx);
						let partial_len = part_nonterm_len[new_top_state][lhs_index];

						self.apply_partial_rule(partial_id, partial_len, false);
					}
				}		

				let jump = &lalr1_tables::JUMP[new_top_state];
				let jump_state : TIndex = jump[lhs_index];
				self.state.push(jump_state);
			}
		}

		false
	}
}
