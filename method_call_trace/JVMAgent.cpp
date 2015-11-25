#include "JVMAgent.h"
#include "NetworkServer.h"

#include "agent_util.h"
#include "java_crw_demo.h"
#include <cassert>



JVMAgent::JVMAgent(): 
	m_jvmti(nullptr), 
	m_vm_is_dead(false),
	m_vm_is_started(false), 
	m_lock(nullptr),
	m_server(nullptr)
{
	m_server = new NetworkServer();
}


JVMAgent::~JVMAgent()
{
	delete m_server;
	m_server = nullptr;
}

/*static*/ 
void JVMAgent::init_jvmti(JavaVM *jvm, char * options)
{
	JVMAgent &self = instance();
	self.do_init_jvmti(jvm);
	self.parse_options(options);		
	self.init_lock();
	self.init_capabilities();
	self.set_event_notifications();	
	self.set_event_callbacks();
	self.start_network_server();
}

void JVMAgent::finit_jvmti(JavaVM *jvm)
{
	JVMAgent &self = instance();
	self.stop_network_server();
}

/*static */
void JVMAgent::lock()
{
	instance().do_lock();
}

/*static*/ 
void JVMAgent::unlock()
{
	instance().do_unlock();
}

void JVMAgent::parse_options(char *options)
{
	stdout_message("agent options: ");
	stdout_message(options == nullptr ? "nullptr" : options);
	stdout_message("\n");

	char token[MAX_TOKEN_LENGTH];
	char *next;

	// Parse options and set flags in gdata 
	if (options == nullptr)
	{
		return;
	}

	// Get the first token from the options string. 
	next = get_token(options, " ,=", token, sizeof(token));

	// While not at the end of the options string, process this option. 
	while (next != nullptr)
	{
		if (strcmp(token, "help") == 0)
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
		
		if (strcmp(token, "include") == 0)
		{
			// Add this item to the list
			m_include.resize(MAX_METHOD_NAME_LENGTH);
			next = get_token(next, ",=", const_cast<char *>(m_include.data()), MAX_METHOD_NAME_LENGTH);
			// Check for token scan error 
			if (next == nullptr)
			{
				fatal_error("ERROR: include option error\n");
			}
		}
		else if (token[0] != 0)
		{
			// We got a non-empty token and we don't know what it is. 
			fatal_error("ERROR: Unknown option: %s\n", token);
		}

		// Get the next token (returns nullptr if there are no more) 
		next = get_token(next, ",=", token, sizeof(token));
	}
}

void JVMAgent::do_init_jvmti(JavaVM *jvm)
{
	jint res = jvm->GetEnv(reinterpret_cast<void **>(&m_jvmti), JVMTI_VERSION_1);
	if (res != JNI_OK)
	{
		// This means that the VM was unable to obtain this version of the
		// JVMTI interface, this is a fatal error.         
		fatal_error("ERROR: Unable to access JVMTI Version 1 (0x%x),"
			" is your JDK a 5.0 or newer version?"
			" JNIEnv's GetEnv() returned %d\n",
			JVMTI_VERSION_1, res);
	}
}

void JVMAgent::init_capabilities() const
{
	jvmtiError error;

	static jvmtiCapabilities capabilities;
	(void)memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_generate_all_class_hook_events = 1;
	error = m_jvmti->AddCapabilities(&capabilities);
	check_jvmti_error(m_jvmti, error, "Unable to get necessary JVMTI capabilities.");
}

void JVMAgent::set_event_notifications() const
{
	jvmtiEnv *jvmti = m_jvmti;
	jvmtiError error;

	/* At first the only initial events we are interested in are VM
	*   initialization, VM death, and Class File Loads.
	*   Once the VM is initialized we will request more events.
	*/
	error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_START, static_cast<jthread>(nullptr));
	check_jvmti_error(jvmti, error, "Cannot set event notification");

	error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, static_cast<jthread>(nullptr));
	check_jvmti_error(jvmti, error, "Cannot set event notification");

	error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, static_cast<jthread>(nullptr));
	check_jvmti_error(jvmti, error, "Cannot set event notification");

	error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, static_cast<jthread>(nullptr));
	check_jvmti_error(jvmti, error, "Cannot set event notification");
}

