/**
 * CardboardOS - NotC Interpreter Implementation
 */

#include "notc.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include "../core/panic.h"
#include "../fs/vfs.h"
#include "../gui/gui.h"
#include <stddef.h>

// NotC token types
enum notc_token_type {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_KEYWORD,
    TOKEN_SYMBOL,
};

// NotC token
struct notc_token {
    enum notc_token_type type;
    char* text;
    int line;
    int col;
};

// NotC lexer
struct notc_lexer {
    const char* source;
    size_t pos;
    size_t len;
    int line;
    int col;
    char current;
};

// NotC parser
struct notc_parser {
    struct notc_token* tokens;
    size_t count;
    size_t pos;
    bool error;
    char error_msg[256];
};

// NotC interpreter state
static struct notc_state state;
static char error_buffer[256] = {0};

// Built-in functions registry
#define MAX_BUILTINS 32
static struct {
    char name[32];
    struct notc_value (*func)(struct notc_state*, struct notc_value*);
} builtins[MAX_BUILTINS];
static int builtin_count = 0;

// Forward declarations
static struct notc_value parse_expression(struct notc_parser* parser);
static struct notc_value parse_block(struct notc_parser* parser);
static struct notc_value parse_function_call(struct notc_parser* parser, const char* name);

// Lexer functions
static char notc_lexer_next_char(struct notc_lexer* lexer) {
    if (lexer->pos >= lexer->len) {
        lexer->current = '\0';
        return '\0';
    }
    
    char c = lexer->source[lexer->pos++];
    lexer->current = c;
    
    if (c == '\n') {
        lexer->line++;
        lexer->col = 0;
    } else {
        lexer->col++;
    }
    
    return c;
}

static void notc_lexer_skip_whitespace(struct notc_lexer* lexer) {
    while (lexer->current == ' ' || lexer->current == '\t' || lexer->current == '\n' || lexer->current == '\r') {
        notc_lexer_next_char(lexer);
    }
}

static struct notc_token notc_lexer_next_token(struct notc_lexer* lexer) {
    struct notc_token token = {0};
    token.line = lexer->line;
    token.col = lexer->col;
    
    notc_lexer_skip_whitespace(lexer);
    
    if (lexer->current == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }
    
    // Numbers
    if (lexer->current >= '0' && lexer->current <= '9') {
        token.type = TOKEN_NUMBER;
        // TODO: Parse number
        return token;
    }
    
    // Strings
    if (lexer->current == '"') {
        token.type = TOKEN_STRING;
        notc_lexer_next_char(lexer);
        // TODO: Parse string
        return token;
    }
    
    // Identifiers and keywords
    if ((lexer->current >= 'a' && lexer->current <= 'z') ||
        (lexer->current >= 'A' && lexer->current <= 'Z') ||
        lexer->current == '_') {
        token.type = TOKEN_IDENTIFIER;
        // TODO: Parse identifier
        return token;
    }
    
    // Symbols
    token.type = TOKEN_SYMBOL;
    token.text = (char*)&lexer->current;
    notc_lexer_next_char(lexer);
    
    return token;
}

// Parser functions
static struct notc_value parse_statement(struct notc_parser* parser) {
    struct notc_token token = parser->tokens[parser->pos];
    
    if (token.type == TOKEN_IDENTIFIER) {
        // Check if it's a function call
        if (parser->tokens[parser->pos + 1].type == TOKEN_SYMBOL &&
            parser->tokens[parser->pos + 1].text[0] == '(') {
            return parse_function_call(parser, token.text);
        }
    }
    
    // TODO: More statement types
    
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    return result;
}

static struct notc_value parse_block(struct notc_parser* parser) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    // Expect '['
    if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
        parser->tokens[parser->pos].text[0] != '[') {
        parser->error = true;
        strcpy(parser->error_msg, "Expected '['");
        return result;
    }
    parser->pos++;
    
    // Parse statements until ']'
    while (parser->pos < parser->count) {
        if (parser->tokens[parser->pos].type == TOKEN_SYMBOL &&
            parser->tokens[parser->pos].text[0] == ']') {
            parser->pos++;
            break;
        }
        
        result = parse_statement(parser);
        if (parser->error) {
            return result;
        }
    }
    
    return result;
}

