Method Call Trace
=================

Build
-----
-> mingw32-make JDK=%JDK_PATH% OSNAME=win32 run

Run
---
-> java -agentlib:method_call_trace -jar main.jar
