#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#define MAX_TOKENS 128
#define MAX_TOKEN_LEN 256

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_REDIRECT_OUT,
    TOKEN_REDIRECT_IN,
    TOKEN_REDIRECT_APPEND,
    TOKEN_BACKGROUND
} TokenKind;

typedef struct {
    TokenKind type;
    char value[MAX_TOKEN_LEN];
} Token;

typedef struct {
    Token tokens[MAX_TOKENS];
    int count;
} TokenList;

typedef struct {
    char** argv;
    int argc;
    char* input_file;
    char* output_file;
    bool append_output;
    bool background;
} Command;

typedef struct {
    Command* commands;
    int count;
} CommandPipeline;

// Function prototypes
TokenList* parse_tokens(const char* line);
void free_tokens(TokenList* tokens);
CommandPipeline* parse_command_line(const char* line);
void free_command_pipeline(CommandPipeline* pipeline);
void free_command(Command* cmd);

#endif // PARSER_H