void JVMAgent::set_event_callbacks() const 
{
	jvmtiEnv *jvmti = m_jvmti;
	jvmtiError error;

	jvmtiEventCallbacks callbacks;
	(void)memset(&callbacks, 0, sizeof(callbacks));
	callbacks.VMStart = &JVMAgent::cbVMStart; // JVMTI_EVENT_VM_START 
	callbacks.VMInit = &JVMAgent::cbVMInit; // JVMTI_EVENT_VM_INIT 
	callbacks.VMDeath = &JVMAgent::cbVMDeath; // JVMTI_EVENT_VM_DEATH 
	callbacks.ClassFileLoadHook = &JVMAgent::cbClassFileLoadHook; // JVMTI_EVENT_CLASS_FILE_LOAD_HOOK     
	callbacks.ThreadStart = &JVMAgent::cbThreadStart; // JVMTI_EVENT_THREAD_START 
	callbacks.ThreadEnd = &JVMAgent::cbThreadEnd; // JVMTI_EVENT_THREAD_END 
	error = jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));
	check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");
}

void JVMAgent::init_lock()
{
	jvmtiEnv *jvmti = m_jvmti;
	jvmtiError error;

	error = (*jvmti).CreateRawMonitor("agent data", &(m_lock));
	check_jvmti_error(jvmti, error, "Cannot create raw monitor");
}

void JVMAgent::do_lock() const
{
	jvmtiError error = m_jvmti->RawMonitorEnter(m_lock);
	check_jvmti_error(m_jvmti, error, "Cannot enter with raw monitor");
}

void JVMAgent::do_unlock() const
{
	jvmtiError error = m_jvmti->RawMonitorExit(m_lock);
	check_jvmti_error(m_jvmti, error, "Cannot exit with raw monitor");
}

//////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////

/*static*/ 
void JVMAgent::MTRACE_native_entry(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	JVMAgent::instance().process_method_entry(env, klass, thread, cnum, mnum);
}

/*static*/ 
void JVMAgent::MTRACE_native_exit(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	JVMAgent::instance().process_method_exit(env, klass, thread, cnum, mnum);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// JVMTI_EVENT_VM_START 
void __stdcall JVMAgent::cbVMStart(jvmtiEnv *jvmti, JNIEnv *env)
{
	JVMAgent::instance().process_cbVMStart(jvmti, env);
}

// JVMTI_EVENT_VM_INIT 
void __stdcall JVMAgent::cbVMInit(jvmtiEnv *jvmti, JNIEnv* env, jthread thread)
{
	JVMAgent::instance().process_cbVMInit(jvmti, env, thread);
}

// JVMTI_EVENT_VM_DEATH 
void __stdcall JVMAgent::cbVMDeath(jvmtiEnv *jvmti, JNIEnv* env)
{
	JVMAgent::instance().process_cbVMDeath(jvmti, env);
}

// JVMTI_EVENT_THREAD_START 
void __stdcall JVMAgent::cbThreadStart(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
	JVMAgent::instance().process_cbThreadStart(jvmti, env, thread);
}

// JVMTI_EVENT_THREAD_END
void __stdcall JVMAgent::cbThreadEnd(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
	JVMAgent::instance().process_cbThreadEnd(jvmti, env, thread);
}

// JVMTI_EVENT_CLASS_FILE_LOAD_HOOK 
void __stdcall JVMAgent::cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv* env,
	jclass class_being_redefined, jobject loader,
	const char* name, jobject protection_domain,
	jint class_data_len, const unsigned char* class_data,
	jint* new_class_data_len, unsigned char** new_class_data)
{
	JVMAgent::instance().process_cbClassFileLoadHook(jvmti, env, class_being_redefined, loader,
		name, protection_domain, class_data_len, class_data, new_class_data_len, new_class_data);	
}

void JVMAgent::process_cbVMStart(jvmtiEnv *jvmti, JNIEnv *env)
{
	stdout_message("VMStart\n");
	m_vm_is_started = JNI_TRUE;
}

