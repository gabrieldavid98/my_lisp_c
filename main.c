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

enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR
};

typedef struct lval
{
    int type;
    long num;
    char *err;
    char *sym;
    int count;
    struct lval **cell;
} lval;

void lval_print(lval *v);
lval *lval_eval(lval *v);
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

lval *builtin_op(lval *v, char *op)
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

lval *builtin_head(lval *v)
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

lval *builtin_tail(lval *v)
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

lval *builtin_list(lval *v)
{
    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lval *v)
{
    LASSERT(v, v->count == 1,
            "Function 'eval' called with wrong number of arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' called with wrong type");

    lval *vv = lval_take(v, 0);
    vv->type = LVAL_SEXPR;

    return lval_eval(vv);
}

lval *builtin_join(lval *v)
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

lval *builtin(lval *v, char *func)
{
    if (strcmp("list", func) == 0)
        return builtin_list(v);
    if (strcmp("head", func) == 0)
        return builtin_head(v);
    if (strcmp("tail", func) == 0)
        return builtin_tail(v);
    if (strcmp("join", func) == 0)
        return builtin_join(v);
    if (strcmp("eval", func) == 0)
        return builtin_eval(v);
    if (strstr("+-/*", func))
        return builtin_op(v, func);
    lval_del(v);
    return lval_err("Unknown Function!");
}

lval *lval_eval_sexpr(lval *v)
{
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);
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
    if (f->type != LVAL_SYM)
    {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    lval *result = builtin(v, f->sym);
    lval_del(f);

    return result;
}

lval *lval_eval(lval *v)
{
    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(v);

    return v;
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
            symbol : '+' \
                     | '-' \
                     | '*' \
                     | '/' \
                     | '%' \
                     | '^' \
                     | \"add\" \
                     | \"sub\" \
                     | \"mul\" \
                     | \"div\" \
                     | \"mod\" \
                     | \"min\" \
                     | \"max\" \
                     | \"list\" \
                     | \"head\" \
                     | \"tail\" \
                     | \"join\" \
                     | \"eval\" ; \
            sexpr : '(' <expr>* ')' ; \
            qexpr : '{' <expr>* '}' ; \
            expr : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy : /^/ <expr>* /$/ ; \
          ",
              integer, decimal, number, symbol, sexpr, qexpr, expr, lispy);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1)
    {
        char *input = readline("(lispy)> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r))
        {
            lval *result = lval_eval(lval_read(r.output));
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

    mpc_cleanup(8, integer, decimal, number, symbol, sexpr, qexpr, expr, lispy);
    return 0;
}
