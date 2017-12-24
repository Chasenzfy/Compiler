#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "AST.h"
#include "ir.h"
#include "sym_table.h"

InterCodes* newInterCodes() {
    InterCodes* p = (InterCodes*)malloc(sizeof(InterCodes));
    p->next = p->prev = NULL;
    return p;
}

ArgNode* newArgNode(int var_id) {
    ArgNode *arg = (ArgNode*)malloc(sizeof(ArgNode));
    arg->next = NULL;
    arg->var_id = var_id;
    return arg;
}

int newVariableId() {
    static int id = 1;
    return id++;
}

int newLabelId() {
    static int id = 1;
    return id++;
}

static InterCodes* getInterCodesTail(InterCodes* head) {
    if (head == NULL) return NULL;
    while (head->next != NULL) {
        head = head->next;
    }
    return head;
}

InterCodes* concatInterCodes(int count, ...) {
    // input can be:
    //     NULL code1 code2 NULL NULL code3 ...
    if (count < 1) return NULL;

    va_list argp;
    va_start(argp, count);

    InterCodes *head, *tail, *p;
    int i = 1;
    head = tail = va_arg(argp, InterCodes*);

    // find the first Not NULL value
    while (head == NULL && i < count) {
        head = tail = va_arg(argp, InterCodes*);
        i++;
    }
    if (i >= count) return head;

    assert(head);
    assert(tail);
    for (; i < count; i++) {
        p = va_arg(argp, InterCodes*);
        if (p == NULL)
            continue;
        tail = getInterCodesTail(tail);
        assert(tail);
        tail->next = p;
        p->prev = tail;
    }

    va_end(argp);
    return head;
}

InterCodes* genLabelCode(int label_id) {
    InterCodes* codes = newInterCodes();
    codes->code.kind = IR_LABEL;
    codes->code.result.kind = OP_LABEL;
    codes->code.result.u.label_id = label_id;
}

InterCodes* genGotoCode(int label_id) {
    InterCodes* codes = newInterCodes();
    codes->code.kind = IR_GOTO;
    codes->code.result.kind = OP_LABEL;
    codes->code.result.u.label_id = label_id;
}

enum RELOP_TYPE get_relop(ASTNode *RELOP) {
    assert(RELOP);
    assert(RELOP->type == AST_RELOP);

    if (strcmp(RELOP->val.c, "<") == 0) {
        return RELOP_LT;
    } else if (strcmp(RELOP->val.c, "<=") == 0) {
        return RELOP_LE;
    } else if (strcmp(RELOP->val.c, "==") == 0) {
        return RELOP_EQ;
    } else if (strcmp(RELOP->val.c, ">") == 0) {
        return RELOP_GT;
    } else if (strcmp(RELOP->val.c, ">=") == 0) {
        return RELOP_GE;
    } else if (strcmp(RELOP->val.c, "!=") == 0) {
        return RELOP_NE;
    } else {
        assert(0);
    }
}

enum RELOP_TYPE get_reverse_relop(enum RELOP_TYPE relop) {
    switch (relop) {
        case RELOP_LT: return RELOP_GE;
        case RELOP_LE: return RELOP_GT;
        case RELOP_EQ: return RELOP_NE;
        case RELOP_GT: return RELOP_LE;
        case RELOP_GE: return RELOP_LT;
        case RELOP_NE: return RELOP_EQ;
        default: assert(0);
    }
}

