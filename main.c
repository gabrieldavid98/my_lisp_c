#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';

    return cpy;
}

void add_history(char* unused) {}

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

long eval_op(long x, char* op, long y)
{
    if (strcmp(op, "+") == 0 || strcmp(op, "add") == 0) return x + y;
    if (strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) return x - y;
    if (strcmp(op, "*") == 0 || strcmp(op, "mul") == 0) return x * y;
    if (strcmp(op, "/") == 0 || strcmp(op, "div") == 0) return x / y;
    if (strcmp(op, "%") == 0 || strcmp(op, "mod") == 0) return x % y;
    if (strcmp(op, "^") == 0) return pow_l(x, y);
    if (strcmp(op, "min") == 0) return min_l(x, y);
    if (strcmp(op, "max") == 0) return max_l(x, y);

    return 0;
}

long eval(mpc_ast_t* t)
{
    if (strstr(t->tag, "integer"))
    {
        return atoi(t->contents);
    }

    char* op = t->children[1]->contents;
    long x = eval(t->children[2]);

    int arg_num = 0;
    for (int i = 2; t->children[i] != t->children[t->children_num - 1]; i++)
    {
        arg_num++;
    }

    if (strcmp(op, "-") == 0 && arg_num == 1)
    {
        return x * -1;
    }

    int i = 3;
    while(strstr(t->children[i]->tag, "expr"))
    {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv)
{
    mpc_parser_t* integer = mpc_new("integer");
    mpc_parser_t* decimal = mpc_new("decimal");
    mpc_parser_t* number = mpc_new("number");
    mpc_parser_t* operator = mpc_new("operator");
    mpc_parser_t* expr = mpc_new("expr");
    mpc_parser_t* lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
              "\
            integer : /-?\\d+/ ; \
            decimal : /-?\\d+\\.\\d+/ ; \
            number :  <decimal> | <integer> ; \
            operator : '+' \
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
                     | \"max\" ; \
            expr : <number> | '(' <operator> <expr>+ ')' ; \
            lispy : /^/ <operator> <expr>+ /$/ ; \
          ", integer, decimal, number, operator, expr, lispy);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1)
    {
        char* input = readline("(lispy)> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r))
        {
            long result = eval(r.output);
            printf("%li\n", result);
            // mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(6, integer, decimal, number, operator, expr, lispy);
    return 0;
}
