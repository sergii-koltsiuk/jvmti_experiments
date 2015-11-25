#include "java_crw_demo.h"
#include "agent_util.h"

#include <jvmti.h>
#include <jni.h>

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <queue>


#include <winsock2.h>
#include <windows.h>
  
#pragma comment(lib,"ws2_32.lib") //Winsock Library


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


 // Data structure to hold method and class information in agent 

typedef struct MethodInfo
{
    const char *name;          /* Method name */
    const char *signature;     /* Method signature */
    int         calls;         /* Method call count */
    int         returns;       /* Method return count */
} MethodInfo;

typedef struct ClassInfo
{
    const char *name;          /* Class name */
    int         mcount;        /* Method count */
    MethodInfo *methods;       /* Method information */
    int         calls;         /* Method call count for this class */
} ClassInfo;

typedef struct NetworkServerInfo
{
    HANDLE thread;
    bool working;
    SOCKET listen_socket; 
    SOCKET client_sock;
	std::queue<std::string> queue;
} NetworkServerInfo;

 // Global agent data structure                  
typedef struct 
{
    // JVMTI Environment 
    jvmtiEnv *jvmti;
    jboolean vm_is_dead;
    jboolean vm_is_started;

    // Data access Lock 
    jrawMonitorID lock;

    // Options 
    char *include;

     // ClassInfo Table 
    ClassInfo      *classes;
    jint            ccount;

    // Network Server Data
    NetworkServerInfo *server;
} GlobalAgentData;
                  
static GlobalAgentData *gdata;


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static void enter_critical_section(jvmtiEnv *jvmti);
static void exit_critical_section(jvmtiEnv *jvmti);
static void mnum_callbacks(unsigned cnum, const char **names, const char**sigs, int mcount);
static void get_thread_name(jvmtiEnv *jvmti, jthread thread, char *tname, int maxlen);


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static int call_stack_deep = 0;
static void MTRACE_native_entry(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	enter_critical_section(gdata->jvmti); 
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!gdata->vm_is_dead)
		{
			ClassInfo  *cp;
			MethodInfo *mp;

			if (cnum >= gdata->ccount)
			{
				fatal_error("ERROR: Class number out of range\n");
			}

			cp = gdata->classes + cnum;
			if (mnum >= cp->mcount)
			{
				fatal_error("ERROR: Method number out of range\n");
			}

			mp = cp->methods + mnum;

			if (interested((char*)cp->name, (char*)mp->name,
				gdata->include, NULL))
			{
				//mp->calls++;
				//cp->calls++;
                for (int i = 0; i < call_stack_deep; ++i) stdout_message(" ");
				stdout_message("enter: %s:%s\n", cp->name, mp->name);
                ++call_stack_deep;
			}
		}
	} 
	exit_critical_section(gdata->jvmti);
}

static void MTRACE_native_exit(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	enter_critical_section(gdata->jvmti); 
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!gdata->vm_is_dead)
		{
			ClassInfo  *cp;
			MethodInfo *mp;

			if (cnum >= gdata->ccount)
			{
				fatal_error("ERROR: Class number out of range\n");
			}

			cp = gdata->classes + cnum;
			if (mnum >= cp->mcount)
			{
				fatal_error("ERROR: Method number out of range\n");
			}

			mp = cp->methods + mnum;
			if (interested((char*)cp->name, (char*)mp->name, gdata->include, NULL))
			{
				//mp->returns++;
                assert(call_stack_deep != 0);
                --call_stack_deep;
                for (int i = 0; i < call_stack_deep; ++i) stdout_message(" ");
				stdout_message("exit: %s:%s\n", cp->name, mp->name);
			}
		}
	} 
    exit_critical_section(gdata->jvmti);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

 // JVMTI_EVENT_VM_START 
static void JNICALL cbVMStart(jvmtiEnv *jvmti, JNIEnv *env)
{    
	stdout_message("VMStart\n");

	gdata->vm_is_started = JNI_TRUE;
}

