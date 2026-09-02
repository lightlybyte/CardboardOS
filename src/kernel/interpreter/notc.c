/**
 * CardboardOS - NotC Interpreter Complete Implementation
 */

#include "notc.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include "../core/panic.h"
#include "../fs/vfs.h"
#include "../gui/gui.h"
#include "../drivers/keyboard/keyboard.h"
#include <stddef.h>

// Token types
enum notc_token_type {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_KEYWORD,
    TOKEN_SYMBOL,
    TOKEN_OPERATOR,
    TOKEN_COMMENT,
};

// Token structure
struct notc_token {
    enum notc_token_type type;
    char* text;
    int line;
    int col;
};

// Lexer
struct notc_lexer {
    const char* source;
    size_t pos;
    size_t len;
    int line;
    int col;
    char current;
};

// Parser
struct notc_parser {
    struct notc_token* tokens;
    size_t count;
    size_t pos;
    bool error;
    char error_msg[256];
    struct notc_state* state;
};

// Interpreter state
static struct notc_state state;
static char error_buffer[256] = {0};
static struct notc_value* variables = NULL;
static size_t var_count = 0;

// Built-in functions
#define MAX_BUILTINS 32
static struct {
    char name[32];
    struct notc_value (*func)(struct notc_state*, struct notc_value*, size_t);
} builtins[MAX_BUILTINS];
static int builtin_count = 0;