static struct notc_value parse_function_call(struct notc_parser* parser, const char* name) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    // Look for built-in function
    for (int i = 0; i < builtin_count; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            // TODO: Parse arguments
            
            // Call the built-in function
            result = builtins[i].func(&state, NULL);
            return result;
        }
    }
    
    parser->error = true;
    snprintf(parser->error_msg, 256, "Unknown function: %s", name);
    return result;
}

// Interpreter functions
void notc_init(void) {
    memset(&state, 0, sizeof(state));
    state.stack_capacity = 1024;
    state.stack = (struct notc_value*)malloc(sizeof(struct notc_value) * state.stack_capacity);
    if (!state.stack) {
        panic("Failed to allocate NotC stack");
    }
    
    // Register built-in functions
    notc_register_builtin("write", notc_builtin_write);
    notc_register_builtin("read", notc_builtin_read);
    notc_register_builtin("readkey", notc_builtin_readkey);
    notc_register_builtin("listfiles", notc_builtin_listfiles);
    notc_register_builtin("run", notc_builtin_run);
    notc_register_builtin("exit", notc_builtin_exit);
    
    state.running = true;
    state.error = false;
}

bool notc_execute(const char* source) {
    if (!source) return false;
    
    // Initialize lexer
    struct notc_lexer lexer = {0};
    lexer.source = source;
    lexer.len = strlen(source);
    lexer.line = 1;
    lexer.col = 0;
    notc_lexer_next_char(&lexer);
    
    // Tokenize
    struct notc_token tokens[1024];
    int token_count = 0;
    
    while (token_count < 1024) {
        struct notc_token token = notc_lexer_next_token(&lexer);
        tokens[token_count++] = token;
        if (token.type == TOKEN_EOF) break;
    }
    
    // Parse
    struct notc_parser parser = {0};
    parser.tokens = tokens;
    parser.count = token_count;
    parser.pos = 0;
    parser.error = false;
    
    // Find main function
    while (parser.pos < parser.count) {
        if (parser.tokens[parser.pos].type == TOKEN_IDENTIFIER &&
            strcmp(parser.tokens[parser.pos].text, "main") == 0) {
            parser.pos++;
            if (parser.tokens[parser.pos].type == TOKEN_SYMBOL &&
                parser.tokens[parser.pos].text[0] == ':') {
                parser.pos++;
                parse_block(&parser);
                break;
            }
        }
        parser.pos++;
    }
    
    if (parser.error) {
        strcpy(error_buffer, parser.error_msg);
        return false;
    }
    
    return true;
}

bool notc_run_file(const char* filename) {
    // TODO: Read file from filesystem
    // For now, return false
    return false;
}

void notc_register_builtin(const char* name, struct notc_value (*func)(struct notc_state*, struct notc_value*)) {
    if (builtin_count >= MAX_BUILTINS) return;
    
    strncpy(builtins[builtin_count].name, name, 31);
    builtins[builtin_count].name[31] = '\0';
    builtins[builtin_count].func = func;
    builtin_count++;
}

const char* notc_get_error(void) {
    return error_buffer;
}

// Built-in function implementations
struct notc_value notc_builtin_write(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    if (!args || args->type != NOTC_TYPE_STRING) {
        return result;
    }
    
    // Write to VGA or GUI
    vga_write(args->data.string_val, 0, 0);
    
    return result;
}

struct notc_value notc_builtin_read(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_STRING;
    // TODO: Implement read
    return result;
}

struct notc_value notc_builtin_readkey(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_STRING;
    // TODO: Implement readkey
    return result;
}

struct notc_value notc_builtin_listfiles(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_STRING;
    // TODO: Implement listfiles
    return result;
}

struct notc_value notc_builtin_run(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    // TODO: Implement run
    return result;
}

struct notc_value notc_builtin_exit(struct notc_state* st, struct notc_value* args) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    st->running = false;
    return result;
}