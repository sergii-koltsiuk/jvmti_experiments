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
SOURCES=main.c java_crw_demo.c agent_util.c
JAVA_SOURCES=Test.java TestThread.java
JAVA_TOOL_SOURCES=bridge.java
JAVA_MANIFEST=manifest.mf

# Name of jar file that needs to be created
SOURCES_JARFILE=test.jar
TOOL_JARFILE=bridge.jar
JDK=$(JDK_PATH)

# Windows Microsoft C/C++ Optimizing Compiler Version 12
CC="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl"
# Compiler options needed to build it
COMMON_FLAGS=-Gy -DWIN32
# Options that help find errors
COMMON_FLAGS+=-W0 -WX
ifeq ($(OPT), true)
    CFLAGS= -Ox -Op -Zi $(COMMON_FLAGS) 
else
    CFLAGS= -Od -Zi $(COMMON_FLAGS) 
    LDFLAGS= /debug /incremental:yes
endif
# Object files needed to create library
OBJECTS=$(SOURCES:%.c=%.obj)
# Library name and options needed to build it
LIBRARY=$(LIBNAME).dll

# Libraries we are dependent on
LIBRARIES= /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib"
LIBRARIES += /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib"
# Building a shared library
LINK_SHARED="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\link" /dll -out:$@
CFLAGS += -I"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include"

# Common -I options
CFLAGS += -I.
CFLAGS += -I"$(JDK)/include" -I"$(JDK)/include/win32"

# Default rule (build both native library and jar file)
all: $(LIBRARY) jarfiles
java: jarfiles

# Build native library
$(LIBRARY): $(OBJECTS)
	$(LINK_SHARED) $(OBJECTS) $(LIBRARIES)

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
	del $(LIBRARY) $(SOURCES_JARFILE) $(TOOL_JARFILE) $(OBJECTS) 
	del *.class *.lib *.exp *pdb

# Simple tester
test: all
	"$(JDK)/bin/java" -Xbootclasspath/a:bridge.jar  -agentlib:method_call_trace=include=Test -jar test.jar

%.obj: %.c
	$(COMPILE.c) $<