InterCodes* translate_Exp(ASTNode* Exp, int place) {
    assert(Exp);
    assert(Exp->type == AST_Exp);

    InterCodes* codes = newInterCodes();
    if (Exp->child->type == AST_INT) { // Exp -> INT
        codes->code.kind = IR_ASSIGN;
        codes->code.result.kind = OP_TEMP;
        codes->code.result.u.var_id = place;
        codes->code.arg1.kind = OP_CONSTANT;
        codes->code.arg1.u.value = Exp->child->val.i;
    } else if (Exp->child->type == AST_ID && Exp->child->sibling == NULL) { // Exp -> ID
        codes->code.kind = IR_ASSIGN;
        codes->code.result.kind = OP_TEMP;
        codes->code.result.u.var_id = place;
        Symbol sym = lookupSymbol(Exp->child->val.c, true);
        // if (sym->u.type->kind != BASIC) {
        //     codes->code.arg1.kind = OP_ADDR;
        // } else {
            codes->code.arg1.kind = OP_VARIABLE;
        // }
        codes->code.arg1.symbol = sym;
    } else if(Exp->child->type == AST_FLOAT) {
        assert(0);
    } else if (Exp->child->type == AST_LP) {  // Exp -> LP Exp RP
        codes = translate_Exp(Exp->child->sibling, place);
    } else if (Exp->child->sibling->type == AST_ASSIGNOP) { // Exp -> EXP1 ASSIGNOP Exp2
        if (Exp->child->child->type == AST_ID) { // Exp1 -> ID
            Symbol variable = lookupSymbol(Exp->child->child->val.c, true);
            int t1 = newVariableId();
            InterCodes* code1 = translate_Exp(Exp->child->sibling->sibling, t1);

            InterCodes* code2 = newInterCodes();
            code2->code.kind = IR_ASSIGN;
            code2->code.result.kind = OP_VARIABLE;
            code2->code.result.symbol = variable;
            code2->code.arg1.kind = OP_TEMP;
            code2->code.arg1.u.var_id = t1;

            InterCodes* code3 = newInterCodes();
            code3->code.kind = IR_ASSIGN;
            code3->code.result.kind = OP_TEMP;
            code3->code.result.u.var_id = place;
            code3->code.arg1.kind = OP_VARIABLE;
            code3->code.arg1.symbol = variable;

            codes = concatInterCodes(3, code1, code2, code3);
        }
        else if (Exp->child->child->subtype == ARRAY_USE) { // Exp1 -> Exp LB Exp RB   i.e. Exp1 is array
            int t1 = newVariableId();
            int t2 = newVariableId();
            InterCodes* code1 = translate_Exp(Exp->child->child, t1);
            InterCodes* code2 = translate_Exp(Exp->child->child->sibling->sibling, t2);

            int t3 = newVariableId();
            InterCodes* code3 = newInterCodes();
            code3->code.kind = IR_MUL;
            code3->code.result.kind = OP_TEMP;
            code3->code.result.u.var_id = t3;
            code3->code.arg1.kind = OP_TEMP;
            code3->code.arg1.u.var_id = t2;
            code3->code.arg2.kind = OP_CONSTANT;
            code3->code.arg2.u.value = getTypeSize(Exp->child->expType);

            int t4 = newVariableId();
            InterCodes* code4 = newInterCodes();
            code4->code.kind = IR_ADD;
            code4->code.result.kind = OP_TEMP;
            code4->code.result.u.var_id = t4;
            code4->code.arg1.kind = OP_TEMP;
            code4->code.arg1.u.var_id = t1;
            code4->code.arg2.kind = OP_TEMP;
            code4->code.arg2.u.var_id = t3;

            int t5 = newVariableId();
            InterCodes* code5 = translate_Exp(Exp->child->sibling->sibling, t5);

            InterCodes* code6 = newInterCodes();
            code6->code.kind = IR_DEREF_L;
            code6->code.result.kind = OP_TEMP;
            code6->code.result.u.var_id = t4;
            code6->code.arg1.kind = OP_TEMP;
            code6->code.arg1.u.var_id = t5;

            InterCodes* code7 = newInterCodes();
            code7->code.kind = IR_ASSIGN;
            code7->code.result.kind = OP_TEMP;
            code7->code.result.u.var_id = place;
            code7->code.arg1.kind = OP_TEMP;
            code7->code.arg1.u.var_id = t5;

            codes = concatInterCodes(7, code1, code2, code3, code4, code5, code6, code7);
        }
        else if (Exp->child->child->subtype == STRUCT_USE) { // Exp1 -> Exp DOT Exp   i.e. Exp1 is struct
            char *name = Exp->child->child->sibling->sibling->val.c;
            int t1 = newVariableId();
            InterCodes* code1 = translate_Exp(Exp->child->child, t1);

            assert(Exp->child->child->expType->kind == STRUCTURE);
            FieldList field = Exp->child->child->expType->u.structure;
            int offset = 0;
            while (strcmp(field->name, name) != 0) {
                offset += getTypeSize(field->type);
                assert(field->tail != NULL);
                field = field->tail;
            }

            int t2 = newVariableId();
            InterCodes* code2 = newInterCodes();
            code2->code.kind = IR_ADD;
            code2->code.result.kind = OP_TEMP;
            code2->code.result.u.var_id = t2;
            code2->code.arg1.kind = OP_TEMP;
            code2->code.arg1.u.var_id = t1;
            code2->code.arg2.kind = OP_CONSTANT;
            code2->code.arg2.u.value = offset;

            int t3 = newVariableId();
            InterCodes* code3 = translate_Exp(Exp->child->sibling->sibling, t3);

            InterCodes* code4 = newInterCodes();
            code4->code.kind = IR_DEREF_L;
            code4->code.result.kind = OP_TEMP;
            code4->code.result.u.var_id = t2;
            code4->code.arg1.kind = OP_TEMP;
            code4->code.arg1.u.var_id = t3;

            InterCodes* code5 = newInterCodes();
            code5->code.kind = IR_ASSIGN;
            code5->code.result.kind = OP_TEMP;
            code5->code.result.u.var_id = place;
            code5->code.arg1.kind = OP_TEMP;
            code5->code.arg1.u.var_id = t3;

            codes = concatInterCodes(5, code1, code2, code3, code4, code5);
        }
        else {
            assert(0);
        }
    } else if (Exp->child->sibling->type == AST_PLUS) { // Exp -> EXP PLUS Exp
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        InterCodes* code3 = newInterCodes();
        code3->code.kind = IR_ADD;
        code3->code.result.kind = OP_TEMP;
        code3->code.result.u.var_id = place;
        code3->code.arg1.kind = OP_TEMP;
        code3->code.arg1.u.var_id = t1;
        code3->code.arg2.kind = OP_TEMP;
        code3->code.arg2.u.var_id = t2;

        codes = concatInterCodes(3, code1, code2, code3);
    } else if (Exp->child->sibling->type == AST_MINUS) { // Exp -> EXP MINUS Exp
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        InterCodes* code3 = newInterCodes();
        code3->code.kind = IR_SUB;
        code3->code.result.kind = OP_TEMP;
        code3->code.result.u.var_id = place;
        code3->code.arg1.kind = OP_TEMP;
        code3->code.arg1.u.var_id = t1;
        code3->code.arg2.kind = OP_TEMP;
        code3->code.arg2.u.var_id = t2;

        codes = concatInterCodes(3, code1, code2, code3);
    } else if (Exp->child->sibling->type == AST_STAR) { // Exp -> EXP STAR Exp
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        InterCodes* code3 = newInterCodes();
        code3->code.kind = IR_MUL;
        code3->code.result.kind = OP_TEMP;
        code3->code.result.u.var_id = place;
        code3->code.arg1.kind = OP_TEMP;
        code3->code.arg1.u.var_id = t1;
        code3->code.arg2.kind = OP_TEMP;
        code3->code.arg2.u.var_id = t2;

        codes = concatInterCodes(3, code1, code2, code3);
    } else if (Exp->child->sibling->type == AST_DIV) { // Exp -> EXP DIV Exp
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        InterCodes* code3 = newInterCodes();
        code3->code.kind = IR_DIV;
        code3->code.result.kind = OP_TEMP;
        code3->code.result.u.var_id = place;
        code3->code.arg1.kind = OP_TEMP;
        code3->code.arg1.u.var_id = t1;
        code3->code.arg2.kind = OP_TEMP;
        code3->code.arg2.u.var_id = t2;

        codes = concatInterCodes(3, code1, code2, code3);
    } else if (Exp->child->type == AST_MINUS) { // Exp -> MINUS Exp
        int t1 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child->sibling, t1);

        InterCodes* code2 = newInterCodes();
        code2->code.kind = IR_SUB;
        code2->code.result.kind = OP_TEMP;
        code2->code.result.u.var_id = place;
        code2->code.arg1.kind = OP_CONSTANT;
        code2->code.arg1.u.value = 0;
        code2->code.arg2.kind = OP_TEMP;
        code2->code.arg2.u.var_id = t1;

        codes = concatInterCodes(2, code1, code2);
    } else if (Exp->child->sibling->type == AST_RELOP ||
               Exp->child->sibling->type == AST_AND ||
               Exp->child->sibling->type == AST_OR ||
               Exp->child->type == AST_NOT) {
        int true_label = newLabelId();
        int false_label = newLabelId();

        InterCodes* code1 = newInterCodes();
        code1->code.kind = IR_ASSIGN;
        code1->code.result.kind = OP_TEMP;
        code1->code.result.u.var_id = place;
        code1->code.arg1.kind = OP_CONSTANT;
        code1->code.arg1.u.value = 0;

        InterCodes* code2 = translate_Cond(Exp, true_label, false_label);

        InterCodes* code3 = genLabelCode(true_label);

        InterCodes* code4 = newInterCodes();
        code4->code.kind = IR_ASSIGN;
        code4->code.result.kind = OP_TEMP;
        code4->code.result.u.var_id = place;
        code4->code.arg1.kind = OP_CONSTANT;
        code4->code.arg1.u.value = 1;

        InterCodes* code5 = genLabelCode(false_label);

        codes = concatInterCodes(5, code1, code2, code3, code4, code5);
    } else if (Exp->child->type == AST_ID && Exp->child->sibling->type == AST_LP) {
        if (Exp->child->sibling->sibling->type == AST_RP) { // ID LP RP
            Symbol func = lookupSymbol(Exp->child->val.c, true);
            if (strcmp(func->name, "read") == 0) {
                codes->code.kind = IR_READ;
                codes->code.result.kind = OP_TEMP;
                codes->code.result.u.var_id = place;
            } else {
                codes->code.kind = IR_CALL;
                codes->code.result.kind = OP_TEMP;
                codes->code.result.u.var_id = place;
                codes->code.arg1.kind = OP_FUNCTION;
                codes->code.arg1.symbol = func;
            }
        } else { // ID LP Args RP
            Symbol func = lookupSymbol(Exp->child->val.c, true);

            ArgNode* arg_list = NULL;
            InterCodes* code1 = translate_Args(Exp->child->sibling->sibling, &arg_list);
            if (strcmp(func->name, "write") == 0) {
                assert(arg_list);
                assert(arg_list->next == NULL);
                InterCodes* code2 = newInterCodes();
                code2->code.kind = IR_WRITE;
                code2->code.result.kind = OP_TEMP;
                code2->code.result.u.var_id = arg_list->var_id;
                codes = concatInterCodes(2, code1, code2);
            } else {
                InterCodes* code2 = NULL;
                for (ArgNode* p = arg_list; p != NULL; p = p->next) {
                    InterCodes* tmp_code = newInterCodes();
                    tmp_code->code.kind = IR_ARG;
                    tmp_code->code.result.kind = OP_TEMP;
                    tmp_code->code.result.u.var_id = p->var_id;
                    if (code2 == NULL) {
                        code2 = tmp_code;
                    } else {
                        code2 = concatInterCodes(2, code2, tmp_code);
                    }
                }

                InterCodes* code3 = newInterCodes();
                code3->code.kind = IR_CALL;
                code3->code.result.kind = OP_TEMP;
                code3->code.result.u.var_id = place;
                code3->code.arg1.kind = OP_FUNCTION;
                code3->code.arg1.symbol = func;

                codes = concatInterCodes(3, code1, code2, code3);
                // TODO: dealloc arg_list
            }
        }
    } else if (Exp->child->type == AST_Exp && Exp->child->sibling->type == AST_LB) { // Exp -> Exp LB Exp RB
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        int t3 = newVariableId();
        InterCodes* code3 = newInterCodes();
        code3->code.kind = IR_MUL;
        code3->code.result.kind = OP_TEMP;
        code3->code.result.u.var_id = t3;
        code3->code.arg1.kind = OP_TEMP;
        code3->code.arg1.u.var_id = t2;
        code3->code.arg2.kind = OP_CONSTANT;
        code3->code.arg2.u.value = getTypeSize(Exp->expType);

        int t4 = newVariableId();
        int t5 = place;
        InterCodes* code4 = newInterCodes();
        InterCodes* code5 = NULL;
        if (Exp->expType->kind == BASIC) {  // derefernce
            t5 = newVariableId();
            code5 = newInterCodes();
            code5->code.kind = IR_DEREF_R;
            code5->code.result.kind = OP_TEMP;
            code5->code.result.u.var_id = place;
            code5->code.arg1.kind = OP_TEMP;
            code5->code.arg1.u.var_id = t5;
        }
        code4->code.kind = IR_ADD;
        code4->code.result.kind = OP_TEMP;
        code4->code.result.u.var_id = t5;
        code4->code.arg1.kind = OP_TEMP;
        code4->code.arg1.u.var_id = t1;
        code4->code.arg2.kind = OP_TEMP;
        code4->code.arg2.u.var_id = t3;

        codes = concatInterCodes(5, code1, code2, code3, code4, code5);
    } else if (Exp->child->type == AST_Exp && Exp->child->sibling->type == AST_DOT) { // Exp -> Exp DOT Exp
        char *name = Exp->child->sibling->sibling->val.c;
        int t1 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);

        assert(Exp->child->expType->kind == STRUCTURE);
        FieldList field = Exp->child->expType->u.structure;
        int offset = 0;
        while (strcmp(field->name, name) != 0) {
            offset += getTypeSize(field->type);
            assert(field->tail != NULL);
            field = field->tail;
        }

        int t2 = place;
        InterCodes* code2 = newInterCodes();
        InterCodes* code3 = NULL;
        if (Exp->expType->kind == BASIC) {  // derefernce
            t2 = newVariableId();
            code3 = newInterCodes();
            code3->code.kind = IR_DEREF_R;
            code3->code.result.kind = OP_TEMP;
            code3->code.result.u.var_id = place;
            code3->code.arg1.kind = OP_TEMP;
            code3->code.arg1.u.var_id = t2;
        }
        code2->code.kind = IR_ADD;
        code2->code.result.kind = OP_TEMP;
        code2->code.result.u.var_id = t2;
        code2->code.arg1.kind = OP_TEMP;
        code2->code.arg1.u.var_id = t1;
        code2->code.arg2.kind = OP_CONSTANT;
        code2->code.arg2.u.value = offset;

        codes = concatInterCodes(3, code1, code2, code3);
    } else {
        ASTwalk(Exp, 0);
        assert(0);
    }

    return codes;
}

