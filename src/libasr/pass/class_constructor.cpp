#include <libasr/asr.h>
#include <libasr/containers.h>
#include <libasr/exception.h>
#include <libasr/asr_utils.h>
#include <libasr/asr_verify.h>
#include <libasr/pass/replace_class_constructor.h>
#include <libasr/pass/pass_utils.h>


namespace LCompilers {

using ASR::down_cast;
using ASR::is_a;

class ReplaceStructConstructor: public ASR::BaseExprReplacer<ReplaceStructConstructor> {

    public:

    Allocator& al;
    Vec<ASR::stmt_t*>& pass_result;
    bool& remove_original_statement;
    bool realloc_lhs;

    SymbolTable* current_scope;
    ASR::expr_t* result_var;

    ReplaceStructConstructor(Allocator& al_, Vec<ASR::stmt_t*>& pass_result_,
        bool& remove_original_statement_) :
    al(al_), pass_result(pass_result_),
    remove_original_statement(remove_original_statement_),
    current_scope(nullptr), result_var(nullptr) {}

    void replace_StructConstructor(ASR::StructConstructor_t* x) {
        Vec<ASR::stmt_t*>* result_vec = &pass_result;
        PassUtils::ReplacerUtils::replace_StructConstructor(
            x, this, false, remove_original_statement, result_vec, false,
        ASR::cast_kindType::IntegerToInteger, nullptr, realloc_lhs);
    }
};

class StructConstructorVisitor : public ASR::CallReplacerOnExpressionsVisitor<StructConstructorVisitor>
{
    private:

        Allocator& al;
        bool remove_original_statement;
        ReplaceStructConstructor replacer;
        Vec<ASR::stmt_t*> pass_result;
        bool realloc_lhs;

    public:

        StructConstructorVisitor(Allocator& al_, bool realloc_lhs_) :
        al(al_), remove_original_statement(false),
        replacer(al_, pass_result, remove_original_statement), realloc_lhs(realloc_lhs_) {
            pass_result.n = 0;
            pass_result.reserve(al, 0);
        }

        void call_replacer() {
            replacer.current_expr = current_expr;
            replacer.current_scope = current_scope;
            replacer.realloc_lhs = realloc_lhs;
            replacer.replace_expr(*current_expr);
        }

        void transform_stmts(ASR::stmt_t **&m_body, size_t &n_body) {
            Vec<ASR::stmt_t*> body;
            body.reserve(al, n_body);

            for (size_t i = 0; i < n_body; i++) {
                pass_result.n = 0;
                pass_result.reserve(al, 1);
                remove_original_statement = false;
                replacer.result_var = nullptr;
                visit_stmt(*m_body[i]);
                for (size_t j = 0; j < pass_result.size(); j++) {
                    body.push_back(al, pass_result[j]);
                }
                if( !remove_original_statement ) {
                    body.push_back(al, m_body[i]);
                }
                remove_original_statement = false;
            }
            m_body = body.p;
            n_body = body.size();
            replacer.result_var = nullptr;
            pass_result.n = 0;
            pass_result.reserve(al, 0);
        }

        void visit_Variable(const ASR::Variable_t& /*x*/) {
            // Do nothing, already handled in init_expr pass
        }

        void visit_Assignment(const ASR::Assignment_t &x) {
            if (x.m_overloaded) {
                this->visit_stmt(*x.m_overloaded);
                remove_original_statement = false;
                return ;
            }

            replacer.result_var = x.m_target;
            ASR::expr_t** current_expr_copy_9 = current_expr;
            current_expr = const_cast<ASR::expr_t**>(&(x.m_value));
            this->call_replacer();
            current_expr = current_expr_copy_9;
            if( !remove_original_statement ) {
                this->visit_expr(*x.m_value);
            }
        }

};

void pass_replace_class_constructor(Allocator &al,
    ASR::TranslationUnit_t &unit,
    const LCompilers::PassOptions& pass_options) {
    StructConstructorVisitor v(al, pass_options.realloc_lhs);
    v.visit_TranslationUnit(unit);
    PassUtils::UpdateDependenciesVisitor w(al);
    w.visit_TranslationUnit(unit);
}


} // namespace LCompilers
