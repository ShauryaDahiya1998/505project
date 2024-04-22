CXX = g++ # Windows (SAD) LINE
# CXX = aarch64-linux-gnu-g++  MAC LINE

CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations -w
LIBS = -lthrift -lssl -lcrypto -lresolv

TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc httpCreatorSource.cc frontserverV1.cc smtpclient.cc smtp.cc

KVS_SERVER = kvs.cc gen-cpp/StorageOps.cpp 

KVS_TARGET = kvs

KVS_CLIENT = client.cc gen-cpp/StorageOps.cpp

KVS_CLIENT_TARGET = client 

# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET) $(KVS_TARGET)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	$(CXX)  $(CXXFLAGS)  -o $(TARGET)  gen-cpp/StorageOps.cpp gen-cpp/KvsCoordOps.cpp $(OBJECTS)  -lpthread $(LIBS) -w

$(KVS_TARGET): $(KVS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(KVS_CLIENT_TARGET): $(KVS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Rule to compile source files to object files
# %.o: %.cc
# 	@echo "Compiling $<"
# 	$(CXX) -c $< -o $@ -lpthread -w

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS) $(KVS_TARGET)