InterCodes* translate_Stmt(ASTNode *Stmt) {
    assert(Stmt);
    assert(Stmt->type == AST_Stmt);

    InterCodes* codes = NULL;

    if (Stmt->child->type == AST_CompSt) { // Stmt -> CompSt
        codes = translate_CompSt(Stmt->child);
    } else if (Stmt->child->type == AST_Exp) { // Stmt -> Exp SEMI
        codes = translate_Exp(Stmt->child, VAR_NULL);
    } else if (Stmt->child->type == AST_RETURN) { // Stmt -> RETURN Exp SEMI
        int t1 = newVariableId();
        InterCodes* code1 = translate_Exp(Stmt->child->sibling, t1);

        InterCodes* code2 = newInterCodes();
        code2->code.kind = IR_RETURN;
        code2->code.result.kind = OP_TEMP;
        code2->code.result.u.var_id = t1;

        codes = concatInterCodes(2, code1, code2);
    } else if (Stmt->child->type == AST_WHILE) { // Stmt -> WHILE LP Exp RP Stmt
        int label1 = newLabelId();
        int label2 = LABEL_FALL;    
        int label3 = newLabelId();

        InterCodes* code1 = translate_Cond(Stmt->child->sibling->sibling, label2, label3);
        InterCodes* code2 = translate_Stmt(Stmt->child->sibling->sibling->sibling->sibling);

        codes = concatInterCodes(5, genLabelCode(label1), code1,
                                    code2, genGotoCode(label1), genLabelCode(label3));
    } else if (Stmt->child->type == AST_IF) {
        if (Stmt->child->sibling->sibling->sibling->sibling->sibling == NULL) { // Stmt -> IF LP Exp RP Stmt
            int label1 = LABEL_FALL;
            int label2 = newLabelId();
            InterCodes* code1 = translate_Cond(Stmt->child->sibling->sibling, label1, label2);
            InterCodes* code2 = translate_Stmt(Stmt->child->sibling->sibling->sibling->sibling);
            codes = concatInterCodes(3, code1, code2, genLabelCode(label2));
        } else { // IF LP Exp RP Stmt ELSE Stmt
            int label1 = LABEL_FALL;
            int label2 = newLabelId();
            int label3 = newLabelId();
            InterCodes* code1 = translate_Cond(Stmt->child->sibling->sibling, label1, label2);
            InterCodes* code2 = translate_Stmt(Stmt->child->sibling->sibling->sibling->sibling);
            InterCodes* code3 = translate_Stmt(Stmt->child->sibling->sibling->sibling->sibling->sibling->sibling);
            codes = concatInterCodes(6, code1, code2, genGotoCode(label3), genLabelCode(label2),
                                        code3, genLabelCode(label3));
        }
    } else {
        assert(0);
    }

    return codes;
}

