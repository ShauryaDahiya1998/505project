CXX = aarch64-linux-gnu-g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations -pthread -w
LIBS = -lthrift -lssl -lcrypto  -lresolv

TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc httpCreatorSource.cc frontserverV1.cc smtpclient.cc smtp.cc

KVS_SERVER = kvs.cc gen-cpp/StorageOps.cpp gen-cpp/KvsCoordOps.cpp

KVS_TARGET = kvs 

KVS_CLIENT = client.cc gen-cpp/StorageOps.cpp gen-cpp/KvsCoordOps.cpp

FRONT_COORD = frontEndCoord.cc gen-cpp/FrontEndCoordOps.cpp gen-cpp/StorageOps.cpp gen-cpp/FrontEndOps.cpp

FRONT_COORD_TARGET = frontEndCoord


KVS_CLIENT_TARGET = client 

TEST_CLIENT = testclient.cc gen-cpp/StorageOps.cpp

TEST_CLIENT_TARGET = testclient

KVS_COORD = kvsCoord.cc gen-cpp/KvsCoordOps.cpp gen-cpp/StorageOps.cpp

KVS_COORD_TARGET = kvsCoord

# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET) $(KVS_TARGET) $(KVS_COORD_TARGET) $(FRONT_COORD_TARGET)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	$(CXX)  $(CXXFLAGS)  -o $(TARGET)  gen-cpp/StorageOps.cpp gen-cpp/KvsCoordOps.cpp gen-cpp/FrontEndCoordOps.cpp gen-cpp/FrontEndOps.cpp $(OBJECTS)  -lpthread $(LIBS) -w

$(KVS_TARGET): $(KVS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(KVS_CLIENT_TARGET): $(KVS_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(KVS_COORD_TARGET): $(KVS_COORD)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(FRONT_COORD_TARGET): $(FRONT_COORD)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) -lpthread

$(TEST_CLIENT_TARGET): $(TEST_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)


# Rule to compile source files to object files
# %.o: %.cc
# 	@echo "Compiling $<"
# 	$(CXX) -c $< -o $@ -lpthread -w

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS) $(KVS_TARGET) $(FRONT_COORD_TARGET) $(KVS_COORD_TARGET)
