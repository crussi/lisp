#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOK_MAX 1024

typedef enum {
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_NUM,
    TOK_STR,
    TOK_ATOM,
    TOK_LAMBDA
} TokenType;

typedef struct Token {
    TokenType type;
    union {
        int ivalue;
        float fvalue;
        char *svalue;
    };
} Token;

typedef struct LispNode {
    union {
        struct {
            struct LispNode **children;
            int child_count;
        };
        struct {
            char **params;
            int param_count;
            struct LispNode *body;
        };
        union {
            int ivalue;
            float fvalue;
            char *svalue;
        };
    };
    TokenType type;
} LispNode;

typedef struct {
    char *name;
    LispNode *value;
} Symbol;

Symbol symtable[1024];
int symcount = 0;

char *read_file(const char *);
unsigned int lex(const char *, Token *);
LispNode *parse_atom(Token *, int *);
LispNode *parse_list(Token *, int *);
LispNode *parse_expr(Token *, int *);
void print_node(LispNode *, int);
void define_symbol(char *, LispNode *);
Symbol *lookup_symbol(char *);
LispNode *eval(LispNode *);

int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input.lisp>\n", argv[0]);
        exit(EXIT_FAILURE);
        return 0;
    }
    char *buf = read_file(argv[1]);

    Token tokarr[TOK_MAX];
    unsigned int num_tokens = lex(buf, tokarr);
    int pos = 0;
    while (pos < (int) num_tokens) {
        LispNode *root = parse_expr(tokarr, &pos);
        eval(root);
    }

    free(buf);
    return 0;
}

static int
is_atom_char(char c)
{
    return c != '"' && c != '(' && c != ')' && !isspace(c) && c != '\0';
}


char *
read_file(const char *path)
{
    if (!path) return NULL;
    FILE *in = fopen(path, "r");
    if (!in) return NULL;
    fseek(in, 0, SEEK_END);
    long int size = ftell(in);
    fseek(in, 0, SEEK_SET);
    char *x = malloc(size + 1);
    if (!x) return NULL;
    fread(x, sizeof(char), size, in);
    x[size] = '\0';
    fclose(in);
    return x;
}

unsigned int
lex(const char *src, Token *tokarr)
{
    unsigned int tokens = 0;
    for (int i = 0; src[i] != '\0'; i++) {
        if (isdigit(src[i])) {
            tokarr[tokens++] = (Token){
                .type = TOK_NUM,
                .ivalue = atoi(&src[i])
            };
            while (isdigit(src[i+1]))
                i++;
        } else if (is_atom_char(src[i])) { // TODO: add floats
            int start = i;
            while (is_atom_char(src[i]))
                i++;
            int length = i - start;
            i--;
            tokarr[tokens++] = (Token){
                .type = TOK_ATOM,
                .svalue = strndup(&src[start], length)
            };
        } else {
            switch (src[i]) {
            case '(':
                tokarr[tokens++].type = TOK_LPAREN;
                break;
            case ')':
                tokarr[tokens++].type = TOK_RPAREN;
                break;
            case '"': {
                i++;
                int start = i;
                while (src[i] != '"' && src[i] != '\0')
                    i++;
                int length = i - start;
                tokarr[tokens++] = (Token){
                    .type = TOK_STR,
                    .svalue = strndup(&src[start], length)
                };
                break;
            }
            }
        }
    }
    return tokens;
}

LispNode *
parse_atom(Token *tokarr, int *pos)
{
    if (!tokarr || !pos) return NULL;

    TokenType type = tokarr[*pos].type;
    if (type == TOK_LPAREN || type == TOK_RPAREN) return NULL;

    LispNode *n = calloc(1, sizeof(LispNode));
    if (!n) return NULL;

    n->type = type;
    switch (type) { // TODO: add floats
    case TOK_STR:
    case TOK_ATOM:
        n->svalue = tokarr[(*pos)++].svalue;
        break;
    case TOK_NUM:
        n->ivalue = tokarr[(*pos)++].ivalue;
        break;
    case TOK_LPAREN: // pedantic (for the compiler)
    case TOK_RPAREN: // ^
    case TOK_LAMBDA: // ^
        return NULL;
    }

    return n;
}

LispNode *
parse_list(Token *tokarr, int *pos)
{
    if (!tokarr || !pos ) return NULL;

    TokenType type = tokarr[*pos].type;
    if (type != TOK_LPAREN) return NULL;

    LispNode *n = calloc(1, sizeof(LispNode));
    if (!n) return NULL;

    if (tokarr[*pos].type == TOK_LPAREN)
        (*pos)++;

    while (tokarr[*pos].type != TOK_RPAREN) {
        n->children = realloc(n->children, sizeof(LispNode *) * (n->child_count + 1));
        n->children[n->child_count++] = parse_expr(tokarr, pos);
    }
    (*pos)++;

    return n;
}

LispNode *
parse_expr(Token *tokarr, int *pos)
{
    if (!tokarr || !pos) return NULL;

    LispNode *n;
    if (tokarr[*pos].type == TOK_LPAREN)
        n = parse_list(tokarr, pos);
    else
        n = parse_atom(tokarr, pos);

    return n;
}

