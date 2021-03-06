#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#define UNUSED (x) (void) (x)
#define FOREVER 1

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char *
readline (char* prompt)
{
    fputs (prompt, stdout);
    fgets (buffer, 2048, stdin);
    char* cpy = malloc (strlen (buffer) +1);
    strcpy (cpy, buffer);
    cpy[strlen (cpy) -1] = '\0';
    return cpy;
}

void
add_history (char* unused) { UNUSED (unused); }

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
lval_int (long x)
{
    lval *v = malloc (sizeof (lval));
    v->type = LVAL_INT;
    v->inum = x;
    return v;
}

lval *
lval_double (double x)
{
    lval *v = malloc (sizeof (lval));
    v->type = LVAL_DOUBLE;
    v->dnum = x;
    return v;
}

lval *
lval_err (char *message)
{
    lval *v = malloc (sizeof (lval));
    v->type = LVAL_ERR;
    v->err = malloc (strlen (message) + 1);
    strcpy (v->err, message);
    return v;
}

lval *
lval_sym (char *sym)
{
    lval *v = malloc (sizeof (lval));
    v->type = LVAL_SYM;
    v->sym = malloc (strlen (sym) + 1);
    strcpy (v->sym, sym);
    return v;
}

lval *
lval_sexpr ()
{
    lval *v = malloc (sizeof (lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void
lval_del (lval *v)
{
    switch (v->type)
    {
    case LVAL_DOUBLE: break;
    case LVAL_INT: break;
    case LVAL_ERR: free (v->err); break;
    case LVAL_SYM: free (v->sym); break;
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
            lval_del (v->cell[i]);

        free (v->cell);
        break;
    }
    free (v);
}
lval *
lval_read_num (mpc_ast_t *t)
{
    errno = 0;

    if (strstr (t->contents, "."))
    {
        double x = strtod (t->contents, NULL);
        return errno != ERANGE ? lval_double (x) : lval_err ("Invalid floating point number");
    }
    else
    {
        long x = strtol (t->contents, NULL, 10);
        return errno != ERANGE ? lval_int (x) : lval_err ("Invalid integer");
    }
}

lval *
lval_add (lval *v, lval *x)
{
    v->count++;
    v->cell = realloc (v->cell, sizeof (lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *
lval_read (mpc_ast_t *t)
{
    if (strstr (t->tag, "number")) return lval_read_num (t);
    if (strstr (t->tag, "symbol")) return lval_sym (t->contents);

    lval *x = NULL;

    if (strcmp (t->tag, ">") == 0) x = lval_sexpr ();
    if (strstr (t->tag, "sexpr"))  x = lval_sexpr ();

    for (int i = 0; i< t->children_num; i++)
    {
        if (strcmp (t->children[i]->contents, " (") == 0) continue;
        if (strcmp (t->children[i]->contents, ")") == 0) continue;
        if (strcmp (t->children[i]->tag, "regex")  == 0) continue;
        x = lval_add (x, lval_read (t->children[i]));
    }

    return x;
}

void
lval_print (lval *v);

void
lval_expr_print (lval *v, char open, char close)
{
    putchar (open);

    for (int i = 0; i < v->count; i++)
    {
        lval_print (v->cell[i]);

        if (i != (v->count - 1)) putchar (' ');
    }
    putchar (close);
}

void
lval_print (lval *v)
{
    switch (v->type)
    {
    case LVAL_INT    : printf ("%li", v->inum); break;
    case LVAL_DOUBLE : printf ("%lf", v->dnum); break;
    case LVAL_ERR    : printf ("%s", v->err); break;
    case LVAL_SYM    : printf ("%s", v->sym); break;
    case LVAL_SEXPR  : lval_expr_print (v, '(', ')'); break;
    }
}

void
lval_println (lval *v) { lval_print (v); putchar ('\n'); }
lval *

lval_pop (lval *v, int i)
{
    /* Find the item at "i" */
    lval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove (&v->cell[i],
             &v->cell[i + 1],
             sizeof (lval*) * (v->count -i -1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof (lval*) * v->count);
    return x;
}

lval *
lval_take (lval *v, int i)
{
    lval *x = lval_pop (v, i);
    lval_del (v);
    return x;
}

lval *
builtin_op (lval *a, char *op)
{

    /* Ensure all the arguments are numbers */
    for (int i = 0; i < a->count; i++)
        if (a->cell[i]->type != LVAL_INT
            && a->cell[i]->type != LVAL_DOUBLE)
        {
            lval_del(a);
            return lval_err("Cannnot operate on non-number!");
        }

    /* Pop the first element */
    lval *x = lval_pop (a, 0);

    /* If no arguments and sub then preform unary negation */
    if ((strcmp (op, "-") == 0) && a->count == 0)
    {
        if (x->type == LVAL_INT) x->inum = -x->inum;
        else x->dnum = -x->dnum;
    }

    /* White there are still elements remaining */
    while (a->count > 0)
    {
        /* Pop the next element */
        lval *y = lval_pop (a, 0);

        if ((x->type == LVAL_INT) && (y->type == LVAL_INT))
        {
            if (strcmp (op, "+") == 0) (x->inum += y->inum);
            if (strcmp (op, "-") == 0) (x->inum -= y->inum);
            if (strcmp (op, "*") == 0) (x->inum *= y->inum);
            if (strcmp (op, "%") == 0) (x->inum %= y->inum);
            if (strcmp (op, "^") == 0) (x->inum = pow(x->inum, y->inum));
            if (strcmp (op, "/") == 0)
            {
                if (y->inum == 0)
                {
                    lval_del (x);
                    lval_del (y);
                    x = lval_err ("Division By Zero");
                }
            }
        }

        if ((x->type == LVAL_DOUBLE) && (y->type == LVAL_INT))
        {
            if (strcmp (op, "+") == 0) (x->dnum += y->inum);
            if (strcmp (op, "-") == 0) (x->dnum -= y->inum);
            if (strcmp (op, "*") == 0) (x->dnum *= y->inum);
            if (strcmp (op, "/") == 0)
            {
                if (y->inum == 0)
                {
                    lval_del (x);
                    lval_del (y);
                    x = lval_err ("Division By Zero");
                }
            }
        }

        if ((x->type == LVAL_INT) && (y->type == LVAL_DOUBLE))
        {
            if (strcmp (op, "+") == 0) (x->inum += y->dnum);
            if (strcmp (op, "-") == 0) (x->inum -= y->dnum);
            if (strcmp (op, "*") == 0) (x->inum *= y->dnum);
            if (strcmp (op, "/") == 0)
            {
                if (y->dnum == 0)
                {
                    lval_del (x);
                    lval_del (y);
                    x = lval_err ("Division By Zero");
                }
            }
        }

        if ((x->type == LVAL_DOUBLE) && (y->type == LVAL_DOUBLE))
        {
            if (strcmp (op, "+") == 0) (x->dnum += y->dnum);
            if (strcmp (op, "-") == 0) (x->dnum -= y->dnum);
            if (strcmp (op, "*") == 0) (x->dnum *= y->dnum);
            if (strcmp (op, "/") == 0)
            {
                if (y->dnum == 0)
                {
                    lval_del (x);
                    lval_del (y);
                    x = lval_err ("Division By Zero");
                }
            }
        }

        lval_del (y);
    }

    lval_del (a);
    return x;
}

lval *
lval_eval_sexpr (lval *v);

lval *
lval_eval (lval *v)
{
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr (v);

    /* All other values remain the same */
    return v;
}

lval *
lval_eval_sexpr (lval *v)
{
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++)
        v->cell[i] = lval_eval (v->cell[i]);

    /* Error Checking */
    for (int i = 0; i < v->count; i++)
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take (v, i);

    /* Empty Expression */
    if (v->count == 0) return v;

    /* Single Expression */
    if (v->count == 1) return lval_take (v, 0);

    /* Ensure First Element is Symbol */
    lval *f = lval_pop (v, 0);
    if (f->type != LVAL_SYM)
    {
        lval_del (f);
        lval_del (v);
        return lval_err("S-expression Does not start with symbol!");
    }

    /* Call Builtin with Operator */
    lval *result = builtin_op (v, f->sym);
    lval_del(f);
    return result;
}

int
main (void)
{
    puts ("Lispy Version 0.0.1\n");
    puts ("Press Ctrl+c to Exit\n");

    mpc_parser_t *Number = mpc_new ("number");
    mpc_parser_t *Symbol = mpc_new ("symbol");
    mpc_parser_t *Sexpr  = mpc_new ("sexpr");
    mpc_parser_t *Expr   = mpc_new ("expr");
    mpc_parser_t *Lispy  = mpc_new ("lispy");

    mpca_lang (MPCA_LANG_DEFAULT,
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

    while (FOREVER)
    {
        char *input = readline ("> ");
        add_history (input);

        mpc_result_t r;

        if (mpc_parse ("<stdin>", input, Lispy, &r))
        {
            lval *x = lval_eval (lval_read (r.output));
            lval_println (x);
            lval_del (x);
        }
        else
        {
            mpc_err_print (r.error);
            mpc_err_delete (r.error);
        }

        free (input);
    }

    mpc_cleanup (5, Number, Symbol, Sexpr, Expr, Lispy);
}