// Lexer functions
static char lexer_next_char(struct notc_lexer* lexer) {
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

static void lexer_skip_whitespace(struct notc_lexer* lexer) {
    while (lexer->current == ' ' || lexer->current == '\t' || 
           lexer->current == '\n' || lexer->current == '\r') {
        lexer_next_char(lexer);
    }
}

static void lexer_skip_comment(struct notc_lexer* lexer) {
    if (lexer->current == ';') {
        while (lexer->current != '\n' && lexer->current != '\0') {
            lexer_next_char(lexer);
        }
    }
}

static struct notc_token lexer_next_token(struct notc_lexer* lexer) {
    struct notc_token token = {0};
    token.line = lexer->line;
    token.col = lexer->col;
    
    lexer_skip_whitespace(lexer);
    lexer_skip_comment(lexer);
    
    if (lexer->current == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }
    
    // Numbers
    if ((lexer->current >= '0' && lexer->current <= '9') || lexer->current == '-') {
        token.type = TOKEN_NUMBER;
        static char num_buffer[64];
        int i = 0;
        token.text = num_buffer;
        
        if (lexer->current == '-') {
            num_buffer[i++] = '-';
            lexer_next_char(lexer);
        }
        
        while (lexer->current >= '0' && lexer->current <= '9') {
            num_buffer[i++] = lexer->current;
            lexer_next_char(lexer);
        }
        num_buffer[i] = '\0';
        return token;
    }
    
    // Strings
    if (lexer->current == '"') {
        token.type = TOKEN_STRING;
        static char str_buffer[1024];
        int i = 0;
        token.text = str_buffer;
        
        lexer_next_char(lexer); // Skip opening quote
        
        while (lexer->current != '"' && lexer->current != '\0') {
            if (lexer->current == '\\') {
                lexer_next_char(lexer);
                switch (lexer->current) {
                    case 'n': str_buffer[i++] = '\n'; break;
                    case 't': str_buffer[i++] = '\t'; break;
                    case 'r': str_buffer[i++] = '\r'; break;
                    case '\\': str_buffer[i++] = '\\'; break;
                    case '"': str_buffer[i++] = '"'; break;
                    default: str_buffer[i++] = lexer->current; break;
                }
            } else {
                str_buffer[i++] = lexer->current;
            }
            lexer_next_char(lexer);
        }
        str_buffer[i] = '\0';
        lexer_next_char(lexer); // Skip closing quote
        return token;
    }
    
    // Identifiers and keywords
    if ((lexer->current >= 'a' && lexer->current <= 'z') ||
        (lexer->current >= 'A' && lexer->current <= 'Z') ||
        lexer->current == '_') {
        token.type = TOKEN_IDENTIFIER;
        static char id_buffer[256];
        int i = 0;
        token.text = id_buffer;
        
        while ((lexer->current >= 'a' && lexer->current <= 'z') ||
               (lexer->current >= 'A' && lexer->current <= 'Z') ||
               (lexer->current >= '0' && lexer->current <= '9') ||
               lexer->current == '_') {
            id_buffer[i++] = lexer->current;
            lexer_next_char(lexer);
        }
        id_buffer[i] = '\0';
        
        // Check if keyword
        if (strcmp(id_buffer, "if") == 0 ||
            strcmp(id_buffer, "else") == 0 ||
            strcmp(id_buffer, "while") == 0 ||
            strcmp(id_buffer, "for") == 0 ||
            strcmp(id_buffer, "return") == 0 ||
            strcmp(id_buffer, "func") == 0 ||
            strcmp(id_buffer, "var") == 0) {
            token.type = TOKEN_KEYWORD;
        }
        
        return token;
    }
    
    // Symbols and operators
    token.type = TOKEN_SYMBOL;
    static char sym_buffer[4];
    sym_buffer[0] = lexer->current;
    sym_buffer[1] = '\0';
    token.text = sym_buffer;
    
    // Check for multi-character operators
    if ((lexer->current == '=' && lexer_next_char(lexer) == '=') ||
        (lexer->current == '!' && lexer_next_char(lexer) == '=') ||
        (lexer->current == '<' && lexer_next_char(lexer) == '=') ||
        (lexer->current == '>' && lexer_next_char(lexer) == '=') ||
        (lexer->current == '&' && lexer_next_char(lexer) == '&') ||
        (lexer->current == '|' && lexer_next_char(lexer) == '|')) {
        sym_buffer[1] = lexer->current;
        sym_buffer[2] = '\0';
        lexer_next_char(lexer);
    }
    
    lexer_next_char(lexer);
    return token;
}

// Parser functions
static struct notc_value parse_expression(struct notc_parser* parser);
static struct notc_value parse_statement(struct notc_parser* parser);
static struct notc_value parse_block(struct notc_parser* parser);
static struct notc_value parse_function_call(struct notc_parser* parser, const char* name);
static struct notc_value parse_variable(struct notc_parser* parser);

// Variable management
static struct notc_value* find_variable(const char* name) {
    for (size_t i = 0; i < var_count; i++) {
        if (strcmp(variables[i].data.string_val, name) == 0) {
            return &variables[i + 1];
        }
    }
    return NULL;
}

static void set_variable(const char* name, struct notc_value value) {
    struct notc_value* var = find_variable(name);
    if (var) {
        if (var->type == NOTC_TYPE_STRING && var->data.string_val) {
            free(var->data.string_val);
        }
        *var = value;
        return;
    }
    
    // Create new variable
    variables = realloc(variables, sizeof(struct notc_value) * (var_count + 2));
    if (!variables) {
        panic("Failed to allocate variable");
    }
    
    variables[var_count].type = NOTC_TYPE_STRING;
    variables[var_count].data.string_val = malloc(strlen(name) + 1);
    strcpy(variables[var_count].data.string_val, name);
    
    variables[var_count + 1] = value;
    var_count++;
}

static struct notc_value parse_expression(struct notc_parser* parser) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    struct notc_token token = parser->tokens[parser->pos];
    
    // Number
    if (token.type == TOKEN_NUMBER) {
        result.type = NOTC_TYPE_INT;
        result.data.int_val = atoi(token.text);
        parser->pos++;
        return result;
    }
    
    // String
    if (token.type == TOKEN_STRING) {
        result.type = NOTC_TYPE_STRING;
        result.data.string_val = malloc(strlen(token.text) + 1);
        strcpy(result.data.string_val, token.text);
        parser->pos++;
        return result;
    }
    
    // Variable
    if (token.type == TOKEN_IDENTIFIER) {
        struct notc_value* var = find_variable(token.text);
        if (var) {
            // Return copy
            result = *var;
            if (result.type == NOTC_TYPE_STRING && result.data.string_val) {
                // Copy string
                char* new_str = malloc(strlen(result.data.string_val) + 1);
                strcpy(new_str, result.data.string_val);
                result.data.string_val = new_str;
            }
            parser->pos++;
            return result;
        }
        
        // Function call
        if (parser->tokens[parser->pos + 1].type == TOKEN_SYMBOL &&
            strcmp(parser->tokens[parser->pos + 1].text, "(") == 0) {
            return parse_function_call(parser, token.text);
        }
        
        // Unknown identifier
        parser->error = true;
        snprintf(parser->error_msg, 256, "Undefined variable: %s", token.text);
        return result;
    }
    
    parser->error = true;
    snprintf(parser->error_msg, 256, "Unexpected token: %s", token.text);
    return result;
}