InterCodes* translate_StmtList(ASTNode *StmtList) {
    assert(StmtList);
    assert(StmtList->type == AST_StmtList);

    InterCodes* codes = NULL;

    if (StmtList->child != NULL) {
        codes = translate_Stmt(StmtList->child);
        InterCodes* code2 = translate_StmtList(StmtList->child->sibling);
        if (code2 != NULL) {
            codes = concatInterCodes(2, codes, code2);
        }
    }

    return codes;
}

InterCodes* translate_CompSt(ASTNode *CompSt) {
    assert(CompSt);
    assert(CompSt->type == AST_CompSt);

    InterCodes* code1 = translate_DefList(CompSt->child->sibling);
    InterCodes* code2 = translate_StmtList(CompSt->child->sibling->sibling);

    return concatInterCodes(2, code1, code2);
}

InterCodes* translate_DefList(ASTNode *DefList) {
    assert(DefList);
    assert(DefList->type == AST_DefList);

    InterCodes* codes = NULL;

    if (DefList->child != NULL) {
        codes = translate_Def(DefList->child);
        InterCodes* code2 = translate_DefList(DefList->child->sibling);
        if (code2 != NULL) {
            codes = concatInterCodes(2, codes, code2);
        }
    }

    return codes;
}

InterCodes* translate_Def(ASTNode *Def) {
    assert(Def);
    assert(Def->type == AST_Def);

    return translate_DecList(Def->child->sibling);
}

