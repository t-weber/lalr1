/**
 * ast asm generator
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LR1_AST_ASM_H__
#define __LR1_AST_ASM_H__

#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <iostream>
#include <cstdint>

#include "lval.h"
#include "ast.h"
#include "sym.h"
#include "script_vm/opcodes.h"


class ASTAsm : public ASTVisitor
{
public:
	ASTAsm(std::iostream& ostr,
		std::unordered_map<std::size_t, std::tuple<std::string, OpCode>> *ops = nullptr);

	ASTAsm(const ASTAsm&) = delete;
	const ASTAsm& operator=(const ASTAsm&) = delete;

	virtual void visit(const ASTToken<t_lval>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<t_real>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<t_int>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<t_str>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<void*>* ast, std::size_t level) override;
	virtual void visit(const ASTUnary* ast, std::size_t level) override;
	virtual void visit(const ASTBinary* ast, std::size_t level) override;
	virtual void visit(const ASTList* ast, std::size_t level) override;
	virtual void visit(const ASTCondition* ast, std::size_t level) override;
	virtual void visit(const ASTLoop* ast, std::size_t level) override;
	virtual void visit(const ASTFunc* ast, std::size_t level) override;
	virtual void visit(const ASTFuncCall* ast, std::size_t level) override;
	virtual void visit(const ASTJump* ast, std::size_t level) override;
	virtual void visit(const ASTDeclare* ast, std::size_t level) override;

	void SetStream(std::iostream* ostr) { m_ostr = ostr; }
	void SetBinary(bool bin) { m_binary = bin; }

	void AddExternalFunc(const std::string& name) { m_ext_funcs.insert(name); }
	void AlwaysCallExternal(bool b) { m_always_call_ext = b; }
	void PatchFunctionAddresses();
	void FinishCodegen();

	const SymTab& GetSymbolTable() const { return m_symtab; }


private:
	std::iostream* m_ostr{};
	const std::unordered_map<std::size_t, std::tuple<std::string, OpCode>> *m_ops{nullptr};
	bool m_binary{false};

	ConstTab m_consttab{};                 // table of constants
	SymTab m_symtab{};                     // table of symbols

	t_vm_addr m_glob_stack{};              // current offset into global variable stack
	std::unordered_map<std::string, t_vm_addr> m_local_stack{};

	std::string m_cur_func{};              // currently active function
	std::vector<std::string> m_cur_loop{}; // currently active loops in function

	// stream positions where addresses need to be patched in
	std::vector<std::tuple<std::string, std::streampos, t_vm_addr, const ::ASTBase*>>
		m_func_comefroms{};
	std::vector<std::streampos> m_endfunc_comefroms{};
	std::unordered_multimap<std::string, std::streampos> m_loop_begin_comefroms{};
	std::unordered_multimap<std::string, std::streampos> m_loop_end_comefroms{};
	std::vector<std::streampos> m_const_addrs{};

	std::size_t m_glob_label{0};           // jump label counter

	bool m_always_call_ext{false};         // always call external function
	std::unordered_set<std::string> m_ext_funcs{};  // external functions
};


#endif
