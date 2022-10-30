/**
 * expression parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 16-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

// load lexer
const lexer = require("./lexer.js");
const ids = require("./ids.js");


// load standalone parser
//const expr_parser = require("./expr_parser.js");


// load table-based parser
const parser = require("../../modules/lalr1_js/parser.js");
const tables_file = "./expr.json";
if(process.argv.length >= 3)
	tables_file = process.argv[2];
const tables = require(tables_file);


// create semantics
let semantics = {};
semantics[ids.SemanticIds.start] = (expr) => { return expr["val"]; };
semantics[ids.SemanticIds.brackets] = (bracket_open, expr, bracket_close) => { return expr["val"]; };

semantics[ids.SemanticIds.add] = (expr1, op, expr2) => { return expr1["val"] + expr2["val"]; };
semantics[ids.SemanticIds.sub] = (expr1, op, expr2) => { return expr1["val"] - expr2["val"]; };
semantics[ids.SemanticIds.mul] = (expr1, op, expr2) => { return expr1["val"] * expr2["val"]; };
semantics[ids.SemanticIds.div] = (expr1, op, expr2) => { return expr1["val"] / expr2["val"]; };
semantics[ids.SemanticIds.mod] = (expr1, op, expr2) => { return expr1["val"] % expr2["val"]; };
semantics[ids.SemanticIds.pow] = (expr1, op, expr2) => { return Math.pow(expr1["val"], expr2["val"]); };

semantics[ids.SemanticIds.uadd] = (op, expr) => { return expr["val"]; };
semantics[ids.SemanticIds.usub] = (op, expr) => { return -expr["val"]; };

semantics[ids.SemanticIds.real] = (sym_real) => { return sym_real["val"]; };
semantics[ids.SemanticIds.int] = (sym_int) => { return sym_int["val"]; };


// read and process expression
process.stdin.on("data", (data) =>
{
	let theparser = new parser.Parser(tables);
	//let theparser = new expr_parser.Parser();
	const end_token = theparser.get_end_token();

	const input_str = data.toString();
	const input_tokens = lexer.get_tokens(input_str);
	//console.log(input_tokens);
	input_tokens.push([ end_token ]);

	theparser.set_semantics(semantics);
	theparser.set_input(input_tokens);
	let result = theparser.parse();

	if(result != null)
		console.log(result["val"]);
});
