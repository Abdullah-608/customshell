#include "parser.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void add_token(TokenList* tokens, TokenKind type, const char* value) {
    if (tokens->count >= MAX_TOKENS) return;
    
    tokens->tokens[tokens->count].type = type;
    if (value) {
        strncpy(tokens->tokens[tokens->count].value, value, MAX_TOKEN_LEN - 1);
        tokens->tokens[tokens->count].value[MAX_TOKEN_LEN - 1] = '\0';
    } else {
        tokens->tokens[tokens->count].value[0] = '\0';
    }
    tokens->count++;
}

TokenList* parse_tokens(const char* line) {
    if (!line) return NULL;
    
    TokenList* tokens = (TokenList*)xmalloc(sizeof(TokenList));
    tokens->count = 0;
    
    const char* p = line;
    char token[MAX_TOKEN_LEN];
    int token_len = 0;
    bool in_quotes = false;
    bool in_single_quotes = false;
    char quote_char = 0;
    
    while (*p) {
        if (token_len >= MAX_TOKEN_LEN - 1) {
            token[token_len] = '\0';
            add_token(tokens, TOKEN_WORD, token);
            token_len = 0;
        }
        
        if (!in_quotes && !in_single_quotes) {
            // Check for operators
            if (*p == '|') {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    add_token(tokens, TOKEN_WORD, token);
                    token_len = 0;
                }
                add_token(tokens, TOKEN_PIPE, NULL);
                p++;
                continue;
            }
            
            if (*p == '>') {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    add_token(tokens, TOKEN_WORD, token);
                    token_len = 0;
                }
                p++;
                if (*p == '>') {
                    add_token(tokens, TOKEN_REDIRECT_APPEND, NULL);
                    p++;
                } else {
                    add_token(tokens, TOKEN_REDIRECT_OUT, NULL);
                }
                continue;
            }
            
            if (*p == '<') {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    add_token(tokens, TOKEN_WORD, token);
                    token_len = 0;
                }
                add_token(tokens, TOKEN_REDIRECT_IN, NULL);
                p++;
                continue;
            }
            
            if (*p == '&' && (p[1] == '\0' || isspace(p[1]))) {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    add_token(tokens, TOKEN_WORD, token);
                    token_len = 0;
                }
                add_token(tokens, TOKEN_BACKGROUND, NULL);
                p++;
                continue;
            }
            
            // Check for quotes
            if (*p == '"') {
                in_quotes = true;
                quote_char = '"';
                p++;
                continue;
            }
            
            if (*p == '\'') {
                in_single_quotes = true;
                quote_char = '\'';
                p++;
                continue;
            }
            
            // Check for escape character
            if (*p == '\\') {
                p++;
                if (*p) {
                    token[token_len++] = *p;
                    p++;
                }
                continue;
            }
            
            // Whitespace ends token
            if (isspace(*p)) {
                if (token_len > 0) {
                    token[token_len] = '\0';
                    add_token(tokens, TOKEN_WORD, token);
                    token_len = 0;
                }
                p++;
                continue;
            }
        } else {
            // Inside quotes
            if ((in_quotes && *p == '"') || (in_single_quotes && *p == '\'')) {
                in_quotes = false;
                in_single_quotes = false;
                p++;
                continue;
            }
            
            // Handle escape in quotes
            if (*p == '\\' && in_quotes) {
                p++;
                if (*p) {
                    token[token_len++] = *p;
                    p++;
                }
                continue;
            }
        }
        
        token[token_len++] = *p;
        p++;
    }
    
    // Add remaining token
    if (token_len > 0) {
        token[token_len] = '\0';
        add_token(tokens, TOKEN_WORD, token);
    }
    
    return tokens;
}

void free_tokens(TokenList* tokens) {
    free(tokens);
}

CommandPipeline* parse_command_line(const char* line) {
    if (!line) return NULL;
    
    TokenList* tokens = parse_tokens(line);
    if (!tokens || tokens->count == 0) {
        if (tokens) free_tokens(tokens);
        return NULL;
    }
    
    CommandPipeline* pipeline = (CommandPipeline*)xmalloc(sizeof(CommandPipeline));
    pipeline->commands = NULL;
    pipeline->count = 0;
    
    Command* current_cmd = NULL;
    bool expect_redirect_file = false;
    TokenKind redirect_type = TOKEN_REDIRECT_OUT;
    
    for (int i = 0; i < tokens->count; i++) {
        Token* token = &tokens->tokens[i];
        
        if (token->type == TOKEN_PIPE) {
            expect_redirect_file = false;
            current_cmd = NULL;
            continue;
        }
        
        if (token->type == TOKEN_REDIRECT_OUT || 
            token->type == TOKEN_REDIRECT_IN ||
            token->type == TOKEN_REDIRECT_APPEND) {
            expect_redirect_file = true;
            redirect_type = token->type;
            if (!current_cmd) {
                // Need to start a new command
                pipeline->commands = (Command*)xrealloc(
                    pipeline->commands, 
                    (pipeline->count + 1) * sizeof(Command));
                current_cmd = &pipeline->commands[pipeline->count++];
                memset(current_cmd, 0, sizeof(Command));
            }
            continue;
        }
        
        if (token->type == TOKEN_BACKGROUND) {
            if (current_cmd) {
                current_cmd->background = true;
            }
            continue;
        }
        
        if (expect_redirect_file && token->type == TOKEN_WORD) {
            expect_redirect_file = false;
            if (current_cmd) {
                if (redirect_type == TOKEN_REDIRECT_IN) {
                    current_cmd->input_file = strdup(token->value);
                } else {
                    current_cmd->output_file = strdup(token->value);
                    current_cmd->append_output = (redirect_type == TOKEN_REDIRECT_APPEND);
                }
            }
            continue;
        }
        
        // Regular word token
        if (token->type == TOKEN_WORD) {
            if (!current_cmd) {
                // Start new command
                pipeline->commands = (Command*)xrealloc(
                    pipeline->commands, 
                    (pipeline->count + 1) * sizeof(Command));
                current_cmd = &pipeline->commands[pipeline->count++];
                memset(current_cmd, 0, sizeof(Command));
            }
            
            // Add argument
            current_cmd->argv = (char**)xrealloc(
                current_cmd->argv,
                (current_cmd->argc + 1) * sizeof(char*));
            current_cmd->argv[current_cmd->argc++] = strdup(token->value);
        }
    }
    
    free_tokens(tokens);
    return pipeline;
}

void free_command(Command* cmd) {
    if (!cmd) return;
    
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }
    
    free(cmd->input_file);
    free(cmd->output_file);
}

void free_command_pipeline(CommandPipeline* pipeline) {
    if (!pipeline) return;
    
    if (pipeline->commands) {
        for (int i = 0; i < pipeline->count; i++) {
            free_command(&pipeline->commands[i]);
        }
        free(pipeline->commands);
    }
    
    free(pipeline);
}

