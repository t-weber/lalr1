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
semantics[ids.SemanticIds.start] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"];
};
semantics[ids.SemanticIds.brackets] = (args, done, retval) =>
{
	if(!done) return null;
	return args[1]["val"];
};

semantics[ids.SemanticIds.add] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"] + args[2]["val"];
};
semantics[ids.SemanticIds.sub] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"] - args[2]["val"];
};
semantics[ids.SemanticIds.mul] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"] * args[2]["val"];
};
semantics[ids.SemanticIds.div] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"] / args[2]["val"];
};
semantics[ids.SemanticIds.mod] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"] % args[2]["val"];
};
semantics[ids.SemanticIds.pow] = (args, done, retval) =>
{
	if(!done) return null;
	return Math.pow(args[0]["val"], args[2]["val"]);
};

semantics[ids.SemanticIds.uadd] = (args, done, retval) =>
{
	if(!done) return null;
	return args[1]["val"];
};
semantics[ids.SemanticIds.usub] = (args, done, retval) =>
{
	if(!done) return null;
	return -args[1]["val"];
};

semantics[ids.SemanticIds.real] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"];
};
semantics[ids.SemanticIds.int] = (args, done, retval) =>
{
	if(!done) return null;
	return args[0]["val"];
};


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