InterCodes* translate_DecList(ASTNode *DecList) {
    assert(DecList);
    assert(DecList->type == AST_DecList);

    InterCodes* codes = NULL;

    codes = translate_Dec(DecList->child);

    if (DecList->child->sibling != NULL) {
        InterCodes* code2 = translate_DecList(DecList->child->sibling->sibling);
        if (code2 != NULL) {
            codes = concatInterCodes(2, codes, code2);
        }
    }

    return codes;
}

InterCodes* translate_Dec(ASTNode *Dec) {
    assert(Dec);
    assert(Dec->type == AST_Dec);

    InterCodes* codes = NULL;

    if (Dec->child->sibling == NULL) { // Dec -> VarDec
        codes = translate_VarDec(Dec->child);
    } else if (Dec->child->sibling != NULL) { // Dec -> VarDec ASSIGNOP Exp
        int t1 = newVariableId();
        assert(Dec->child->child->type == AST_ID);
        Symbol variable = lookupSymbol(Dec->child->child->val.c, true);
        InterCodes* code1 = translate_Exp(Dec->child->sibling->sibling, t1);

        InterCodes* code2 = newInterCodes();
        code2->code.kind = IR_ASSIGN;
        code2->code.result.kind = OP_VARIABLE;
        code2->code.result.symbol = variable;
        code2->code.arg1.kind = OP_TEMP;
        code2->code.arg1.u.var_id = t1;

        codes = concatInterCodes(2, code1, code2);
    } else {
        assert(0);
    }

    return codes;
}