void JVMAgent::process_cbVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const
{
	lock();
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
		if (klass == nullptr)
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
		if (field == nullptr)
		{
			fatal_error("ERROR: JNI: Cannot get field from %s\n", STRING(MTRACE_class));
		}

		(*env).SetStaticIntField(klass, field, 1);

		/////////////////////////////////////////////
		/////////////////////////////////////////////
		/////////////////////////////////////////////

		char  tname[MAX_THREAD_NAME_LENGTH];

		static jvmtiEvent events[] = { JVMTI_EVENT_THREAD_START, JVMTI_EVENT_THREAD_END };
		size_t i;

		// The VM has started. 
		get_thread_name(jvmti, thread, tname, sizeof(tname));
		stdout_message("VMInit %s\n", tname);

		// The VM is now initialized, at this time we make our requests
		// for additional events.


		for (i = 0; i < sizeof(events) / sizeof(jvmtiEvent); i++)
		{
			jvmtiError error;

			// Setup event  notification modes 
			error = (*jvmti).SetEventNotificationMode(JVMTI_ENABLE,
				events[i], static_cast<jthread>(nullptr));
			check_jvmti_error(jvmti, error, "Cannot set event notification");
		}
	}
	unlock();
}

void JVMAgent::process_cbVMDeath(jvmtiEnv *jvmti, JNIEnv *env)
{
	lock();
	{
		jclass   klass;
		jfieldID field;

		// The VM has died. 
		stdout_message("VMDeath\n");

		// Disengage calls in MTRACE_class. 
		klass = (*env).FindClass(STRING(MTRACE_class));
		if (klass == nullptr)
		{
			fatal_error("ERROR: JNI: Cannot find %s with FindClass\n", STRING(MTRACE_class));
		}

		field = (*env).GetStaticFieldID(klass, STRING(MTRACE_engaged), "I");
		if (field == nullptr)
		{
			fatal_error("ERROR: JNI: Cannot get field from %s\n", STRING(MTRACE_class));
		}

		(*env).SetStaticIntField(klass, field, 0);

		m_vm_is_dead = JNI_TRUE;

	}
	unlock();
}

void JVMAgent::process_cbThreadStart(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const
{
	lock();
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!m_vm_is_dead)
		{
			char  tname[MAX_THREAD_NAME_LENGTH];

			get_thread_name(jvmti, thread, tname, sizeof(tname));
			stdout_message("ThreadStart %s\n", tname);
		}
	}
	unlock();
}

void JVMAgent::process_cbThreadEnd(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const
{
	lock();
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!m_vm_is_dead)
		{
			char  tname[MAX_THREAD_NAME_LENGTH];

			get_thread_name(jvmti, thread, tname, sizeof(tname));
			stdout_message("ThreadEnd %s\n", tname);
		}
	}
	unlock();
}

void JVMAgent::process_cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv *env, jclass class_being_redefined, jobject loader, const char *name, jobject protection_domain, jint class_data_len, const unsigned char *class_data, jint *new_class_data_len, unsigned char **new_class_data)
{
	lock();
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!m_vm_is_dead)
		{
			const char *classname;

			/* Name could be nullptr */
			if (name == nullptr)
			{
				classname = java_crw_demo_classname(class_data, class_data_len, nullptr);
				if (classname == nullptr)
				{
					fatal_error("ERROR: No classname inside classfile\n");
				}
			}
			else
			{
				classname = strdup(name);
				if (classname == nullptr)
				{
					fatal_error("ERROR: Out of malloc memory\n");
				}
			}

			*new_class_data_len = 0;
			*new_class_data = nullptr;

			if (interested(const_cast<char*>(classname), "", const_cast<char *>(m_include.data()), nullptr))
			{
				stdout_message("Class load %s\n", classname);

				jint           cnum;
				int            system_class;
				unsigned char *new_image;
				long           new_length;
				ClassInfo     *cp;

				/* Get unique number for every class file image loaded */

				/* Save away class information */

				cnum = m_classes.size();
				m_classes.resize(cnum + 1);
				cp = &m_classes[cnum];
				cp->m_name = classname;

				cp->m_calls = 0;
				cp->m_mcount = 0;

				/* Is it a system class? If the class load is before VmStart
				*   then we will consider it a system class that should
				*   be treated carefully. (See java_crw_demo)
				*/
				system_class = 0;
				if (!m_vm_is_started)
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
					nullptr, nullptr,
					nullptr, nullptr,
					&new_image,
					&new_length,
					nullptr,
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
				if (new_image != nullptr)
				{
					(void)free((void*)new_image); /* Free malloc() space with free() */
				}
			}

			(void)free((void*)classname);
		}
	}
	unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

