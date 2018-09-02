#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#define UNUSED(x) (void)(x)

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char* 
readline(char* prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) +1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) -1] = '\0';
    return cpy;
}

void
add_history(char* unused) { UNUSED(unused); }

#else

#include <readline/readline.h>
#include <readline/history.h>

#endif

enum
{
    LVAL_INT,
    LVAL_DOUBLE,
    LVAL_ERR
};

enum
{
    DIV_ZERO,
    INVALID_OP,
    INVALID_NUM
};

typedef struct {
    int type;
    long inum;
    double dnum;
    int err;
} lval;

lval 
lval_int(long x)
{
    lval v;
    v.type = LVAL_INT;
    v.inum = x;
    return v;
}

lval 
lval_double(double x)
{
    lval v;
    v.type = LVAL_DOUBLE;
    v.dnum = x;
    return v;
}

lval 
lval_err(int x)
{
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void 
lval_print(lval v)
{
    switch (v.type)
    {
    case LVAL_INT: printf("%li", v.inum); break;
    case LVAL_DOUBLE: printf("%lf", v.dnum); break;
    case LVAL_ERR:
        if (v.err == DIV_ZERO) printf("Error: Division By Zero.");
        if (v.err == INVALID_OP) printf("Error: Invalid Operator.");
        if (v.err == INVALID_NUM) printf("Error: Invalid Number.");
    break;
    }
}

void 
lval_println(lval v) { lval_print(v); putchar('\n'); }

lval 
eval_op(lval x, char* op, lval y)
{
    if (x.type == LVAL_ERR) return x;
    if (y.type == LVAL_ERR) return y;

    if ((x.type == LVAL_INT) && (y.type == LVAL_INT))
    {
        if (strcmp(op, "+") == 0) return lval_int(x.inum + y.inum);
        if (strcmp(op, "-") == 0) return lval_int(x.inum - y.inum);
        if (strcmp(op, "*") == 0) return lval_int(x.inum * y.inum);
        if (strcmp(op, "%") == 0) return lval_int(x.inum % y.inum);
        if (strcmp(op, "^") == 0) return lval_int(pow(x.inum, y.inum));
        if (strcmp(op, "/") == 0)
        {
            return y.inum == 0
                ? lval_err(DIV_ZERO)
                : lval_double((double)x.inum / (double)y.inum);
        }
    }

    if ((x.type == LVAL_DOUBLE) && (y.type == LVAL_INT))
    {
        if (strcmp(op, "+") == 0) return lval_double(x.dnum + y.inum);
        if (strcmp(op, "-") == 0) return lval_double(x.dnum - y.inum);
        if (strcmp(op, "*") == 0) return lval_double(x.dnum * y.inum);
        if (strcmp(op, "/") == 0)
        {
            return y.inum == 0
                ? lval_err(DIV_ZERO)
                : lval_double(x.dnum / (double)y.inum);
        }
    }

    if ((x.type == LVAL_INT) && (y.type == LVAL_DOUBLE))
    {
        if (strcmp(op, "+") == 0) return lval_double(x.inum + y.dnum);
        if (strcmp(op, "-") == 0) return lval_double(x.inum - y.dnum);
        if (strcmp(op, "*") == 0) return lval_double(x.inum * y.dnum);
        if (strcmp(op, "/") == 0)
        {
            return y.dnum == 0
                ? lval_err(DIV_ZERO)
                : lval_double((double)x.inum / y.dnum);
        }
    }

    if ((x.type == LVAL_DOUBLE) && (y.type == LVAL_DOUBLE))
    {
        if (strcmp(op, "+") == 0) return lval_double(x.dnum + y.dnum);
        if (strcmp(op, "-") == 0) return lval_double(x.dnum - y.dnum);
        if (strcmp(op, "*") == 0) return lval_double(x.dnum * y.dnum);
        if (strcmp(op, "/") == 0)
        {
            return y.dnum == 0
                ? lval_err(DIV_ZERO)
                : lval_double(x.dnum / y.dnum);
        }
    }

    return lval_err(INVALID_OP);
}

lval 
eval(mpc_ast_t* t)
{
    if (strstr(t->tag, "number"))
    {
        errno = 0;

        if (strstr(t->contents, "."))
        {
            double x = strtod(t->contents, NULL);
            return errno != ERANGE ? lval_double(x) : lval_err(INVALID_NUM);
        }   
        else
        {
            long x = strtol(t->contents, NULL, 10);
            return errno != ERANGE ? lval_int(x) : lval_err(INVALID_NUM);
        }
    }

    char* op = t->children[1]->contents;
    lval x = eval(t->children[2]);
    int i = 3;

    while (strstr(t->children[i]->tag, "expr"))
    {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int 
main(void)
{ 
    puts("Lispy Version 0.0.1\n");
    puts("Press Ctrl+c to Exit\n");

    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lisp     = mpc_new("lisp");

    mpca_lang(MPCA_LANG_DEFAULT,
              "\
              number   : /-?[0-9]+(\\.?[0-9]*)/ ;                        \
              operator : '+' | '-' | '*' | '/' | '%' | '^' ;             \
              expr     : <number> | '(' <operator> <expr>+ ')' ;         \
              lisp     : /^/ <operator> <expr>+ /$/ ;                    \
              ",
              Number,
              Operator,
              Expr,
              Lisp
    );

    while(true)
    {
        char* input = readline("> ");
        add_history(input);

        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Lisp, &r))
        {
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lisp);
}