// JVMTI_EVENT_VM_INIT 
static void JNICALL cbVMInit(jvmtiEnv *jvmti, JNIEnv* env, jthread thread)
{
	enter_critical_section(jvmti);
	{
		jclass   klass;
		jfieldID field;
		int      rc;

		// Java Native Methods for class 
		static JNINativeMethod registry[2] = {
			{ STRING(MTRACE_native_entry), "(Ljava/lang/Object;II)V",
			(void*)&MTRACE_native_entry },
			{ STRING(MTRACE_native_exit), "(Ljava/lang/Object;II)V",
			(void*)&MTRACE_native_exit }
		};

		

		// Register Natives for class whose methods we use 
		klass = (*env).FindClass(STRING(MTRACE_class));
		if (klass == NULL)
		{
			fatal_error("ERROR: JNI: Cannot find %s with FindClass\n",
				STRING(MTRACE_class));
		}

		rc = (*env).RegisterNatives(klass, registry, 2);
		if (rc != 0)
		{
			fatal_error("ERROR: JNI: Cannot register native methods for %s\n", STRING(MTRACE_class));
		}

		// Engage calls. 
		field = (*env).GetStaticFieldID(klass, STRING(MTRACE_engaged), "I");
		if (field == NULL)
		{
			fatal_error("ERROR: JNI: Cannot get field from %s\n", STRING(MTRACE_class));
		}

		(*env).SetStaticIntField(klass, field, 1);

		/////////////////////////////////////////////
		/////////////////////////////////////////////
		/////////////////////////////////////////////

        char  tname[MAX_THREAD_NAME_LENGTH];

        static jvmtiEvent events[] = { JVMTI_EVENT_THREAD_START, JVMTI_EVENT_THREAD_END };
        int i;

         // The VM has started. 
        get_thread_name(jvmti, thread, tname, sizeof(tname));
        stdout_message("VMInit %s\n", tname);

        // The VM is now initialized, at this time we make our requests
        // for additional events.
         

        for( i=0; i < (int)(sizeof(events)/sizeof(jvmtiEvent)); i++) 
        {
            jvmtiError error;

             // Setup event  notification modes 
            error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE,
                                  events[i], (jthread)NULL);
            check_jvmti_error(jvmti, error, "Cannot set event notification");
        }
    } 
    exit_critical_section(jvmti);
}

// JVMTI_EVENT_VM_DEATH 
static void JNICALL cbVMDeath(jvmtiEnv *jvmti, JNIEnv* env)
{
    enter_critical_section(jvmti); 
    {
        jclass   klass;
        jfieldID field;

        // The VM has died. 
        stdout_message("VMDeath\n");

        // Disengage calls in MTRACE_class. 
        klass = (*env).FindClass(STRING(MTRACE_class));
        if ( klass == NULL ) 
        {
            fatal_error("ERROR: JNI: Cannot find %s with FindClass\n", STRING(MTRACE_class));
        }

        field = (*env).GetStaticFieldID(klass, STRING(MTRACE_engaged), "I");
        if ( field == NULL ) 
        {
            fatal_error("ERROR: JNI: Cannot get field from %s\n", STRING(MTRACE_class));
        }

        (*env).SetStaticIntField(klass, field, 0);

        gdata->vm_is_dead = JNI_TRUE;

    } 
    exit_critical_section(jvmti);
}

// JVMTI_EVENT_THREAD_START 
static void JNICALL cbThreadStart(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
    enter_critical_section(jvmti); 
    {
        // It's possible we get here right after VmDeath event, be careful 
        if ( !gdata->vm_is_dead )
         {
            char  tname[MAX_THREAD_NAME_LENGTH];

            get_thread_name(jvmti, thread, tname, sizeof(tname));
            stdout_message("ThreadStart %s\n", tname);
        }
    } 
    exit_critical_section(jvmti);
}

// JVMTI_EVENT_THREAD_END
static void JNICALL cbThreadEnd(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
    enter_critical_section(jvmti); 
    {
        // It's possible we get here right after VmDeath event, be careful 
        if ( !gdata->vm_is_dead )
        {
            char  tname[MAX_THREAD_NAME_LENGTH];

            get_thread_name(jvmti, thread, tname, sizeof(tname));
            stdout_message("ThreadEnd %s\n", tname);
        }
    } 
    exit_critical_section(jvmti);
}

