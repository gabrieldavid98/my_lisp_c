#include <stdio.h>
#include <stdlib.h>
#include <mpc.h>

#define LASSERT(args, cond, err) \
    if (!(cond))                 \
    {                            \
        lval_del(args);          \
        return lval_err(err);    \
    }

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';

    return cpy;
}

void add_history(char *unused) {}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif // _WIN32

long pow_l(long x, long y)
{
    if (y == 0)
    {
        return 1;
    }

    long res = x;
    for (int i = 1; i < y; i++)
    {
        res *= res;
    }

    return res;
}

long min_l(long x, long y)
{
    if (x < y)
    {
        return x;
    }

    return y;
}

long max_l(long x, long y)
{
    if (x > y)
    {
        return x;
    }

    return y;
}

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR
};

typedef lval *(*lbuiltin)(lenv *, lval *);

struct lval
{
    int type;
    long num;
    char *err;
    char *sym;
    lbuiltin fun;

    int count;
    struct lval **cell;
};

struct lenv
{
    int count;
    char **syms;
    lval **vals;
};

void lval_print(lval *v);
lval *lval_eval(lenv *e, lval *v);
lval *lval_join(lval *x, lval *y);

lval *lval_num(long num)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = num;
    return v;
}

lval *lval_err(char *err)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(err) + 1);
    strcpy(v->err, err);
    return v;
}

lval *lval_sym(char *sym)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

lval *lval_fun(lbuiltin func)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

lval *lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval *lval_qexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v)
{
    switch (v->type)
    {
    case LVAL_NUM:
        break;
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;
    case LVAL_FUN:
        break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
        {
            lval_del(v->cell[i]);
        }

        free(v->cell);
        break;
    }

    free(v);
}

lval *lval_copy(lval *v)
{
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type)
    {
    case LVAL_FUN:
        x->fun = v->fun;
        break;
    case LVAL_NUM:
        x->num = v->num;
        break;
    case LVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err);
        break;
    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym);
        break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(lval *) * x->count);
        for (int i = 0; i < x->count; i++)
        {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
    }

    return x;
}

lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e)
{
    for (int i = 0; i < e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

lval *lenv_get(lenv *e, lval *v)
{
    for (int i = 0; i < e->count; i++)
    {
        if (strcmp(e->syms[i], v->sym) == 0)
        {
            return lval_copy(e->vals[i]);
        }
    }

    return lval_err("unbound symbol!");
}

void lenv_put(lenv *e, lval *k, lval *v)
{
    for (int i = 0; i < e->count; i++)
    {
        if (strcmp(e->syms[i], k->sym) == 0)
        {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);

    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

lval *lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count - 1] = x;

    return v;
}

lval *lval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long num = strtol(t->contents, NULL, 10);

    return errno != ERANGE ? lval_num(num) : lval_err("invalid number");
}

lval *lval_read(mpc_ast_t *t)
{
    if (strstr(t->tag, "number"))
        return lval_read_num(t);
    if (strstr(t->tag, "symbol"))
        return lval_sym(t->contents);

    lval *v = NULL;
    if (strcmp(t->tag, ">") == 0)
        v = lval_sexpr();
    if (strstr(t->tag, "sexpr"))
        v = lval_sexpr();
    if (strstr(t->tag, "qexpr"))
        v = lval_qexpr();

    for (int i = 0; i < t->children_num; i++)
    {
        if (strcmp(t->children[i]->contents, "(") == 0)
            continue;
        if (strcmp(t->children[i]->contents, ")") == 0)
            continue;
        if (strcmp(t->children[i]->contents, "{") == 0)
            continue;
        if (strcmp(t->children[i]->contents, "}") == 0)
            continue;
        if (strcmp(t->children[i]->tag, "regex") == 0)
            continue;

        v = lval_add(v, lval_read(t->children[i]));
    }

    return v;
}

void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);

    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);

        if (i != (v->count - 1))
            putchar(' ');
    }

    putchar(close);
}

void lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_NUM:
        printf("%li", v->num);
        break;
    case LVAL_ERR:
        printf("Error: %s", v->err);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_FUN:
        printf("<function>");
        break;
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    case LVAL_QEXPR:
        lval_expr_print(v, '{', '}');
        break;
    }
}

void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

lval *lval_pop(lval *v, int i)
{
    lval *x = v->cell[i];
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);

    return x;
}

lval *lval_take(lval *v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);

    return x;
}

lval *builtin_op(lenv *e, lval *v, char *op)
{
    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type != LVAL_NUM)
        {
            lval_del(v);
            return lval_err("Cannot operate on non-number!");
        }
    }

    lval *x = lval_pop(v, 0);

    if ((strcmp(op, "-") == 0) && v->count == 0)
        x->num = -x->num;

    while (v->count > 0)
    {
        lval *y = lval_pop(v, 0);

        if (strcmp(op, "+") == 0)
            x->num += y->num;
        if (strcmp(op, "-") == 0)
            x->num -= y->num;
        if (strcmp(op, "*") == 0)
            x->num *= y->num;
        if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0)
        {
            if (y->num == 0)
            {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero!");
                break;
            }

            if (strcmp(op, "/") == 0)
                x->num /= y->num;

            if (strcmp(op, "%") == 0)
                x->num = x->num % y->num;
        }

        lval_del(y);
    }

    lval_del(v);
    return x;
}