static struct notc_value parse_statement(struct notc_parser* parser) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    struct notc_token token = parser->tokens[parser->pos];
    
    // Variable assignment
    if (token.type == TOKEN_IDENTIFIER &&
        parser->tokens[parser->pos + 1].type == TOKEN_SYMBOL &&
        strcmp(parser->tokens[parser->pos + 1].text, "=") == 0) {
        
        char var_name[256];
        strcpy(var_name, token.text);
        parser->pos += 2; // Skip identifier and '='
        
        struct notc_value value = parse_expression(parser);
        set_variable(var_name, value);
        return result;
    }
    
    // Function call
    if (token.type == TOKEN_IDENTIFIER &&
        parser->tokens[parser->pos + 1].type == TOKEN_SYMBOL &&
        strcmp(parser->tokens[parser->pos + 1].text, "(") == 0) {
        return parse_function_call(parser, token.text);
    }
    
    // Return statement
    if (token.type == TOKEN_KEYWORD && strcmp(token.text, "return") == 0) {
        parser->pos++;
        result = parse_expression(parser);
        return result;
    }
    
    // If statement
    if (token.type == TOKEN_KEYWORD && strcmp(token.text, "if") == 0) {
        parser->pos++;
        // Expect '('
        if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
            strcmp(parser->tokens[parser->pos].text, "(") != 0) {
            parser->error = true;
            strcpy(parser->error_msg, "Expected '(' after if");
            return result;
        }
        parser->pos++;
        
        // Parse condition
        struct notc_value cond = parse_expression(parser);
        
        // Expect ')'
        if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
            strcmp(parser->tokens[parser->pos].text, ")") != 0) {
            parser->error = true;
            strcpy(parser->error_msg, "Expected ')' after if condition");
            return result;
        }
        parser->pos++;
        
        // Parse then block
        result = parse_block(parser);
        
        // Check for else
        if (parser->tokens[parser->pos].type == TOKEN_KEYWORD &&
            strcmp(parser->tokens[parser->pos].text, "else") == 0) {
            parser->pos++;
            struct notc_value else_result = parse_block(parser);
            
            if (cond.data.int_val == 0) {
                result = else_result;
            }
        }
        
        return result;
    }
    
    // While loop
    if (token.type == TOKEN_KEYWORD && strcmp(token.text, "while") == 0) {
        parser->pos++;
        
        // Expect '('
        if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
            strcmp(parser->tokens[parser->pos].text, "(") != 0) {
            parser->error = true;
            strcpy(parser->error_msg, "Expected '(' after while");
            return result;
        }
        parser->pos++;
        
        // Parse condition
        struct notc_value cond = parse_expression(parser);
        
        // Expect ')'
        if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
            strcmp(parser->tokens[parser->pos].text, ")") != 0) {
            parser->error = true;
            strcpy(parser->error_msg, "Expected ')' after while condition");
            return result;
        }
        parser->pos++;
        
        // Parse body
        while (cond.data.int_val != 0) {
            parse_block(parser);
            // Re-evaluate condition
            struct notc_token saved_token = parser->tokens[parser->pos];
            // TODO: Re-evaluate condition properly
            break;
        }
        
        return result;
    }
    
    parser->error = true;
    snprintf(parser->error_msg, 256, "Unexpected statement: %s", token.text);
    return result;
}