// JVMTI_EVENT_CLASS_FILE_LOAD_HOOK 
static void JNICALL cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv* env,
    jclass class_being_redefined, jobject loader,
    const char* name, jobject protection_domain,
    jint class_data_len, const unsigned char* class_data,
    jint* new_class_data_len, unsigned char** new_class_data)
{
    enter_critical_section(jvmti); 
    {
        // It's possible we get here right after VmDeath event, be careful 
        if ( !gdata->vm_is_dead ) 
        {
                const char *classname;

            /* Name could be NULL */
            if ( name == NULL ) 
            {
                classname = java_crw_demo_classname(class_data, class_data_len, NULL);
                if ( classname == NULL ) 
                {
                    fatal_error("ERROR: No classname inside classfile\n");
                }
            } 
            else
            {
                classname = strdup(name);
                if ( classname == NULL ) 
                {
                    fatal_error("ERROR: Out of malloc memory\n");
                }
            }

            *new_class_data_len = 0;
            *new_class_data     = NULL;

            if (interested((char*)classname, "", gdata->include, NULL))
            {
				stdout_message("Class load %s\n", classname);

                jint           cnum;
                int            system_class;
                unsigned char *new_image;
                long           new_length;
                ClassInfo     *cp;

                /* Get unique number for every class file image loaded */
                cnum = gdata->ccount++;

                /* Save away class information */
                if ( gdata->classes == NULL ) 
                {
                    gdata->classes = (ClassInfo*)malloc(gdata->ccount*sizeof(ClassInfo));
                }
                else 
                {
                    gdata->classes = (ClassInfo*)realloc((void*)gdata->classes, gdata->ccount*sizeof(ClassInfo));
                }

                if ( gdata->classes == NULL ) 
                {
                    fatal_error("ERROR: Out of malloc memory\n");
                }

                cp = gdata->classes + cnum;
                cp->name = (const char *)strdup(classname);
                if ( cp->name == NULL ) 
                {
                    fatal_error("ERROR: Out of malloc memory\n");
                }

                cp->calls    = 0;
                cp->mcount   = 0;
                cp->methods  = NULL;

                /* Is it a system class? If the class load is before VmStart
                 *   then we will consider it a system class that should
                 *   be treated carefully. (See java_crw_demo)
                 */
                system_class = 0;
                if ( !gdata->vm_is_started ) 
                {
                    system_class = 1;
                }

                /* Call the class file reader/write demo code */
                java_crw_demo(cnum,
                    classname,
                    class_data,
                    class_data_len,
                    system_class,
                    STRING(MTRACE_class), "L" STRING(MTRACE_class) ";",
                    STRING(MTRACE_entry), "(II)V",
                    STRING(MTRACE_exit), "(II)V",
                    NULL, NULL,
                    NULL, NULL,
                    &new_image,
                    &new_length,
                    NULL,
                    &mnum_callbacks);

				/* If we got back a new class image, return it back as "the"
				*   new class image. This must be JVMTI Allocate space.
				*/
				if (new_length > 0)
				{
					unsigned char *jvmti_space;

					jvmti_space = (unsigned char *)allocate(jvmti, (jint)new_length);
					(void)memcpy((void*)jvmti_space, (void*)new_image, (int)new_length);
					*new_class_data_len = (jint)new_length;
					*new_class_data = jvmti_space; /* VM will deallocate */

					stdout_message("Class hooked %s\n", classname);
				}

				/* Always free up the space we get from java_crw_demo() */
				if (new_image != NULL)
				{
					(void)free((void*)new_image); /* Free malloc() space with free() */
				}
            }   

			(void)free((void*)classname);
        }
    }
    exit_critical_section(jvmti);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

/* Callback from java_crw_demo() that gives us mnum mappings */
static void mnum_callbacks(unsigned cnum, const char **names, const char**sigs, int mcount)
{
    ClassInfo *cp;
    int mnum;

    if ( cnum >= (unsigned)gdata->ccount ) 
	{
        fatal_error("ERROR: Class number out of range\n");
    }

    if ( mcount == 0 ) 
	{
        return;
    }

    cp = gdata->classes + (int)cnum;
    cp->calls = 0;
    cp->mcount = mcount;
    cp->methods = (MethodInfo*)calloc(mcount, sizeof(MethodInfo));
    if ( cp->methods == NULL ) {
        fatal_error("ERROR: Out of malloc memory\n");
    }

    for ( mnum = 0 ; mnum < mcount ; mnum++ ) {
        MethodInfo *mp;

        mp            = cp->methods + mnum;
        mp->name      = (const char *)strdup(names[mnum]);
        if ( mp->name == NULL ) {
            fatal_error("ERROR: Out of malloc memory\n");
        }
        mp->signature = (const char *)strdup(sigs[mnum]);
        if ( mp->signature == NULL ) {
            fatal_error("ERROR: Out of malloc memory\n");
        }
    }
}

/* Get a name for a jthread */
static void get_thread_name(jvmtiEnv *jvmti, jthread thread, char *tname, int maxlen)
{
    jvmtiThreadInfo info;
    jvmtiError      error;

    /* Make sure the stack variables are garbage free */
    (void)memset(&info,0, sizeof(info));

    /* Assume the name is unknown for now */
    (void)strcpy(tname, "Unknown");

    /* Get the thread information, which includes the name */
    error = (*jvmti).GetThreadInfo(thread, &info);
    check_jvmti_error(jvmti, error, "Cannot get thread info");

    /* The thread might not have a name, be careful here. */
    if ( info.name != NULL ) {
        int len;

        /* Copy the thread name into tname if it will fit */
        len = (int)strlen(info.name);
        if ( len < maxlen ) {
            (void)strcpy(tname, info.name);
        }

        /* Every string allocated by JVMTI needs to be freed */
        deallocate(jvmti, (void*)info.name);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static void parse_agent_options(char *options)
{
    stdout_message("agent options: ");
    stdout_message(options==NULL? "NULL":options);
    stdout_message("\n");

    char token[MAX_TOKEN_LENGTH];
    char *next;

     // Parse options and set flags in gdata 
    if ( options==NULL ) 
    {
        return;
    }

     // Get the first token from the options string. 
    next = get_token(options, " ,=", token, sizeof(token));

     // While not at the end of the options string, process this option. 
    while ( next != NULL )
    {
        if ( strcmp(token,"help")==0 ) 
        {
            stdout_message("The method_call_trace JVMTI demo agent\n");
            stdout_message("\n");
            stdout_message(" java -agent:method_call_trace[=options] ...\n");
            stdout_message("\n");
            stdout_message("The options are comma separated:\n");
            stdout_message("\t help\t\t\t Print help information\n");
            stdout_message("\t include=item\t\t Only these classes/methods\n");
            stdout_message("\n");
            stdout_message("item\t Qualified class and/or method names\n");
            stdout_message("\n");
            exit(0);
        } 
        else if ( strcmp(token,"include")==0 ) 
        {
            int used;
            int maxlen;

            maxlen = MAX_METHOD_NAME_LENGTH;
            if ( gdata->include == NULL ) 
            {
                gdata->include = (char*)calloc(maxlen+1, 1);
                used = 0;
            } 
            else 
            {
                used  = (int)strlen(gdata->include);
                gdata->include[used++] = ',';
                gdata->include[used] = 0;
                gdata->include = (char*) realloc((void*)gdata->include, used+maxlen+1);
            }
            if ( gdata->include == NULL ) 
            {
                fatal_error("ERROR: Out of malloc memory\n");
            }

             // Add this item to the list 
            next = get_token(next, ",=", gdata->include+used, maxlen);
             // Check for token scan error 
            if ( next==NULL )
            {
                fatal_error("ERROR: include option error\n");
            }
        } 
        else if ( token[0] != 0 ) 
        {
             // We got a non-empty token and we don't know what it is. 
            fatal_error("ERROR: Unknown option: %s\n", token);
        }

         // Get the next token (returns NULL if there are no more) 
        next = get_token(next, ",=", token, sizeof(token));
    }
}

static void init_global_data()
{
    static GlobalAgentData data;                 
    (void)memset((void*)&data, 0, sizeof(data));
    gdata = &data;
}

static void init_jvmti(JavaVM *jvm)
{
    static jvmtiEnv *jvmti = NULL;
    jint res;

    res = (*jvm).GetEnv((void **)&jvmti, JVMTI_VERSION_1);
    if (res != JNI_OK) 
    {
        // This means that the VM was unable to obtain this version of the
        // JVMTI interface, this is a fatal error.         
        fatal_error("ERROR: Unable to access JVMTI Version 1 (0x%x),"
                " is your JDK a 5.0 or newer version?"
                " JNIEnv's GetEnv() returned %d\n",
               JVMTI_VERSION_1, res);
    }

    gdata->jvmti = jvmti;
}

static void init_capabilities()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    static jvmtiCapabilities capabilities;
    (void)memset(&capabilities, 0, sizeof(jvmtiCapabilities));
    capabilities.can_generate_all_class_hook_events  = 1;
    error = (*jvmti).AddCapabilities(&capabilities);             
    check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");
}

static void set_event_notifications()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    /* At first the only initial events we are interested in are VM
     *   initialization, VM death, and Class File Loads.
     *   Once the VM is initialized we will request more events.
     */
    error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_START, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");

    error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");

    error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");

    error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
}

static void set_event_callbacks()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    jvmtiEventCallbacks callbacks;               
    (void)memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMStart = &cbVMStart; // JVMTI_EVENT_VM_START 
    callbacks.VMInit = &cbVMInit; // JVMTI_EVENT_VM_INIT 
    callbacks.VMDeath = &cbVMDeath; // JVMTI_EVENT_VM_DEATH 
    callbacks.ClassFileLoadHook = &cbClassFileLoadHook; // JVMTI_EVENT_CLASS_FILE_LOAD_HOOK     
    callbacks.ThreadStart = &cbThreadStart; // JVMTI_EVENT_THREAD_START 
    callbacks.ThreadEnd = &cbThreadEnd; // JVMTI_EVENT_THREAD_END 
    error = (*jvmti).SetEventCallbacks(&callbacks,(jint)sizeof(callbacks));
    check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");
}

