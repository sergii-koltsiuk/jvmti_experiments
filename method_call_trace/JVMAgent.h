#ifndef _INCLUDE_JVM_AGENT_H_
#define _INCLUDE_JVM_AGENT_H_


#include "JVMAgentConstants.h"

#include <jvmti.h>

#include <string>
#include <vector>


class NetworkServer;


class JVMAgent
{
public:
	static JVMAgent & instance()
	{
		static JVMAgent instance;
		return instance;
	}

	JVMAgent(JVMAgent const&) = delete;
	JVMAgent(JVMAgent&&) = delete;
	JVMAgent& operator=(JVMAgent const&) = delete;
	JVMAgent& operator=(JVMAgent &&) = delete;
	
	static void init_jvmti(JavaVM *jvm, char * options);	
	static void finit_jvmti(JavaVM *jvm);
	static void lock();
	static void unlock();

protected:
	JVMAgent();
	~JVMAgent();

private:
	void parse_options(char * options);
	void do_init_jvmti(JavaVM *jvm);
	void init_capabilities() const;
	void set_event_notifications() const;
	void set_event_callbacks() const;
	void init_lock();
	void do_lock() const;
	void do_unlock() const;

	static void __stdcall cbVMStart(jvmtiEnv *jvmti, JNIEnv *env);
	static void __stdcall cbVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread);
	static void __stdcall cbVMDeath(jvmtiEnv *jvmti, JNIEnv *env);
	static void __stdcall cbThreadStart(jvmtiEnv *jvmti, JNIEnv *env, jthread thread);
	static void __stdcall cbThreadEnd(jvmtiEnv *jvmti, JNIEnv *env, jthread thread);
	static void __stdcall cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv *env, 
		jclass class_being_redefined, jobject loader, const char *name, 
		jobject protection_domain, jint class_data_len, const unsigned char *class_data, 
		jint *new_class_data_len, unsigned char **new_class_data);

	void process_cbVMStart(jvmtiEnv *jvmti, JNIEnv *env);
	void process_cbVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const;
	void process_cbVMDeath(jvmtiEnv *jvmti, JNIEnv *env);
	void process_cbThreadStart(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const;
	void process_cbThreadEnd(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) const;
	void process_cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv *env,
		jclass class_being_redefined, jobject loader, const char *name,
		jobject protection_domain, jint class_data_len, const unsigned char *class_data,
		jint *new_class_data_len, unsigned char **new_class_data);


	static void mnum_callbacks(unsigned cnum, const char **names, const char **sigs, int mcount);
	static void get_thread_name(jvmtiEnv *jvmti, jthread thread, char *tname, int maxlen);

	static void MTRACE_native_entry(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum);
	static void MTRACE_native_exit(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum);
	void process_method_entry(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum);
	void process_method_exit(JNIEnv *env, jclass klass, jobject thread, jint cnum, jint mnum);

	void start_network_server() const;
	void stop_network_server() const;

private:
	struct MethodInfo
	{
		std::string m_name;					 // Method name 
		std::string m_signature;			 // Method signature 
		int         m_calls;				 // Method call count 
		int         m_returns;				 // Method return count 
	};

	struct ClassInfo
	{
		std::string m_name;					 // Class name 
		int         m_mcount;				 // Method count 
		std::vector<MethodInfo> m_methods;   // Method information 
		int         m_calls;				 // Method call count for this class 
	};

private:
	// JVMTI Environment 
	jvmtiEnv *m_jvmti;
	bool m_vm_is_dead;
	bool m_vm_is_started;

	// Data access Lock 
	jrawMonitorID m_lock;

	// Options 
	std::string m_include;

	// ClassInfo Table 
	std::vector<ClassInfo> m_classes;

	// Network Server Data
	NetworkServer *m_server;
};

#endif // _INCLUDE_JVM_AGENT_H_
