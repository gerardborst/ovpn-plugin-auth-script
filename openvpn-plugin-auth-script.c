/*
 * auth-script OpenVPN plugin
 * 
 * Runs an external script to decide whether to authenticate a user or not.
 * Useful for checking 2FA on VPN auth attempts as it doesn't block the main
 * openvpn process, unlike passing the script to --auth-user-pass-verify.
 * 
 * Functions required to be a valid OpenVPN plugin:
 * openvpn_plugin_open_v3
 * openvpn_plugin_func_v3
 * openvpn_plugin_close_v1
 */

/* Required to use strdup */
#define __EXTENSIONS__

/********** Includes */
#include <stddef.h>
#include <errno.h>
#include <openvpn-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/********** Constants */
/* For consistency in log messages */
#define PLUGIN_NAME "auth-script"
#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#define OPENVPN_PLUGIN_VERSION_MIN 3
#define SCRIPT_NAME_IDX 0

/* Where we store our own settings/state */
struct plugin_context 
{
        plugin_log_t plugin_log;
        const char *argv[];
};

/* Handle an authentication request */
static int deferred_handler(struct plugin_context *context, 
                const char *envp[])
{
        plugin_log_t log = context->plugin_log;

        log(PLOG_DEBUG, PLUGIN_NAME, 
                        "Deferred handler using script_path=%s", 
                        context->argv[SCRIPT_NAME_IDX]);

        pid_t pid1 = fork();

        /* Parent - child failed to fork */
        if (pid1 < 0) {
                log(PLOG_ERR, PLUGIN_NAME, 
                                "fork failed: %s", strerror(errno));
                return OPENVPN_PLUGIN_FUNC_ERROR;
        }

        /* Parent - child forked successfully 
         *
         * Here we wait until that child completes before notifying OpenVPN of
         * our status.
         */
        if (pid1 > 0) {
        	waitpid(pid1, NULL, 0);
        	return OPENVPN_PLUGIN_FUNC_DEFERRED;
        }


        /* Child Control - Spin off our sucessor */
        log(PLOG_DEBUG, PLUGIN_NAME, "In child");
        pid_t pid2 = fork();

        /* Notify our parent that our child failed to fork */
        if (pid2 < 0) {
        	log(PLOG_ERR, PLUGIN_NAME, "fork failed in child: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        /* new parent: exit right away */
        if (pid2 > 0)
        	exit(EXIT_SUCCESS);

        /* Child Spawn - This process actually spawns the script */
        
        log(PLOG_DEBUG, PLUGIN_NAME, "In grand-child");

        int execve_rc = execve(context->argv[0], 
                        (char *const*)context->argv, 
                        (char *const*)envp);
        if ( execve_rc == -1 ) {
        	log(PLOG_ERR, PLUGIN_NAME, "Error trying to execve in grandchild: [%s]: [%s]", strerror(errno), context->argv[0]);
        }
        exit(EXIT_FAILURE);
}

/* We require OpenVPN Plugin API v3 */
OPENVPN_EXPORT int openvpn_plugin_min_version_required_v1()
{
        return OPENVPN_PLUGIN_VERSION_MIN;
}

/* 
 * Handle plugin initialization
 *        arguments->argv[0] is path to shared lib
 *        arguments->argv[1] is expected to be path to script
 */
OPENVPN_EXPORT int openvpn_plugin_open_v3(const int struct_version,
                struct openvpn_plugin_args_open_in const *arguments,
                struct openvpn_plugin_args_open_return *retptr)
{
        plugin_log_t log = arguments->callbacks->plugin_log;
        log(PLOG_DEBUG, PLUGIN_NAME, "FUNC: openvpn_plugin_open_v3");

        log(PLOG_NOTE, PLUGIN_NAME, "Version: [%s]", STRINGIZE_VALUE_OF(VERSION));
        log(PLOG_NOTE, PLUGIN_NAME, "Commit Hash: [%s]", STRINGIZE_VALUE_OF(COMMIT_HASH));
        log(PLOG_NOTE, PLUGIN_NAME, "Build Time: [%s]", STRINGIZE_VALUE_OF(BUILD_TIME));

        struct plugin_context *context = NULL;

        /* Safeguard on openvpn versions */
        if (struct_version < OPENVPN_PLUGINv3_STRUCTVER) {
                log(PLOG_ERR, PLUGIN_NAME, 
                                "ERROR: struct version was older than required");
                return OPENVPN_PLUGIN_FUNC_ERROR;
        }

        /* Tell OpenVPN we want to handle these calls */
        retptr->type_mask = OPENVPN_PLUGIN_MASK(
                        OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY);

        
        /*
         * Determine the size of the arguments provided so we can allocate and
         * argv array of appropriate length.
         */
        size_t arg_size = 0;
        for (int arg_idx = 1; arguments->argv[arg_idx]; arg_idx++)
                arg_size += strlen(arguments->argv[arg_idx]);


        /* 
         * Plugin init will fail unless we create a handler, so we'll store our
         * script path and it's arguments there as we have to create it anyway. 
         */
        context = (struct plugin_context *) malloc(
                        sizeof(struct plugin_context) + arg_size);
        memset(context, 0, sizeof(struct plugin_context) + arg_size);
        context->plugin_log = log;


        /* 
         * Check we've been handed a script path to call
         * This comes directly from openvpn config file:
         *           plugin /path/to/auth.so /path/to/auth/script.sh
         *
         * IDX 0 should correspond to the library, IDX 1 should be the
         * script, and any subsequent entries should be arguments to the script.
         *
         * Note that if arg_size is 0 no script argument was included.
         */
        if (arg_size > 0) {
                memcpy(&context->argv, &arguments->argv[1], arg_size);

                log(PLOG_DEBUG, PLUGIN_NAME, 
                                "script_path=%s", 
                                context->argv[SCRIPT_NAME_IDX]);
        } else {
                free(context);
                log(PLOG_ERR, PLUGIN_NAME, 
                                "ERROR: no script_path specified in config file");
                return OPENVPN_PLUGIN_FUNC_ERROR;
        }        

        /* Pass state back to OpenVPN so we get handed it back later */
        retptr->handle = (openvpn_plugin_handle_t) context;

        log(PLOG_DEBUG, PLUGIN_NAME, "plugin initialized successfully");

        return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

/* Called when we need to handle OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY calls */
OPENVPN_EXPORT int openvpn_plugin_func_v3(const int struct_version,
                struct openvpn_plugin_args_func_in const *arguments,
                struct openvpn_plugin_args_func_return *retptr)
{
        (void)retptr; /* Squish -Wunused-parameter warning */
        struct plugin_context *context = 
                (struct plugin_context *) arguments->handle;
        plugin_log_t log = context->plugin_log;

        log(PLOG_DEBUG, PLUGIN_NAME, "FUNC: openvpn_plugin_func_v3");

        /* Safeguard on openvpn versions */
        if (struct_version < OPENVPN_PLUGINv3_STRUCTVER) {
                log(PLOG_ERR, PLUGIN_NAME, 
                                "ERROR: struct version was older than required");
                return OPENVPN_PLUGIN_FUNC_ERROR;
        }

        if(arguments->type == OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY) {
                log(PLOG_DEBUG, PLUGIN_NAME,
                                "Handling auth with deferred script");
                return deferred_handler(context, arguments->envp);
        } else
                return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

OPENVPN_EXPORT void openvpn_plugin_close_v1(openvpn_plugin_handle_t handle)
{
        struct plugin_context *context = (struct plugin_context *) handle;
        free(context);
}
