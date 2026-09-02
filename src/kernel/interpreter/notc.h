/**
 * CardboardOS - NotC Interpreter Header
 */

#ifndef NOTC_H
#define NOTC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// NotC value types
enum notc_type {
    NOTC_TYPE_NULL,
    NOTC_TYPE_INT,
    NOTC_TYPE_FLOAT,
    NOTC_TYPE_STRING,
    NOTC_TYPE_ARRAY,
    NOTC_TYPE_FUNCTION,
    NOTC_TYPE_BUILTIN
};

// NotC value structure
struct notc_value {
    enum notc_type type;
    union {
        int64_t int_val;
        double float_val;
        char* string_val;
        struct {
            struct notc_value* items;
            size_t count;
        } array;
        void* function_ptr;
    } data;
};

// NotC interpreter state
struct notc_state {
    struct notc_value* stack;
    size_t stack_size;
    size_t stack_capacity;
    struct notc_value* variables;
    size_t var_count;
    bool running;
    bool error;
};

// Initialize NotC interpreter
void notc_init(void);

// Execute NotC code
bool notc_execute(const char* source);

// Execute NotC file
bool notc_run_file(const char* filename);

// Register a built-in function
void notc_register_builtin(const char* name, struct notc_value (*func)(struct notc_state*, struct notc_value*));

// Get last error
const char* notc_get_error(void);

// NotC built-in functions
struct notc_value notc_builtin_write(struct notc_state* state, struct notc_value* args);
struct notc_value notc_builtin_read(struct notc_state* state, struct notc_value* args);
struct notc_value notc_builtin_readkey(struct notc_state* state, struct notc_value* args);
struct notc_value notc_builtin_listfiles(struct notc_state* state, struct notc_value* args);
struct notc_value notc_builtin_run(struct notc_state* state, struct notc_value* args);
struct notc_value notc_builtin_exit(struct notc_state* state, struct notc_value* args);

#endif // NOTC_H