/* Callback from java_crw_demo() that gives us mnum mappings */
/*static*/
void JVMAgent::mnum_callbacks(unsigned cnum, const char **names, const char**sigs, int mcount)
{
	ClassInfo *class_info;
	int mnum;
	JVMAgent &self = instance();

	if (cnum >= self.m_classes.size())
	{
		fatal_error("ERROR: Class number out of range\n");
	}

	if (mcount == 0)
	{
		return;
	}

	class_info = &self.m_classes[cnum];
	class_info->m_calls = 0;
	class_info->m_mcount = mcount;
	class_info->m_methods.resize(mcount);

	for (int method_index = 0; method_index < mcount; method_index++)
	{
		MethodInfo *mp = &class_info->m_methods[method_index];
		mp->m_name = names[method_index];
		mp->m_signature = sigs[method_index];
	}
}

/* Get a name for a jthread */
void JVMAgent::get_thread_name(jvmtiEnv *jvmti, jthread thread, char *tname, int maxlen)
{
	jvmtiThreadInfo info;
	jvmtiError      error;

	/* Make sure the stack variables are garbage free */
	(void)memset(&info, 0, sizeof(info));

	/* Assume the name is unknown for now */
	(void)strcpy(tname, "Unknown");

	/* Get the thread information, which includes the name */
	error = (*jvmti).GetThreadInfo(thread, &info);
	check_jvmti_error(jvmti, error, "Cannot get thread info");

	/* The thread might not have a name, be careful here. */
	if (info.name != nullptr)
	{
		size_t len;

		/* Copy the thread name into tname if it will fit */
		len = strlen(info.name);
		if (len < maxlen)
		{
			(void)strcpy(tname, info.name);
		}

		/* Every string allocated by JVMTI needs to be freed */
		deallocate(jvmti, static_cast<void*>(info.name));
	}
}

void JVMAgent::process_method_entry(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	lock();
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!m_vm_is_dead)
		{
			if (cnum >= m_classes.size())
			{
				fatal_error("ERROR: Class number out of range\n");
			}

			ClassInfo  *class_info = &m_classes[cnum];
			if (mnum >= class_info->m_mcount)
			{
				fatal_error("ERROR: Method number out of range\n");
			}

			MethodInfo *method_info = &class_info->m_methods[mnum];

			if (interested(const_cast<char*>(class_info->m_name.c_str()), 
				const_cast<char*>(method_info->m_name.c_str()), 
				const_cast<char *>(m_include.c_str()), nullptr))
			{
				//mp->calls++;
				//cp->calls++;
				m_server->enqueue_for_sending("enter: " + class_info->m_name + ":" + method_info->m_name + "\r\n");
			}
		}
	}
	unlock();
}

void JVMAgent::process_method_exit(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum)
{
	lock();
	{
		// It's possible we get here right after VmDeath event, be careful 
		if (!m_vm_is_dead)
		{
			if (cnum >= m_classes.size())
			{
				fatal_error("ERROR: Class number out of range\n");
			}

			ClassInfo  *class_info = &m_classes[cnum];
			if (mnum >= class_info->m_mcount)
			{
				fatal_error("ERROR: Method number out of range\n");
			}

			MethodInfo *method_info = &class_info->m_methods[mnum];
			if (interested(const_cast<char*>(class_info->m_name.c_str()), 
				const_cast<char*>(method_info->m_name.c_str()), 
				const_cast<char *>(m_include.c_str()),
				nullptr))
			{
				//mp->returns++;
				m_server->enqueue_for_sending("exit: " + class_info->m_name + ":" + method_info->m_name + "\r\n");
			}
		}
	}
	unlock();
}

void JVMAgent::start_network_server() const
{
	assert(m_server != nullptr);

	m_server->start();
}

void JVMAgent::stop_network_server() const
{
	assert(m_server != nullptr);

	m_server->stop();
}
