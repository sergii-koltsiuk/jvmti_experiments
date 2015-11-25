#ifndef _INCLUDE_JVM_AGENT_CONSTANTS_H_
#define _INCLUDE_JVM_AGENT_CONSTANTS_H_

#define MTRACE_class        bridge          /* Name of class we are using */
#define MTRACE_entry        method_entry    /* Name of java entry method */
#define MTRACE_exit         method_exit     /* Name of java exit method */
#define MTRACE_native_entry _method_entry   /* Name of java entry native */
#define MTRACE_native_exit  _method_exit    /* Name of java exit native */
#define MTRACE_engaged      engaged         /* Name of java static field */

/* C macros to create strings from tokens */
#define _STRING(s) #s
#define STRING(s) _STRING(s)


#define MAX_TOKEN_LENGTH        16
#define MAX_THREAD_NAME_LENGTH  512
#define MAX_METHOD_NAME_LENGTH  1024

#endif // _INCLUDE_JVM_AGENT_CONSTANTS_H_