
include $(CLEAR_VARS)
DTOP?=/home/cwchung/workspace/lightstore/projects/flash_kt_wr/zcu102
CONNECTALDIR?=/home/cwchung/workspace/lightstore/tools/connectal
LOCAL_ARM_MODE := arm
include $(CONNECTALDIR)/scripts/Makefile.connectal.application
LOCAL_SRC_FILES := /home/cwchung/workspace/lightstore/projects/flash_kt_wr/main.cpp /home/cwchung/workspace/lightstore/tools/connectal/cpp/dmaManager.c /home/cwchung/workspace/lightstore/tools/connectal/cpp/platformMemory.cpp $(PORTAL_SRC_FILES)

LOCAL_PATH :=
LOCAL_MODULE := android.exe
LOCAL_MODULE_TAGS := optional
LOCAL_LDLIBS := -llog   
LOCAL_CPPFLAGS := "-march=armv7-a"
LOCAL_CFLAGS := -I$(DTOP)/jni -I$(CONNECTALDIR) -I$(CONNECTALDIR)/cpp -I$(CONNECTALDIR)/lib/cpp   -Werror
LOCAL_CXXFLAGS := -I$(DTOP)/jni -I$(CONNECTALDIR) -I$(CONNECTALDIR)/cpp -I$(CONNECTALDIR)/lib/cpp   -Werror
LOCAL_CFLAGS2 := $(cdefines2)s

include $(BUILD_EXECUTABLE)
