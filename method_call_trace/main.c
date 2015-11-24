#include <jvmti.h>
#include <jni.h>
#include <stdlib.h>


#define MAX_TOKEN_LENGTH        16
#define MAX_THREAD_NAME_LENGTH  512
#define MAX_METHOD_NAME_LENGTH  1024

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
} GlobalAgentData;
                  
static GlobalAgentData *gdata;


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// JVMTI_EVENT_VM_INIT 
void JNICALL cbVMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread)
{
    printf("VMInit\n");
}

// JVMTI_EVENT_VM_DEATH 
void JNICALL cbVMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env)
{
    printf("VMDeath\n");
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static void parse_agent_options(char *options)
{
    printf("agent options: ");
    printf(options==NULL? "NULL":options);
    printf("\r\n");

    char token[MAX_TOKEN_LENGTH];
    char *next;

     // Parse options and set flags in gdata 
    if ( options==NULL ) 
    {
        return;
    }

     // Get the first token from the options string. 
    next = get_token(options, ",=", token, sizeof(token));

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

    res = (*jvm)->GetEnv(jvm, (void **)&jvmti, JVMTI_VERSION_1);
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
    capabilities.can_signal_thread = 1;
    capabilities.can_get_owned_monitor_info = 1;
    capabilities.can_generate_method_entry_events = 1;
    capabilities.can_generate_method_exit_events = 1;
    capabilities.can_tag_objects = 1;   

    error = (*jvmti)->AddCapabilities(jvmti, &capabilities);             
    check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");
}

static void set_event_notifications()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    error = (*jvmti)->SetEventNotificationMode
        (jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);

    check_jvmti_error(jvmti, error, "Cannot set event notification");
                  

    error = (*jvmti)->SetEventNotificationMode
        (jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);

    check_jvmti_error(jvmti, error, "Cannot set event notification");
}

static void set_event_callbacks()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    jvmtiEventCallbacks callbacks;               
    (void)memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit = &cbVMInit;  // JVMTI_EVENT_VM_INIT 
    callbacks.VMDeath = &cbVMDeath;  // JVMTI_EVENT_VM_DEATH 
    error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks,(jint)sizeof(callbacks));
    check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");
}

static void init_lock()
{
    jvmtiEnv *jvmti = gdata->jvmti;
    jvmtiError error;

    error = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));
    check_jvmti_error(jvmti, error, "Cannot create raw monitor");
}

static void enter_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
}

static void exit_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error = (*jvmti)->RawMonitorExit(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////


JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) 
{
    parse_agent_options(options);
    init_global_data();
    init_jvmti(jvm);
    set_event_notifications();
    set_event_callbacks();
    init_lock();

    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{

}