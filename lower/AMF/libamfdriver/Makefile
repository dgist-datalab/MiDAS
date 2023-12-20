TARGET_NAME := libAmfManager.a

OBJ_DIR := obj

INCLUDES := \
	-I connectal_lib \
	-I connectal_lib/cpp \
	-I device_ifc

CXXFLAGS += -std=c++11 -g

LIBS := \
	-lm -lpthread -lrt

SRCS := \
	device_ifc/AmfIndication.c \
	device_ifc/AmfRequest.c \
	device_ifc/GeneratedCppCallbacks.cpp \
	device_ifc/MMURequest.c \
	device_ifc/MMUIndication.c \
	device_ifc/MemServerRequest.c \
	device_ifc/MemServerIndication.c \
	connectal_lib/cpp/portal.c \
	connectal_lib/cpp/portalPrintf.c \
	connectal_lib/cpp/transportHardware.c \
	connectal_lib/cpp/poller.cpp \
	connectal_lib/cpp/DmaBuffer.cpp \
	connectal_lib/cpp/dmaManager.c \
	connectal_lib/cpp/platformMemory.cpp \
	connectal_lib/cpp/timer.c \
	AmfManager.cpp \

OBJS := \
	$(SRCS:%.c=$(OBJ_DIR)/%.o) $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

.PHONY: all clean build

all: $(TARGET_NAME)
$(TARGET_NAME): build $(OBJS)
	$(AR) r $(@) $(OBJS)

build:
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf obj libAmfManager.a *.exe


example1.exe: $(TARGET_NAME) example1.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ example1.cpp $(TARGET_NAME) $(LIBS)

hardReset.exe: $(TARGET_NAME) hardReset.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ hardReset.cpp $(TARGET_NAME) $(LIBS)

badBlock.exe: $(TARGET_NAME) badBlock.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ badBlock.cpp $(TARGET_NAME) $(LIBS)

fastRead.exe: $(TARGET_NAME) badBlock.cpp
	$(CXX) $(CXXFLAGS) -DWRITESYNC -DFASTREAD $(INCLUDES) -o $@ badBlock.cpp $(TARGET_NAME) $(LIBS)