// this may not be needed
void
print_node(LispNode *node, int depth)
{
    if (!node) return;

    switch (node->type) {
    case TOK_NUM:
        printf("%*s%s %d\n", depth, "", "NUMBER", node->ivalue);
        break;
    case TOK_LPAREN: {
        printf("%*s%s\n", depth++, "", "LPAREN");
        for (int i = 0; i < node->child_count; i++)
            print_node(node->children[i], depth);
        break;
    }
    case TOK_STR:
        printf("%*s%s %s\n", depth, "", "STR", node->svalue);
        break;
    case TOK_ATOM:
        printf("%*s%s %s\n", depth, "", "ATOM", node->svalue);
        break;
    case TOK_RPAREN:
    case TOK_LAMBDA:
        return;
    }
}

void
define_symbol(char *name, LispNode *value)
{
    if (!name) return;
    symtable[symcount].name = name;
    symtable[symcount].value = value;
    symcount++;
}

Symbol *
lookup_symbol(char *name)
{
    for (int i = symcount - 1; i >= 0; i--) {
        if (strcmp(symtable[i].name, name) == 0)
            return &symtable[i];
    }
    return NULL;
}

static int
compare(int x, int y, char *type)
{
    if (strcmp(type, "=") == 0) {
        return x == y;
    } else if (strcmp(type, "!=") == 0) {
        return x != y;
    } else if (strcmp(type, "<") == 0) {
        return x < y;
    } else if (strcmp(type, ">") == 0) {
        return x > y;
    } else if (strcmp(type, "<=") == 0) {
        return x <= y;
    } else if (strcmp(type, ">=") == 0) {
        return x >= y;
    }
    return 0;
}

LispNode *
eval(LispNode *node)
{
    if (!node) return NULL;

    switch (node->type) {
    case TOK_NUM:
    case TOK_STR:
        return node;
    case TOK_ATOM: { // TODO: better error handling
        Symbol *tempsym = lookup_symbol(node->svalue);
        if (!tempsym) {
            fprintf(stderr, "%s not defined!", node->svalue);
            exit(EXIT_FAILURE);
        }
        return tempsym->value;
    }
    case TOK_LPAREN: {
        char *op = node->children[0]->svalue;
        LispNode *n = calloc(1, sizeof(LispNode));

        if (strcmp(op, "+") == 0) {
            int left = eval(node->children[1])->ivalue;
            int right = eval(node->children[2])->ivalue;

            n->type = TOK_NUM;
            n->ivalue = left + right;

            return n;
        } else if (strcmp(op, "-") == 0) {
            int left = eval(node->children[1])->ivalue;
            int right = eval(node->children[2])->ivalue;

            n->type = TOK_NUM;
            n->ivalue = left - right;

            return n;
        } else if (strcmp(op, "*") == 0) {
            int left = eval(node->children[1])->ivalue;
            int right = eval(node->children[2])->ivalue;

            n->type = TOK_NUM;
            n->ivalue = left * right;

            return n;
        } else if (strcmp(op, "/") == 0) {
            int left = eval(node->children[1])->ivalue;
            int right = eval(node->children[2])->ivalue;

            n->type = TOK_NUM;
            n->ivalue = left / right;

            return n;
        } else if (strcmp(op, "=") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, "=");
            return n;
        } else if (strcmp(op, "<") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, "<");
            return n;
        } else if (strcmp(op, ">") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, ">");
            return n;
        } else if (strcmp(op, "<=") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, "<=");
            return n;
        } else if (strcmp(op, ">=") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, ">=");
            return n;
        } else if (strcmp(op, "!=") == 0) {
            n->type = TOK_NUM;
            n->ivalue = compare(eval(node->children[1])->ivalue, eval(node->children[2])->ivalue, "!=");
            return n;
        } else if (strcmp(op, "define") == 0) { /* DEFINE */
            free(n);
            define_symbol(node->children[1]->svalue, eval(node->children[2]));
            break;
        } else if (strcmp(op, "print") == 0) {  /* PRINT */
            free(n);

            LispNode *arg = eval(node->children[1]);
            if (arg->type == TOK_NUM)
                printf("%d", arg->ivalue);
            else if (arg->type == TOK_STR)
                printf("%s", arg->svalue);

            break;
        } else if (strcmp(op, "scan") == 0) {  /* PRINT */
            char buf[4096];
            fgets(buf, sizeof(buf), stdin);
            buf[strcspn(buf, "\n")] = '\0';

            if (isdigit(buf[0])) {
                n->type = TOK_NUM;
                n->ivalue = atoi(&buf[0]);
            } else {
                n->type = TOK_STR;
                n->svalue = strdup(buf);
            }

            return n;
        } else if (strcmp(op, "lambda") == 0) { /* LAMBDA */
            n->type = TOK_LAMBDA;
            n->body = node->children[2];

            int count = node->children[1]->child_count;
            n->params = malloc(sizeof(char *) * count);
            n->param_count = count;

            for (int i = 0; i < count; i++)
                n->params[i] = node->children[1]->children[i]->svalue;
            return n;
        } else if (strcmp(op, "if") == 0) { // if () then else
            free(n);

            if (eval(node->children[1])->ivalue == 1)
                return eval(node->children[2]);
            else
                return eval(node->children[3]);
        } else { // assume function call
            LispNode *fn = eval(node->children[0]);
            int temp = symcount;

            if (fn && fn->type == TOK_LAMBDA) {
                for (int i = 0; i < fn->param_count; i++) {
                    define_symbol(fn->params[i], eval(node->children[i + 1]));
                }
            }

            LispNode *result = eval(fn->body);
            symcount = temp;
            return result;
        }
    }
    case TOK_LAMBDA: // pedantic (for the compiler)
    case TOK_RPAREN: // ^
        break;
    }

    return NULL;
}