lval *builtin_add(lenv *e, lval *v)
{
    return builtin_op(e, v, "+");
}

lval *builtin_sub(lenv *e, lval *v)
{
    return builtin_op(e, v, "-");
}

lval *builtin_mul(lenv *e, lval *v)
{
    return builtin_op(e, v, "*");
}

lval *builtin_div(lenv *e, lval *v)
{
    return builtin_op(e, v, "/");
}

lval *builtin_head(lenv *e, lval *v)
{
    LASSERT(v, v->count == 1,
            "Function 'head' called with wrong number of arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'head' called with wrong type");
    LASSERT(v, v->cell[0]->count != 0,
            "Function 'head' called with empty {}");

    lval *head = lval_take(v, 0);

    while (head->count > 1)
    {
        lval_del(lval_pop(head, 1));
    }

    return head;
}

lval *builtin_tail(lenv *e, lval *v)
{
    LASSERT(v, v->count == 1,
            "Function 'tail' called with wrong number of arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' called with wrong type");
    LASSERT(v, v->cell[0]->count != 0,
            "Function 'tail' called with empty {}");

    lval *tail = lval_take(v, 0);

    lval_del(lval_pop(tail, 0));
    return tail;
}

lval *builtin_list(lenv *e, lval *v)
{
    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lenv *e, lval *v)
{
    LASSERT(v, v->count == 1,
            "Function 'eval' called with wrong number of arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' called with wrong type");

    lval *vv = lval_take(v, 0);
    vv->type = LVAL_SEXPR;

    return lval_eval(e, vv);
}

lval *builtin_join(lenv *e, lval *v)
{
    for (int i = 0; i < v->count; i++)
    {
        LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
                "Function 'join' called with wrong type");
    }

    lval *vv = lval_pop(v, 0);

    while (v->count)
    {
        vv = lval_join(vv, lval_pop(v, 0));
    }

    lval_del(v);
    return vv;
}

lval *lval_join(lval *x, lval *y)
{
    while (y->count)
    {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

lval *builtin_def(lenv *e, lval *a)
{
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'def' called with wrong type");

    lval *syms = a->cell[0];

    for (int i = 0; i < syms->count; i++)
    {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Function 'def' cannot define non-symbol")
    }

    LASSERT(a, syms->count == a->count - 1,
            "Function 'def' cannot define incorrect number of values to symbols");

    for (int i = 0; i < syms->count; i++)
    {
        lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }

    lval_del(a);
    return lval_sexpr();
}

// lval *builtin(lenv *e, lval *v, char *func)
// {
//     if (strcmp("list", func) == 0)
//         return builtin_list(v);
//     if (strcmp("head", func) == 0)
//         return builtin_head(v);
//     if (strcmp("tail", func) == 0)
//         return builtin_tail(v);
//     if (strcmp("join", func) == 0)
//         return builtin_join(v);
//     if (strcmp("eval", func) == 0)
//         return builtin_eval(v);
//     if (strstr("+-/*", func))
//         return builtin_op(e, v, func);
//     lval_del(v);
//     return lval_err("Unknown Function!");
// }

lval *lval_eval_sexpr(lenv *e, lval *v)
{
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    if (v->count == 0)
        return v;

    if (v->count == 1)
        return lval_take(v, 0);

    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN)
    {
        lval_del(v);
        lval_del(f);
        return lval_err("first element is not a function");
    }

    lval *result = f->fun(e, v);
    lval_del(f);

    return result;
}

lval *lval_eval(lenv *e, lval *v)
{
    if (v->type == LVAL_SYM)
    {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(e, v);

    return v;
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *e)
{
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    lenv_add_builtin(e, "def", builtin_def);
}

int main(int argc, char **argv)
{
    mpc_parser_t *integer = mpc_new("integer");
    mpc_parser_t *decimal = mpc_new("decimal");
    mpc_parser_t *number = mpc_new("number");
    mpc_parser_t *symbol = mpc_new("symbol");
    mpc_parser_t *sexpr = mpc_new("sexpr");
    mpc_parser_t *qexpr = mpc_new("qexpr");
    mpc_parser_t *expr = mpc_new("expr");
    mpc_parser_t *lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
              "\
            integer : /-?\\d+/ ; \
            decimal : /-?\\d+\\.\\d+/ ; \
            number :  <decimal> | <integer> ; \
            symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
            sexpr : '(' <expr>* ')' ; \
            qexpr : '{' <expr>* '}' ; \
            expr : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy : /^/ <expr>* /$/ ; \
          ",
              integer, decimal, number, symbol, sexpr, qexpr, expr, lispy);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv *e = lenv_new();
    lenv_add_builtins(e);

    while (1)
    {
        char *input = readline("(lispy)> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r))
        {
            lval *result = lval_eval(e, lval_read(r.output));
            lval_println(result);
            lval_del(result);
            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_del(e);

    mpc_cleanup(8, integer, decimal, number, symbol, sexpr, qexpr, expr, lispy);
    return 0;
}
