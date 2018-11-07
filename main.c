#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#define UNUSED(x) (void)(x)

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char *
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

typedef enum
{
    LVAL_ERR,
    LVAL_INT,
    LVAL_DOUBLE,
    LVAL_SYM,
    LVAL_SEXPR
} lval_type;

typedef struct lval
{
    lval_type type;
    long inum;
    double dnum;
    char *err;
    char* sym;
    int count; // For the length of lval list
    struct lval **cell;
} lval;

lval *
lval_int(long x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_INT;
    v->inum = x;
    return v;
}

lval *
lval_double(double x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DOUBLE;
    v->dnum = x;
    return v;
}

lval *
lval_err(char *message)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(message) + 1);
    strcpy(v->err, message);
    return v;
}

lval *
lval_sym(char *sym)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

lval *
lval_sexpr()
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void
lval_del(lval *v)
{
    switch (v->type)
    {
    case LVAL_DOUBLE: break;
    case LVAL_INT: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
            lval_del(v->cell[i]);

        free(v->cell);
        break;
    }
    free(v);
}
lval *
lval_read_num(mpc_ast_t *t)
{
    errno = 0;

    if (strstr(t->contents, "."))
    {
        double x = strtod(t->contents, NULL);
        return errno != ERANGE ? lval_double(x) : lval_err("Invalid floating point number");
    }
    else
    {
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_int(x) : lval_err("Invalid integer");
    }
}

lval *
lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *
lval_read(mpc_ast_t *t)
{
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    lval *x = NULL;

    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strstr(t->tag, "sexpr"))  x = lval_sexpr();

    for (int i = 0; i< t->children_num; i++)
    {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->tag, "regex")  == 0) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void
lval_print(lval *v);

void
lval_expr_print(lval *v, char open, char close)
{
    putchar(open);

    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) putchar(' ');
    }
    putchar(close);
}

void
lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_INT    : printf("%li", v->inum); break;
    case LVAL_DOUBLE : printf("%lf", v->dnum); break;
    case LVAL_ERR    : printf("%s", v->err); break;
    case LVAL_SYM    : printf("%s", v->sym); break;
    case LVAL_SEXPR  : lval_expr_print(v, '(', ')'); break;
    }
}

void
lval_println(lval *v) { lval_print(v); putchar('\n'); }

/* lval */
/* eval_op(lval x, char* op, lval y) */
/* { */
/*     if (x.type == LVAL_ERR) return x; */
/*     if (y.type == LVAL_ERR) return y; */

/*     if ((x.type == LVAL_INT) && (y.type == LVAL_INT)) */
/*     { */
/*         if (strcmp(op, "+") == 0) return lval_int(x.inum + y.inum); */
/*         if (strcmp(op, "-") == 0) return lval_int(x.inum - y.inum); */
/*         if (strcmp(op, "*") == 0) return lval_int(x.inum * y.inum); */
/*         if (strcmp(op, "%") == 0) return lval_int(x.inum % y.inum); */
/*         if (strcmp(op, "^") == 0) return lval_int(pow(x.inum, y.inum)); */
/*         if (strcmp(op, "/") == 0) */
/*         { */
/*             return y.inum == 0 */
/*                 ? lval_err(DIV_ZERO) */
/*                 : lval_double((double)x.inum / (double)y.inum); */
/*         } */
/*     } */

/*     if ((x.type == LVAL_DOUBLE) && (y.type == LVAL_INT)) */
/*     { */
/*         if (strcmp(op, "+") == 0) return lval_double(x.dnum + y.inum); */
/*         if (strcmp(op, "-") == 0) return lval_double(x.dnum - y.inum); */
/*         if (strcmp(op, "*") == 0) return lval_double(x.dnum * y.inum); */
/*         if (strcmp(op, "/") == 0) */
/*         { */
/*             return y.inum == 0 */
/*                 ? lval_err(DIV_ZERO) */
/*                 : lval_double(x.dnum / (double)y.inum); */
/*         } */
/*     } */

/*     if ((x.type == LVAL_INT) && (y.type == LVAL_DOUBLE)) */
/*     { */
/*         if (strcmp(op, "+") == 0) return lval_double(x.inum + y.dnum); */
/*         if (strcmp(op, "-") == 0) return lval_double(x.inum - y.dnum); */
/*         if (strcmp(op, "*") == 0) return lval_double(x.inum * y.dnum); */
/*         if (strcmp(op, "/") == 0) */
/*         { */
/*             return y.dnum == 0 */
/*                 ? lval_err(DIV_ZERO) */
/*                 : lval_double((double)x.inum / y.dnum); */
/*         } */
/*     } */

/*     if ((x.type == LVAL_DOUBLE) && (y.type == LVAL_DOUBLE)) */
/*     { */
/*         if (strcmp(op, "+") == 0) return lval_double(x.dnum + y.dnum); */
/*         if (strcmp(op, "-") == 0) return lval_double(x.dnum - y.dnum); */
/*         if (strcmp(op, "*") == 0) return lval_double(x.dnum * y.dnum); */
/*         if (strcmp(op, "/") == 0) */
/*         { */
/*             return y.dnum == 0 */
/*                 ? lval_err(DIV_ZERO) */
/*                 : lval_double(x.dnum / y.dnum); */
/*         } */
/*     } */

/*     return lval_err(INVALID_OP); */
/* } */

/* lval */
/* eval(mpc_ast_t* t) */
/* { */
/*     if (strstr(t->tag, "number")) */
/*     { */
/*         errno = 0; */

/*         if (strstr(t->contents, ".")) */
/*         { */
/*             double x = strtod(t->contents, NULL); */
/*             return errno != ERANGE ? lval_double(x) : lval_err(INVALID_NUM); */
/*         } */
/*         else */
/*         { */
/*             long x = strtol(t->contents, NULL, 10); */
/*             return errno != ERANGE ? lval_int(x) : lval_err(INVALID_NUM); */
/*         } */
/*     } */

/*     char* op = t->children[1]->contents; */
/*     lval x = eval(t->children[2]); */
/*     int i = 3; */

/*     while (strstr(t->children[i]->tag, "expr")) */
/*     { */
/*         x = eval_op(x, op, eval(t->children[i])); */
/*         i++; */
/*     } */

/*     return x; */
/* } */

int
main(void)
{
    puts("Lispy Version 0.0.1\n");
    puts("Press Ctrl+c to Exit\n");

    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr  = mpc_new("sexpr");
    mpc_parser_t *Expr   = mpc_new("expr");
    mpc_parser_t *Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
              "\
              number : /-?[0-9]+(\\.?[0-9]*)/ ;                       \
              symbol : '+' | '-' | '*' | '/' | '%' | '^' ;            \
              sexpr  : '(' <expr>* ')' ;                              \
              expr   : <number> | <symbol> | <sexpr> ;                \
              lispy  : /^/ <expr>+ /$/ ;                              \
              ",
              Number,
              Symbol,
              Sexpr,
              Expr,
              Lispy
    );

    while(true)
    {
        char *input = readline("> ");
        add_history(input);

        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Lispy, &r))
        {
            /* lval result = eval(r.output); */
            /* lval_println(result); */
            /* mpc_ast_delete(r.output); */
            lval *x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
}