InterCodes* translate_Cond(ASTNode *Exp, int label_true, int label_false) {
    assert(Exp);
    assert(Exp->type == AST_Exp);

    InterCodes* codes = NULL;
    if (Exp->child->type == AST_NOT) { // Exp -> NOT Exp
        codes = translate_Cond(Exp, label_false, label_true);
    } else if (Exp->child->sibling->type == AST_RELOP) { // Exp -> Exp RELOP Exp
        int t1 = newVariableId();
        int t2 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp->child, t1);
        InterCodes* code2 = translate_Exp(Exp->child->sibling->sibling, t2);

        if (label_true != LABEL_FALL && label_false != LABEL_FALL) {
            InterCodes* code3 = newInterCodes();
            code3->code.kind = IR_RELOP;
            code3->code.relop = get_relop(Exp->child->sibling);
            code3->code.result.kind = OP_LABEL;
            code3->code.result.u.label_id = label_true;
            code3->code.arg1.kind = OP_TEMP;
            code3->code.arg1.u.var_id = t1;
            code3->code.arg2.kind = OP_TEMP;
            code3->code.arg2.u.var_id = t2;

            codes = concatInterCodes(4, code1, code2, code3, genGotoCode(label_false));
        } else if (label_true != LABEL_FALL) {
            InterCodes* code3 = newInterCodes();
            code3->code.kind = IR_RELOP;
            code3->code.relop = get_relop(Exp->child->sibling);
            code3->code.result.kind = OP_LABEL;
            code3->code.result.u.label_id = label_true;
            code3->code.arg1.kind = OP_TEMP;
            code3->code.arg1.u.var_id = t1;
            code3->code.arg2.kind = OP_TEMP;
            code3->code.arg2.u.var_id = t2;

            codes = concatInterCodes(3, code1, code2, code3);
        } else if (label_false != LABEL_FALL) {
            InterCodes* code3 = newInterCodes();
            code3->code.kind = IR_RELOP;
            code3->code.relop = get_reverse_relop(get_relop(Exp->child->sibling));
            code3->code.result.kind = OP_LABEL;
            code3->code.result.u.label_id = label_false;
            code3->code.arg1.kind = OP_TEMP;
            code3->code.arg1.u.var_id = t1;
            code3->code.arg2.kind = OP_TEMP;
            code3->code.arg2.u.var_id = t2;

            codes = concatInterCodes(3, code1, code2, code3);
        } else {
            codes = concatInterCodes(2, code1, code2);
        }
    } else if (Exp->child->sibling->type == AST_AND) { // Exp AND Exp
        int label_Exp1_false;
        if (label_false != LABEL_FALL) {
            label_Exp1_false = label_false;
        } else {
            label_Exp1_false = newLabelId();
        }

        InterCodes* code1 = translate_Cond(Exp->child, LABEL_FALL, label_Exp1_false);
        InterCodes* code2 = translate_Cond(Exp->child->sibling->sibling, label_true, label_false);

        if (label_false != LABEL_FALL) {
            codes = concatInterCodes(2, code1, code2);
        } else {
            codes = concatInterCodes(3, code1, code2, genLabelCode(label_Exp1_false));
        }
    } else if (Exp->child->sibling->type == AST_OR) { // Exp OR Exp
        int label_Exp1_true;
        if (label_true != LABEL_FALL) {
            label_Exp1_true = label_true;
        } else {
            label_Exp1_true = newLabelId();
        }

        InterCodes* code1 = translate_Cond(Exp->child, label_Exp1_true, LABEL_FALL);
        InterCodes* code2 = translate_Cond(Exp->child->sibling->sibling, label_true, label_false);

        if (label_true != LABEL_FALL) {
            codes = concatInterCodes(2, code1, code2);
        } else {
            codes = concatInterCodes(3, code1, code2, genLabelCode(label_Exp1_true));
        }
    } else {
        int t1 = newVariableId();
        InterCodes* code1 = translate_Exp(Exp, t1);

        InterCodes* code2 = newInterCodes();
        code2->code.kind = IR_RELOP;
        code2->code.relop = RELOP_NE;
        code2->code.result.kind = OP_LABEL;
        code2->code.result.u.label_id = label_true;
        code2->code.arg1.kind = OP_TEMP;
        code2->code.arg1.u.var_id = t1;
        code2->code.arg2.kind = OP_CONSTANT;
        code2->code.arg2.u.var_id = 0;

        codes = concatInterCodes(3, code1, code2, genGotoCode(label_false));
    }
    assert(codes);
    return codes;
}

