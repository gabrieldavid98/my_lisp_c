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
            operator : '+' | '-' | '*' | '/' | '%' | \"add\" | \"sub\" | \"mul\" | \"div\" ; \
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
            mpc_ast_print(r.output);
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
