
CONNECTALDIR?=/home/cwchung/workspace/nohost/tools/connectal
DTOP?=/home/cwchung/workspace/nohost/projects/amf_8192_bram/zc706_ubuntu

TOOLCHAIN?=arm-linux-gnueabihf-
ifneq ($(TOOLCHAIN),)
CC=$(TOOLCHAIN)gcc
CXX=$(TOOLCHAIN)g++
endif
CFLAGS_COMMON = -O -g -I$(DTOP)/jni -I$(CONNECTALDIR) -I$(CONNECTALDIR)/cpp -I$(CONNECTALDIR)/lib/cpp   -Wall -Werror -I$(DTOP)/jni -I$(CONNECTALDIR) -I$(CONNECTALDIR)/cpp -I$(CONNECTALDIR)/lib/cpp  
CFLAGS = $(CFLAGS_COMMON)
CFLAGS2 = 

include $(DTOP)/Makefile.autotop
include $(CONNECTALDIR)/scripts/Makefile.connectal.application
SOURCES = /home/cwchung/workspace/nohost/projects/amf_8192_bram/main_example.cpp /home/cwchung/workspace/nohost/tools/connectal/cpp/dmaManager.c /home/cwchung/workspace/nohost/tools/connectal/cpp/platformMemory.cpp $(PORTAL_SRC_FILES)
SOURCES2 =  $(PORTAL_SRC_FILES)
XSOURCES = $(CONNECTALDIR)/cpp/XsimTop.cpp $(PORTAL_SRC_FILES)
LDLIBS :=    -lpthread

ubuntu.exe: $(SOURCES)
	$(Q)$(CXX) $(CFLAGS) -o ubuntu.exe $(SOURCES) $(LDLIBS)
	$(Q)[ ! -f ../bin/mkTop.bin.gz ] || $(TOOLCHAIN)objcopy --add-section fpgadata=../bin/mkTop.bin.gz ubuntu.exe

connectal.so: $(SOURCES)
	$(Q)$(CXX) -shared -fpic $(CFLAGS) -o connectal.so $(SOURCES) $(LDLIBS)

ubuntu.exe2: $(SOURCES2)
	$(Q)$(CXX) $(CFLAGS) $(CFLAGS2) -o ubuntu.exe2 $(SOURCES2) $(LDLIBS)

xsim: $(XSOURCES)
	$(CXX) $(CFLAGS) -o xsim $(XSOURCES)