InterCodes* translate_Args(ASTNode *Args, ArgNode** arg_list) {
    assert(Args);
    assert(Args->type == AST_Args);

    int t1 = newVariableId();
    InterCodes* codes = translate_Exp(Args->child, t1);
    ArgNode* arg = newArgNode(t1);
    // append arg before arg_list
    if (*arg_list == NULL) {
        *arg_list = arg;
    } else {
        ArgNode* p = *arg_list;
        *arg_list = arg;
        (*arg_list)->next = p;
    }

    if (Args->child->sibling == NULL) { // Args -> Exp
        return codes;
    } else { // Args -> Exp COMMA Args
        InterCodes* code2 = translate_Args(Args->child->sibling->sibling, arg_list);
        return concatInterCodes(2, codes, code2);
    }
}

InterCodes* translate_Program(ASTNode *Program) {
    assert(Program);
    assert(Program->type == AST_Program);
    return translate_ExtDefList(Program->child);
}

InterCodes* translate_ExtDefList(ASTNode *ExtDefList) {
    assert(ExtDefList);
    assert(ExtDefList->type == AST_ExtDefList);

    InterCodes* codes = NULL;

    if (ExtDefList->child != NULL) {
        codes = translate_ExtDef(ExtDefList->child);
        InterCodes* code2 = translate_ExtDefList(ExtDefList->child->sibling);
        if (code2 != NULL) {
            codes = concatInterCodes(2, codes, code2);
        }
    }

    return codes;
}

InterCodes* translate_ExtDef(ASTNode *ExtDef) {
    assert(ExtDef);
    assert(ExtDef->type == AST_ExtDef);

    InterCodes* codes = NULL;

    if (ExtDef->child->sibling->type == AST_FunDec) {
        codes = translate_FunDec(ExtDef->child->sibling);
        if (ExtDef->child->sibling->sibling->type == AST_CompSt) {
            InterCodes* code = translate_CompSt(ExtDef->child->sibling->sibling);
            codes = concatInterCodes(2, codes, code);
        } else {
            assert(0);
        }
    } else if (ExtDef->child->sibling->type == AST_ExtDecList) {
        codes = translate_ExtDecList(ExtDef->child->sibling);
    }

    return codes;
}

InterCodes* translate_ExtDecList(ASTNode *ExtDecList) {
    assert(ExtDecList);
    assert(ExtDecList->type == AST_ExtDecList);

    InterCodes* codes = NULL;

    codes = translate_VarDec(ExtDecList->child);

    if (ExtDecList->child->sibling != NULL) {
        InterCodes* code2 = translate_ExtDecList(ExtDecList->child->sibling->sibling);
        if (code2 != NULL) {
            codes = concatInterCodes(2, codes, code2);
        }
    }

    return codes;
}

int getTypeSize(Type type) {
    if (type->kind == ARRAY) {
        return type->u.array.size * getTypeSize(type->u.array.elem);
    } else if (type->kind == STRUCTURE) {
        int size = 0;
        for (FieldList p = type->u.structure; p != NULL; p = p->tail) {
            size += getTypeSize(p->type);
        }
        return size;
    } else {
        return 4;
    }
}

InterCodes* translate_VarDec(ASTNode *VarDec) {
    assert(VarDec);
    assert(VarDec->type == AST_VarDec);

    InterCodes* codes = NULL;

    if (VarDec->child->type == AST_ID) {
        Symbol variable = lookupSymbol(VarDec->child->val.c, true);
        if (variable->kind == VAR_DEF) {
            int size = getTypeSize(variable->u.type);
            if (size > 4) {
                int t1 = newVariableId();
                InterCodes* code1 = newInterCodes();
                code1->code.kind = IR_DEC;
                code1->code.result.kind = OP_TEMP;
                code1->code.result.u.var_id = t1;
                code1->code.size = size;

                InterCodes* code2 = newInterCodes();
                code2->code.kind = IR_ADDR;
                code2->code.result.kind = OP_VARIABLE;
                code2->code.result.symbol = variable;
                code2->code.arg1.kind = OP_TEMP;
                code2->code.arg1.u.var_id = t1;

                codes = concatInterCodes(2, code1, code2);
            }
        } else {
            assert(0);
        }
    } else if (VarDec->child->type == AST_VarDec) {
        codes = translate_VarDec(VarDec->child);
    } else {
        assert(0);
    }

    return codes;
}

InterCodes* translate_FunDec(ASTNode *FunDec) {
    assert(FunDec);
    assert(FunDec->type == AST_FunDec);

    InterCodes* codes = newInterCodes();
    codes->code.kind = IR_FUNC;
    codes->code.result.kind = OP_FUNCTION;
    codes->code.result.symbol = lookupSymbol(FunDec->child->val.c, true);

    if (FunDec->child->sibling->sibling->type == AST_VarList) {
        codes = concatInterCodes(2, codes, translate_VarList(FunDec->child->sibling->sibling));
    }

    return codes;
}

