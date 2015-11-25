########################################################################
#
# Sample GNU Makefile for building JVMTI agents
#
#  Example uses:
#       make [OPT=true]
#
########################################################################

# Source lists
LIBNAME=method_call_trace
CSOURCES=java_crw_demo.c agent_util.c
CXXSOURCES = main.cpp JVMAgent.cpp NetworkServer.cpp
JAVA_SOURCES=Test.java TestThread.java
JAVA_TOOL_SOURCES=bridge.java
JAVA_MANIFEST=manifest.mf

# Name of jar file that needs to be created
SOURCES_JARFILE=test.jar
TOOL_JARFILE=bridge.jar
JDK=$(JDK_PATH)

# Windows Microsoft C/C++ Optimizing Compiler Version 12
CC="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl"
CXX="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl"
# Compiler options needed to build it
COMMON_FLAGS=-Gy -DWIN32
# Options that help find errors
COMMON_FLAGS+=-W0 -WX /EHsc
ifeq ($(OPT), true)
    COMMON_FLAGS += -Ox -Op -Zi
else
    COMMON_FLAGS += -Od -Zi
    LDFLAGS= /debug /incremental:yes
endif
# Object files needed to create library
OBJECTC=$(CSOURCES:%.c=%.obj)
OBJECTCXX=$(CXXSOURCES:%.cpp=%.obj)
# Library name and options needed to build it
LIBRARY=$(LIBNAME).dll

# Libraries we are dependent on
LIBRARIES= /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib"
LIBRARIES += /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib"
# Building a shared library
LINK_SHARED="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\link" /dll -out:$@
COMMON_FLAGS += -I"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include"
COMMON_FLAGS += -I"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\include"

# Common -I options
COMMON_FLAGS += -I.
COMMON_FLAGS += -I"$(JDK)/include" -I"$(JDK)/include/win32"
CFLAGS = $(COMMON_FLAGS)
CXXFLAGS = $(COMMON_FLAGS)

# Default rule (build both native library and jar file)
all: $(LIBRARY) jarfiles
dbg: 
	@echo $(OBJECTC) 
	@echo $(OBJECTCXX)
java: jarfiles

# Build native library
$(LIBRARY): $(OBJECTC) $(OBJECTCXX)
	$(LINK_SHARED) $(OBJECTC) $(OBJECTCXX) $(LIBRARIES)

# Build jar file
jarfiles: $(SOURCES_JARFILE) $(TOOL_JARFILE)

$(SOURCES_JARFILE): $(JAVA_SOURCES) $(JAVA_MANIFEST)
	"$(JDK)/bin/javac" $(JAVA_SOURCES)
	"$(JDK)/bin/jar" cfmv $(SOURCES_JARFILE) $(JAVA_MANIFEST) *.class

$(TOOL_JARFILE): $(JAVA_TOOL_SOURCES)
	"$(JDK)/bin/javac" $(JAVA_TOOL_SOURCES)
	"$(JDK)/bin/jar" cfv $(TOOL_JARFILE) *.class

# Cleanup the built bits
clean:
	del $(LIBRARY) $(SOURCES_JARFILE) $(TOOL_JARFILE) $(OBJECTC) $(OBJECTCXX) 
	del *.class *.lib *.exp *pdb

# Simple tester
test: all
	"$(JDK)/bin/java" -Xbootclasspath/a:bridge.jar  -agentlib:method_call_trace=include=Test -jar test.jar

%.obj: %.c
	$(COMPILE.c) $<

%.obj: %.cpp
	$(COMPILE.c) $<

