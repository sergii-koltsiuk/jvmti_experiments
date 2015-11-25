#include "JVMAgent.h"

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) 
{
	JVMAgent::init_jvmti(jvm, options);

    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{
	JVMAgent::finit_jvmti(vm);
}