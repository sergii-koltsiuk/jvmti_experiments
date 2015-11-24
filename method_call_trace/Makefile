########################################################################
#
# Sample GNU Makefile for building JVMTI agents
#
#  Example uses:    
#       gnumake JDK=<java_home> OSNAME=solaris [OPT=true] [LIBARCH=sparc]
#       gnumake JDK=<java_home> OSNAME=solaris [OPT=true] [LIBARCH=sparcv9]
#       gnumake JDK=<java_home> OSNAME=linux   [OPT=true]
#       gnumake JDK=<java_home> OSNAME=win32   [OPT=true]
#
########################################################################

# Source lists
LIBNAME=method_call_trace
SOURCES=main.c java_crw_demo.c agent_util.c
JAVA_SOURCES=main.java
JAVA_MANIFEST=manifest.mf

# Name of jar file that needs to be created
JARFILE=main.jar

# Windows Microsoft C/C++ Optimizing Compiler Version 12
ifeq ($(OSNAME), win32)
    CC="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\cl"
    # Compiler options needed to build it
    COMMON_FLAGS=-Gy -DWIN32
    # Options that help find errors
    COMMON_FLAGS+=-W0 -WX
    ifeq ($(OPT), true)
        CFLAGS= -Ox -Op -Zi $(COMMON_FLAGS) 
    else
        CFLAGS= -Od -Zi $(COMMON_FLAGS) 
    endif
    # Add in java_crw_demo obj file on windows (easier)
    SOURCES+=
    # Object files needed to create library
    OBJECTS=$(SOURCES:%.c=%.obj)
    # Library name and options needed to build it
    LIBRARY=$(LIBNAME).dll
    LDFLAGS=
    # Libraries we are dependent on
    LIBRARIES= /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib"
    LIBRARIES += /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib"
    # Building a shared library
    LINK_SHARED="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\link" /dll -out:$@
    CFLAGS += -I"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include"
endif

# Common -I options
CFLAGS += -I.
CFLAGS += -I"$(JDK)/include" -I"$(JDK)/include/$(OSNAME)"

# Default rule (build both native library and jar file)
all: $(LIBRARY) $(JARFILE)

# Build native library
$(LIBRARY): $(OBJECTS)
	$(LINK_SHARED) $(OBJECTS) $(LIBRARIES)

# Build jar file
$(JARFILE): $(JAVA_SOURCES)
	"$(JDK)/bin/javac" $(JAVA_SOURCES)
	"$(JDK)/bin/jar" cfmv $(JARFILE) $(JAVA_MANIFEST) *.class

# Cleanup the built bits
clean:
	del $(LIBRARY) $(JARFILE) $(OBJECTS)

# Simple tester
run: all
	"$(JDK)/bin/java" -agentlib:method_call_trace -jar $(JARFILE)

# Compilation rule only needed on Windows
ifeq ($(OSNAME), win32)
%.obj: %.c
	$(COMPILE.c) $<
endif