InterCodes* translate_VarList(ASTNode *VarList) {
    assert(VarList);
    assert(VarList->type == AST_VarList);

    InterCodes* codes = translate_ParamDec(VarList->child);

    if (VarList->child->sibling != NULL) {
        codes = concatInterCodes(2, codes, translate_VarList(VarList->child->sibling->sibling));
    }

    return codes;
}

InterCodes* translate_ParamDec(ASTNode *ParamDec) {
    assert(ParamDec);
    assert(ParamDec->type == AST_ParamDec);

    ASTNode *varDec = ParamDec->child->sibling;
    while (varDec->child->type != AST_ID) {
        varDec = varDec->child;
    }
    char *name = varDec->child->val.c;
    Symbol variable = lookupSymbol(name, true);
    assert(variable);

    InterCodes* codes = newInterCodes();
    codes->code.kind = IR_PARAM;
    codes->code.result.kind = OP_VARIABLE;
    codes->code.result.symbol = variable;

    return codes;
}

static void printOperand(Operand op) {
    if (op.kind == OP_TEMP) {
        printf("t%d", op.u.var_id);
    } else if (op.kind == OP_VARIABLE) {
        printf("v_%s", op.symbol->name);
    } else if (op.kind == OP_FUNCTION) {
        printf("%s", op.symbol->name);
    } else if (op.kind == OP_CONSTANT) {
        printf("#%d", op.u.value);
    } else if (op.kind == OP_ADDR) {
        printf("&%s", op.symbol->name);
    } else if (op.kind == OP_LABEL) {
        printf("label%d", op.u.label_id);
    } else {
        assert(0);
    }
}

void generate_ir(ASTNode* Program) {
    InterCodes* codes = translate_Program(Program);

    for (InterCodes* p = codes; p != NULL; p = p->next) {
        switch(p->code.kind) {
            case IR_LABEL: {
                assert(p->code.result.kind == OP_LABEL);
                printf("LABEL label%d :\n", p->code.result.u.label_id);
                break;
            }
            case IR_FUNC: {
                assert(p->code.result.kind == OP_FUNCTION);
                printf("FUNCTION %s :\n", p->code.result.symbol->name);
                break;
            }
            case IR_ASSIGN: {
                if (p->code.result.kind == OP_TEMP && p->code.result.u.var_id == VAR_NULL) {
                    break;
                }
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf("\n");
                break;
            }
            case IR_ADD: {
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf(" + ");
                printOperand(p->code.arg2);
                printf("\n");
                break;
            }
            case IR_SUB: {
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf(" - ");
                printOperand(p->code.arg2);
                printf("\n");
                break;
            }
            case IR_MUL: {
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf(" * ");
                printOperand(p->code.arg2);
                printf("\n");
                break;
            }
            case IR_DIV: {
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf(" / ");
                printOperand(p->code.arg2);
                printf("\n");
                break;
            }
            case IR_GOTO: {
                printf("GOTO ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_RELOP: {
                printf("IF ");
                printOperand(p->code.arg1);
                switch (p->code.relop) {
                    case RELOP_LT: printf(" < "); break;
                    case RELOP_LE: printf(" <= "); break;
                    case RELOP_EQ: printf(" == "); break;
                    case RELOP_GT: printf(" > "); break;
                    case RELOP_GE: printf(" >= "); break;
                    case RELOP_NE: printf(" != "); break;
                    default: assert(0);
                }
                printOperand(p->code.arg2);
                printf(" GOTO ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_DEC: {
                printf("DEC ");
                printOperand(p->code.result);
                printf(" %d\n", p->code.size);
                break;
            }
            case IR_RETURN: {
                printf("RETURN ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_ARG: {
                printf("ARG ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_CALL: {
                printOperand(p->code.result);
                printf(" := CALL ");
                printOperand(p->code.arg1);
                printf("\n");
                break;
            }
            case IR_PARAM: {
                printf("PARAM ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_READ: {
                printf("READ ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_WRITE: {
                printf("WRITE ");
                printOperand(p->code.result);
                printf("\n");
                break;
            }
            case IR_DEREF_R: {
                printOperand(p->code.result);
                printf(" := *");
                printOperand(p->code.arg1);
                printf("\n");
                break;
            }
            case IR_DEREF_L: {
                printf("*");
                printOperand(p->code.result);
                printf(" := ");
                printOperand(p->code.arg1);
                printf("\n");
                break;
            }
            case IR_ADDR: {
                printOperand(p->code.result);
                printf(" := &");
                printOperand(p->code.arg1);
                printf("\n");
                break;
            }
            default: assert(0);
        }
    }
}
