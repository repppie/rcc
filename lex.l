%option noyywrap

%{
#include <stdlib.h>
#include <string.h>
#include "rcc.h"

static int lineno = 1;

struct token *tok;
static struct token *last;

static struct token *
new_token(int _tok)
{
	struct token *t;

	t = malloc(sizeof(struct token));
	memset(t, 0, sizeof(struct token));
	if (!tok)
		tok = t;
	if (last)
		last->next = t;
	last = t;
	t->tok = _tok;
	t->line = lineno;

	return (t);
}
%}

alpha [a-zA-Z_]
digit [0-9]
hex_digit [0-9a-fA-F]
oct_digit [0-7]
ws [ \t]+
number {digit}+
hex_number {hex_digit}+
oct_number {oct_digit}+
id ({alpha})({alpha}|{digit})*
string \"[^\n"]+\"
char \'\\?[^\n']+\'

%%

"auto" { new_token(TOK_AUTO); };
"break" { new_token(TOK_BREAK); };
"case" { new_token(TOK_CASE); };
"char" { new_token(TOK_CHAR); };
"const" { new_token(TOK_CONST); };
"continue" { new_token(TOK_CONTINUE); };
"default" { new_token(TOK_DEFAULT); };
"do" { new_token(TOK_DO); };
"double" { new_token(TOK_DOUBLE); };
"else" { new_token(TOK_ELSE); };
"enum" { new_token(TOK_ENUM); };
"extern" { new_token(TOK_EXTERN); };
"float" { new_token(TOK_FLOAT); };
"for" { new_token(TOK_FOR); };
"goto" { new_token(TOK_GOTO); };
"if" { new_token(TOK_IF); };
"inline" { new_token(TOK_INLINE); };
"int" { new_token(TOK_INT); };
"long" { new_token(TOK_LONG); };
"register" { new_token(TOK_REGISTER); };
"restrict" { new_token(TOK_RESTRICT); };
"return" { new_token(TOK_RETURN); };
"short" { new_token(TOK_SHORT); };
"signed" { new_token(TOK_SIGNED); };
"sizeof" { new_token(TOK_SIZEOF); };
"static" { new_token(TOK_STATIC); };
"struct" { new_token(TOK_STRUCT); };
"switch" { new_token(TOK_SWITCH); };
"typedef" { new_token(TOK_TYPEDEF); };
"union" { new_token(TOK_UNION); };
"unsigned" { new_token(TOK_UNSIGNED); };
"void" { new_token(TOK_VOID); };
"volatile" { new_token(TOK_VOLATILE); };
"while" { new_token(TOK_WHILE); };

"[" { new_token('['); }
"]" { new_token(']'); }
"(" { new_token('('); }
")" { new_token(')'); }
"{" { new_token('{'); }
"}" { new_token('}'); }
"." { new_token('.'); }
"->" { new_token(TOK_PTR); }
"++" { new_token(TOK_INCR); }
"--" { new_token(TOK_DECR); }
"&" { new_token('&'); }
"*" { new_token('*'); }
"+" { new_token('+'); }
"-" { new_token('-'); }
"~" { new_token('~'); }
"!" { new_token('!'); }
"/" { new_token('/'); }
"%" { new_token('%'); }
"<<" { new_token(TOK_SL); }
">>" { new_token(TOK_SR); }
"<" { new_token(TOK_LT); }
">" { new_token(TOK_GT); }
"<=" { new_token(TOK_LE); }
">=" { new_token(TOK_GE); }
"==" { new_token(TOK_EQ); }
"!=" { new_token(TOK_NE); }
"^" { new_token('^'); }
"|" { new_token('|'); }
"&&" { new_token(TOK_AND); }
"||" { new_token(TOK_OR); }
"?" { new_token('?'); }
":" { new_token(':'); }
";" { new_token(';'); }
"..." { new_token(TOK_ELL); }
"=" { new_token('='); }
"*=" { new_token(TOK_ASSMUL); }
"/=" { new_token(TOK_ASSDIV); }
"%=" { new_token(TOK_ASSMOD); }
"+=" { new_token(TOK_ASSADD); }
"-=" { new_token(TOK_ASSSUB); }
"<<=" { new_token(TOK_ASSSL); }
">>=" { new_token(TOK_ASSSR); }
"&=" { new_token(TOK_ASSAND); }
"^=" { new_token(TOK_ASSXOR); }
"|=" { new_token(TOK_ASSOR); }
"," { new_token(','); }

^"#define"[^\n]+
^"#include"[^\n]+
^"#if"{ws}[^\n]+
^"#ifdef"[^\n]+
^"#ifndef"[^\n]+
^"#endif"
^"#else"

(0("x"|"X"){hex_number})|(0{oct_number})|{number} {
	new_token(TOK_CONSTANT);
	last->val = strtol(yytext, NULL, 0);
}
{char} {
	int v;
	new_token(TOK_CONSTANT);
	if (yytext[1] == '\\') {
		switch (yytext[2]) {
		case 'a':
			v = '\a';
			break;
		case 'b':
			v = '\b';
			break;
		case 'f':
			v = '\f';
			break;
		case 'n':
			v = '\n';
			break;
		case 'r':
			v = '\r';
			break;
		case 't':
			v = '\t';
			break;
		case 'v':
			v = '\v';
			break;
		case '\'':
			v = '\'';
			break;
		case '\"':
			v = '\"';
			break;
		case '\?':
			v = '\"';
			break;
		case '\\':
			v = '\\';
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			v = strtol(yytext + 2, NULL, 8);
			break;
		case 'x':
			v = strtol(yytext + 3, NULL, 16);
			break;
		default:
			printf("syntax error at line %d %s\n", lineno, yytext);
			yyterminate();
		}
	} else {
		v = yytext[1];
	}
	last->val = v;
}

{id} {
	new_token(TOK_ID);
	last->str = malloc(yyleng + 1);
	strlcpy(last->str, yytext, yyleng + 1);
}

{string} {
	new_token(TOK_STRING);
	last->str = malloc(yyleng - 1);
	strlcpy(last->str, yytext + 1, yyleng - 1);
}

"\n" lineno++;
{ws}
"//"[^\n]*
"/*" {
	int c;
	while ((c = input()) != 0) {
		if (c == '\n')
			lineno++;
		if (c == '*') {
			if ((c = input()) == '/')
				break;
			else
				unput(c);
		}
	}
}

. {
	printf("syntax error at line %d \"%s\"\n", lineno, yytext);
	yyterminate();
}

%%

void
lex(FILE *f)
{
	yyin = f;
	yylex();
	new_token(TOK_EOF);
}
