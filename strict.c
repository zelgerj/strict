/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "Zend/zend_extensions.h"
#include "php_strict.h"

#if PHP_VERSION_ID >= 70000
static inline int php_strict_handler_recv(ZEND_OPCODE_HANDLER_ARGS) {
    const zend_function *function = EX(func);

    if (function->common.fn_flags & ZEND_ACC_HAS_TYPE_HINTS) {
        const zend_op       *opline   = EX(opline);
        uint32_t             arg      = opline->op1.num;
        zval                *param    = EX_VAR(opline->result.var);
        zend_arg_info       *info     = &function->common.arg_info[arg-1];

        switch (info->type_hint) {
            case _IS_BOOL: {
                if (Z_TYPE_P(param) != IS_TRUE && Z_TYPE_P(param) != IS_FALSE) {
                    zend_error(E_RECOVERABLE_ERROR, 
                        "Argument %d passed to %s%s%s must be boolean, %s given",
                        arg, 
                        function->common.scope ? function->common.scope->name->val : "",
                        function->common.scope ? "::" : "",
                        function->common.function_name->val,
                        zend_get_type_by_const(Z_TYPE_P(param)));
                } else EX(opline)++;
                return ZEND_USER_OPCODE_CONTINUE;
            } break;

            case IS_RESOURCE:
            case IS_STRING:
            case IS_DOUBLE:
            case IS_LONG: {
                if (info->type_hint != Z_TYPE_P(param)) {
                    zend_error(E_RECOVERABLE_ERROR, 
                        "Argument %d passed to %s%s%s must be %s, %s given",
                        arg, 
                        function->common.scope ? function->common.scope->name->val : "",
                        function->common.scope ? "::" : "",
                        function->common.function_name->val,
                        zend_get_type_by_const(info->type_hint),
                        zend_get_type_by_const(Z_TYPE_P(param)));
                } else EX(opline)++;
                return ZEND_USER_OPCODE_CONTINUE;
            } break;
        }
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

static inline int php_strict_handler_variadic(ZEND_OPCODE_HANDLER_ARGS) {
    const zend_function *function = EX(func);
    const zend_op       *opline   = EX(opline);
    uint32_t             arg      = opline->op1.num;
    uint32_t             args     = EX(num_args);
    zval                *params   = EX_VAR(opline->result.var);
    zend_arg_info       *info     = &function->common.arg_info[arg-1];
    
    if (arg <= args) {
        zval *param = 
            EX_VAR_NUM(EX(func)->op_array.last_var + EX(func)->op_array.T);
        
        if (function->common.fn_flags & ZEND_ACC_HAS_TYPE_HINTS) {
            switch (info->type_hint) {
                case _IS_BOOL: {
                    array_init_size(params, args - arg + 1);
                    do {
                        if (Z_TYPE_P(param) != IS_TRUE && Z_TYPE_P(param) != IS_FALSE) {
                            zend_error(E_RECOVERABLE_ERROR, 
                                "Argument %d passed to %s%s%s must be boolean, %s given",
                                arg, 
                                function->common.scope ? function->common.scope->name->val : "",
                                function->common.scope ? "::" : "",
                                function->common.function_name->val,
                                zend_get_type_by_const(Z_TYPE_P(param)));
                        }
                        zend_hash_next_index_insert(Z_ARRVAL_P(params), param);
                        if (Z_REFCOUNTED_P(param)) Z_ADDREF_P(param);
                        param++;
                    } while (++arg <= args);
                    
                    EX(opline)++;
                    return ZEND_USER_OPCODE_CONTINUE;
                } break;

                case IS_RESOURCE:
                case IS_STRING:
                case IS_DOUBLE:
                case IS_LONG: {
                    array_init_size(params, args - arg + 1);
                    do {
                        if (Z_TYPE_P(param) != info->type_hint) {
                            zend_error(E_RECOVERABLE_ERROR, 
                                "Argument %d passed to %s%s%s must be %s, %s given",
                                arg, 
                                function->common.scope ? function->common.scope->name->val : "",
                                function->common.scope ? "::" : "",
                                function->common.function_name->val,
                                zend_get_type_by_const(info->type_hint),
                                zend_get_type_by_const(Z_TYPE_P(param)));
                        }
                        zend_hash_next_index_insert(Z_ARRVAL_P(params), param);
                        if (Z_REFCOUNTED_P(param)) Z_ADDREF_P(param);
                        param++;
                    } while (++arg <= args);
                    
                    EX(opline)++;
                    return ZEND_USER_OPCODE_CONTINUE;
                } break;   
            }   
        }
    }
    
    return ZEND_USER_OPCODE_DISPATCH;
}
#else
#ifndef EX_CV_NUM
#define EX_CV_NUM(execute_data, offset) (execute_data->CVs + offset)
static inline zend_uint zend_vm_stack_get_args_count(TSRMLS_D) {
    if (EG(current_execute_data)) {
        void **p = EG(current_execute_data)->function_state.arguments;
        return (int) (zend_uintptr_t) *p;
    } else return 0;
}
#endif

static zend_never_inline zval **zend_lookup_cv(zval ***ptr, zend_uint var TSRMLS_DC)
{
    zend_compiled_variable *cv = &EG(active_op_array)->vars[var];

    if (!EG(active_symbol_table)) {
        Z_ADDREF(EG(uninitialized_zval));
        *ptr = (zval**)EX_CV_NUM(EG(current_execute_data), EG(active_op_array)->last_var + var);
        **ptr = &EG(uninitialized_zval);
    } else if (zend_hash_quick_find(EG(active_symbol_table), cv->name, cv->name_len+1, cv->hash_value, (void **)ptr)==FAILURE) {
        Z_ADDREF(EG(uninitialized_zval));
        zend_hash_quick_update(EG(active_symbol_table), cv->name, cv->name_len+1, cv->hash_value, &EG(uninitialized_zval_ptr), sizeof(zval *), (void **)ptr);
    }
    return *ptr;
}

static inline int php_strict_handler_recv(ZEND_OPCODE_HANDLER_ARGS) {
#undef EX
#define EX(e) execute_data->e
    const zend_function *function = EX(function_state).function;
    const zend_op *opline = EX(opline);
    zend_uint      arg = opline->op1.num;
    zval          **param = zend_vm_stack_get_arg(arg TSRMLS_CC);

    if (param != NULL && 
        function->common.arg_info && 
        arg <= function->common.num_args) {
        zend_arg_info *info = &function->common.arg_info[arg-1];        
        zval **var, ***ptr;
        
        if (info && info->type_hint != IS_NULL && info->type_hint != Z_TYPE_PP(param)) {
            zend_error(E_RECOVERABLE_ERROR, 
                "Argument %d passed to %s%s%s must be %s, %s given",
                arg,
                function->common.scope ? function->common.scope->name : "",
                function->common.scope ? "::" : "",
                function->common.function_name,
                zend_get_type_by_const(info->type_hint),
                zend_get_type_by_const(Z_TYPE_PP(param)));
        }
        
        ptr = EX_CV_NUM(execute_data, opline->result.var);
        if (*ptr == NULL) {
            var = zend_lookup_cv
                (ptr, opline->result.var TSRMLS_CC);
        } else var = *ptr;
        
        Z_DELREF_PP(var);
        *var = *param;
        Z_ADDREF_PP(var);
        
        EX(opline)++;
        return ZEND_USER_OPCODE_CONTINUE;
    }

#undef EX
    return ZEND_USER_OPCODE_DISPATCH;

}
static inline int php_strict_handler_variadic(ZEND_OPCODE_HANDLER_ARGS) {
#undef EX
#define EX(e) execute_data->e
    const zend_function *function = EX(function_state).function;
    const zend_op    *opline = EX(opline);
    zend_uint         arg    = opline->op1.num;
    zend_uint         args   = zend_vm_stack_get_args_count(TSRMLS_C);
    zval             ***ptr,
                      **var, 
                      *params;
    zend_arg_info     *info  = NULL;
    
    ptr = EX_CV_NUM(execute_data, opline->result.var);
    if (*ptr == NULL) {
        var = zend_lookup_cv
            (ptr, opline->result.var TSRMLS_CC);
    } else var = *ptr;
    
    Z_DELREF_PP(var);
    MAKE_STD_ZVAL(params);
    *var = params;
    
    if (arg <= args) {
        array_init_size(params, args - arg + 1);
    } else array_init(params);
    
    if (function->common.arg_info) {
        info = &function->common.arg_info[arg-1];
    }
    
    for (; arg <= args; ++arg) {
        zval **param = zend_vm_stack_get_arg(arg TSRMLS_CC);
        
        if (info && info->type_hint != IS_NULL && Z_TYPE_PP(param) != info->type_hint) {
             zend_error(E_RECOVERABLE_ERROR,
                "Argument %d passed to %s%s%s must be %s, %s given",
                arg, 
                function->common.scope ? function->common.scope->name : "",
                function->common.scope ? "::" : "",
                function->common.function_name,
                zend_get_type_by_const(info->type_hint),
                zend_get_type_by_const(Z_TYPE_PP(param)));
        }
        
        zend_hash_next_index_insert(Z_ARRVAL_P(params), param, sizeof(zval*), NULL);
        Z_ADDREF_PP(param);
    }
    
    EX(opline)++;
#undef EX
    return ZEND_USER_OPCODE_CONTINUE;
}
#endif

static inline int zend_strict_startup(zend_extension *extension) {
    TSRMLS_FETCH();
#if PHP_VERSION_ID >= 70000
    zend_startup_module(&strict_module_entry TSRMLS_CC);
#else
    zend_startup_module(&strict_module_entry);
#endif
    return SUCCESS;
}

static inline void zend_strict_compile(zend_op_array *ops) {
    TSRMLS_FETCH();
    
#if PHP_VERSION_ID >= 70000
    if (ops->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) {
#else
    if (ops->arg_info) {
#endif
        zend_arg_info *hint = ops->arg_info,
                      *end  = &ops->arg_info[ops->num_args];

#define IS_TYPE(n) \
    (zend_binary_strncasecmp(hint->class_name, hint->class_name_len, ZEND_STRL(n), sizeof(n)-1) == SUCCESS)

#define SET_TYPE(n) \
    hint->type_hint = n; \
    hint++; \
    continue

        do {
            if (IS_TYPE("integer") || IS_TYPE("int")) {
                SET_TYPE(IS_LONG);
            }
            
            if (IS_TYPE("double") || IS_TYPE("float")) {
                SET_TYPE(IS_DOUBLE);
            }
            
            if (IS_TYPE("string")) {
                SET_TYPE(IS_STRING);
            }
            
            if (IS_TYPE("boolean") || IS_TYPE("bool")) {
#if PHP_VERSION_ID >= 70000
                SET_TYPE(_IS_BOOL);
#else
                SET_TYPE(IS_BOOL);
#endif
            }
            
            if (IS_TYPE("resource")) {
                SET_TYPE(IS_RESOURCE);
            }

            hint++;
        } while (hint < end);
    }
#undef IS_TYPE
#undef SET_TYPE
}

#ifndef ZEND_EXT_API
#define ZEND_EXT_API ZEND_DLEXPORT
#endif

ZEND_EXTENSION();

ZEND_EXT_API zend_extension zend_extension_entry = {
	PHP_STRICT_EXTNAME,
    PHP_STRICT_VERSION,
    PHP_STRICT_AUTHOR,
    PHP_STRICT_URL,
    PHP_STRICT_COPY,
    zend_strict_startup,
    NULL, /* shutdown_func_t */
    NULL, /* activate_func_t */
    NULL, /* deactivate_func_t */
    NULL, /* message_handler_func_t */
    zend_strict_compile, /* op_array_handler_func_t */
    NULL, /* statement_handler_func_t */
    NULL, /* fcall_begin_handler_func_t */
    NULL, /* fcall_end_handler_func_t */
    NULL, /* op_array_ctor_func_t */
    NULL, /* op_array_dtor_func_t */
    STANDARD_ZEND_EXTENSION_PROPERTIES
};

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(strict) {
    if (!zend_get_extension("strict")) {
        zend_extension_entry.startup = NULL;
#if PHP_VERSION_ID >= 70000
        zend_register_extension(
            &zend_extension_entry, NULL TSRMLS_CC);
#else
        zend_register_extension(
            &zend_extension_entry, NULL);
#endif
    }

    zend_set_user_opcode_handler(ZEND_RECV,           php_strict_handler_recv);
#ifdef ZEND_RECV_VARIADIC
    zend_set_user_opcode_handler(ZEND_RECV_VARIADIC,  php_strict_handler_variadic);
#endif

	return SUCCESS;
} /* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(strict) {
	php_info_print_table_start();
	php_info_print_table_header(2, "strict support", "enabled");
	php_info_print_table_end();
} /* }}} */

/* {{{ strict_module_entry */
zend_module_entry strict_module_entry = {
	STANDARD_MODULE_HEADER,
	"strict",
	NULL,
	PHP_MINIT(strict),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(strict),
	PHP_STRICT_VERSION,
	STANDARD_MODULE_PROPERTIES
}; /* }}} */

#ifdef COMPILE_DL_STRICT
ZEND_GET_MODULE(strict)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
