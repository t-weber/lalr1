/**
 * expression parser ids
 *
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24-oct-2022
 * @license see 'LICENSE' file
 */

public class Ids
{
	// semantic ids
	public static final int sem_start_id    = 100;
	public static final int sem_brackets_id = 101;
	public static final int sem_add_id      = 200;
	public static final int sem_sub_id      = 201;
	public static final int sem_mul_id      = 202;
	public static final int sem_div_id      = 203;
	public static final int sem_mod_id      = 204;
	public static final int sem_pow_id      = 205;
	public static final int sem_uadd_id     = 210;
	public static final int sem_usub_id     = 211;
	public static final int sem_call0_id    = 300;
	public static final int sem_call1_id    = 301;
	public static final int sem_call2_id    = 302;
	public static final int sem_real_id     = 400;
	public static final int sem_int_id      = 401;
	public static final int sem_ident_id    = 410;
	public static final int sem_assign_id   = 500;

	// token ids
	public static final int tok_real_id     = 1000;
	public static final int tok_int_id      = 1001;
	public static final int tok_str_id      = 1002;
	public static final int tok_ident_id    = 1003;

	// nonterminals
	public static final int nonterm_start   = 10;
	public static final int nonterm_expr    = 20;
}