static struct notc_value parse_block(struct notc_parser* parser) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    // Expect '['
    if (parser->tokens[parser->pos].type != TOKEN_SYMBOL ||
        strcmp(parser->tokens[parser->pos].text, "[") != 0) {
        parser->error = true;
        strcpy(parser->error_msg, "Expected '['");
        return result;
    }
    parser->pos++;
    
    // Parse statements until ']'
    while (parser->pos < parser->count) {
        if (parser->tokens[parser->pos].type == TOKEN_SYMBOL &&
            strcmp(parser->tokens[parser->pos].text, "]") == 0) {
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
            // Parse arguments
            struct notc_value args[16];
            size_t arg_count = 0;
            
            // Expect '('
            if (parser->tokens[parser->pos + 1].type != TOKEN_SYMBOL ||
                strcmp(parser->tokens[parser->pos + 1].text, "(") != 0) {
                parser->error = true;
                strcpy(parser->error_msg, "Expected '('");
                return result;
            }
            parser->pos += 2; // Skip identifier and '('
            
            // Parse arguments
            while (parser->pos < parser->count) {
                if (parser->tokens[parser->pos].type == TOKEN_SYMBOL &&
                    strcmp(parser->tokens[parser->pos].text, ")") == 0) {
                    parser->pos++;
                    break;
                }
                
                if (arg_count < 16) {
                    args[arg_count++] = parse_expression(parser);
                    if (parser->error) return result;
                }
                
                // Check for comma
                if (parser->tokens[parser->pos].type == TOKEN_SYMBOL &&
                    strcmp(parser->tokens[parser->pos].text, ",") == 0) {
                    parser->pos++;
                }
            }
            
            // Call the built-in function
            result = builtins[i].func(&state, args, arg_count);
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
    state.stack = malloc(sizeof(struct notc_value) * state.stack_capacity);
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
    lexer_next_char(&lexer);
    
    // Tokenize
    struct notc_token tokens[1024];
    int token_count = 0;
    
    while (token_count < 1024) {
        struct notc_token token = lexer_next_token(&lexer);
        tokens[token_count++] = token;
        if (token.type == TOKEN_EOF) break;
    }
    
    // Parse
    struct notc_parser parser = {0};
    parser.tokens = tokens;
    parser.count = token_count;
    parser.pos = 0;
    parser.error = false;
    parser.state = &state;
    
    // Find main function
    while (parser.pos < parser.count) {
        if (parser.tokens[parser.pos].type == TOKEN_IDENTIFIER &&
            strcmp(parser.tokens[parser.pos].text, "main") == 0) {
            parser.pos++;
            if (parser.tokens[parser.pos].type == TOKEN_SYMBOL &&
                strcmp(parser.tokens[parser.pos].text, ":") == 0) {
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
    // Try to read file from ISO programs directory
    char full_path[256];
    snprintf(full_path, 256, "/programs/%s", filename);
    
    int fd = vfs_open(full_path, 0);
    if (fd < 0) {
        // Try without /programs/
        fd = vfs_open(filename, 0);
        if (fd < 0) {
            snprintf(error_buffer, 256, "File not found: %s", filename);
            return false;
        }
    }
    
    // Read file
    char buffer[4096];
    size_t bytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
    vfs_close(fd);
    
    if (bytes == 0) {
        strcpy(error_buffer, "Empty file");
        return false;
    }
    
    buffer[bytes] = '\0';
    
    // Execute
    return notc_execute(buffer);
}

void notc_register_builtin(const char* name, struct notc_value (*func)(struct notc_state*, struct notc_value*, size_t)) {
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
struct notc_value notc_builtin_write(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    for (size_t i = 0; i < arg_count; i++) {
        if (args[i].type == NOTC_TYPE_STRING) {
            vga_write(args[i].data.string_val, -1, -1);
        } else if (args[i].type == NOTC_TYPE_INT) {
            char buffer[64];
            itoa(args[i].data.int_val, buffer, 10);
            vga_write(buffer, -1, -1);
        }
    }
    
    return result;
}

struct notc_value notc_builtin_read(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_STRING;
    result.data.string_val = malloc(256);
    
    // Read from keyboard (simplified)
    int i = 0;
    while (i < 255) {
        char c = wait_for_key();
        if (c == '\n' || c == '\r') break;
        if (c == '\b' && i > 0) {
            i--;
            continue;
        }
        result.data.string_val[i++] = c;
        vga_putchar(c);
    }
    result.data.string_val[i] = '\0';
    vga_putchar('\n');
    
    return result;
}

struct notc_value notc_builtin_readkey(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_INT;
    char key = wait_for_key();
    result.data.int_val = key;
    return result;
}

struct notc_value notc_builtin_listfiles(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    const char* path = "/";
    if (arg_count > 0 && args[0].type == NOTC_TYPE_STRING) {
        path = args[0].data.string_val;
    }
    
    void* dir = vfs_opendir(path);
    if (!dir) {
        return result;
    }
    
    const char* entry;
    while ((entry = vfs_readdir(dir)) != NULL) {
        vga_write(entry, -1, -1);
        vga_write("  ", -1, -1);
    }
    
    vfs_closedir(dir);
    vga_write("\n", -1, -1);
    
    return result;
}

struct notc_value notc_builtin_run(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    
    if (arg_count < 1 || args[0].type != NOTC_TYPE_STRING) {
        return result;
    }
    
    notc_run_file(args[0].data.string_val);
    return result;
}

struct notc_value notc_builtin_exit(struct notc_state* st, struct notc_value* args, size_t arg_count) {
    struct notc_value result = {0};
    result.type = NOTC_TYPE_NULL;
    st->running = false;
    return result;
}