static void init_lock()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    error = (*jvmti).CreateRawMonitor("agent data", &(gdata->lock));
        check_jvmti_error(jvmti, error, "Cannot create raw monitor");
}

static void enter_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error = (*jvmti).RawMonitorEnter(gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
}

static void exit_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error = (*jvmti).RawMonitorExit(gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
}

static void init_socket()
{
    WSADATA wsa;
    NetworkServerInfo *server = gdata->server;
    assert(server != NULL);

    struct sockaddr_in server_addr;
  
    stdout_message("\nInitialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        fatal_error("Failed. Error Code : %d\n",WSAGetLastError());
    }
      
    stdout_message("Initialised.\n");
      
    //Create a socket
	if ((server->listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        fatal_error("Could not create socket : %d\n" , WSAGetLastError());
    }
  
    stdout_message("Socket created.\n");
      
    //Prepare the sockaddr_in structure
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(8888);
      
    //Bind
	if (bind(server->listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        fatal_error("Bind failed with error code : %d\n" , WSAGetLastError());
    }
      
    stdout_message("Bind done\n");    
}

static void finit_socket()
{
    NetworkServerInfo *server = gdata->server;

    if (server != NULL)
    {
        if (server->listen_socket != INVALID_SOCKET)
        {
			shutdown(server->listen_socket, SD_BOTH);
            closesocket(server->listen_socket);
			server->listen_socket = INVALID_SOCKET;
        }

		if (server->client_sock != INVALID_SOCKET)
        {
			shutdown(server->client_sock, SD_BOTH);
            closesocket(server->client_sock);
			server->client_sock = INVALID_SOCKET;
        }

        WSACleanup();
    }
}

static void network_server_worker()
{
	NetworkServerInfo *server = gdata->server;
	assert(server != NULL);

    init_socket();

    while(server->working)
    {
        listen(server->listen_socket , 1);
          
        stdout_message("Waiting for incoming connections...\n");
          
        struct sockaddr_in client;
        int sockaddr_size = sizeof(struct sockaddr_in);
        server->client_sock = accept(server->listen_socket , (struct sockaddr *)&client, &sockaddr_size);
        if (server->client_sock == INVALID_SOCKET)
        {
            stdout_message("accept failed with error code : %d\n" , WSAGetLastError());
            continue;
        }
          
        stdout_message("Connection accepted\n");
      
        //Hello to client
        const char *message = "Hello Client , I am JVM TI\n";
        send(server->client_sock , message , strlen(message) , 0);

		while (server->working)
		{
			while (!server->queue.empty())
			{
				
			}
		}
    }
    finit_socket();
}

static DWORD WINAPI ServerThreadProc(void *context)
{
    network_server_worker();
	return 1;
}

static void start_network_server()
{
    if (gdata->server == NULL)
    {
        gdata->server = (NetworkServerInfo *)malloc(sizeof(NetworkServerInfo));
        if (gdata->server == NULL)
        {
            fatal_error("ERROR: Out of malloc memory\n");
        }
    }

    NetworkServerInfo *server = gdata->server;

    server->working = true;
	HANDLE server_thread = CreateThread(NULL, 0, &ServerThreadProc, NULL, 0, NULL);
    if (server_thread == NULL)
    {
        fatal_error("ERROR: Can't create thread\n");
    }

    server->thread = server_thread;
}

static void stop_network_server()
{
    if (gdata->server != NULL && gdata->server->working)
    {
        NetworkServerInfo *server = gdata->server;
        assert(server->thread != NULL);

		server->working = false;
        finit_socket();
        WaitForSingleObject(server->thread, INFINITE);
        CloseHandle(server->thread);
        server->thread = NULL;

		free(server);
		gdata->server = NULL;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////


JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) 
{
    init_global_data();
	parse_agent_options(options);
    init_jvmti(jvm);
	init_capabilities();
    set_event_notifications();
    set_event_callbacks();
    init_lock();
    start_network_server();

    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{
    stop_network_server();
}