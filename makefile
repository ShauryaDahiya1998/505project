CXX = aarch64-linux-gnu-g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
LIBS = -lthrift -lssl -lcrypto

TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc httpCreatorSource.cc frontserverV1.cc

KVS_SERVER = kvs.cc gen-cpp/StorageOps.cpp

KVS_TARGET = kvs 

KVS_COORD = kvsCoord.cc gen-cpp/KvsCoordOps.cpp

KVS_COORD_TARGET = kvsCoord

CLIENT = client.cc gen-cpp/KvsCoordOps.cpp

CLIENT_TARGET = client

# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET) $(KVS_TARGET) $(KVS_COORD_TARGET) $(CLIENT_TARGET)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@g++  $(CXXFLAGS)  -o $(TARGET) gen-cpp/StorageOps.cpp $(OBJECTS)  -lpthread $(LIBS)

$(KVS_TARGET): $(KVS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(KVS_COORD_TARGET): $(KVS_COORD)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(CLIENT_TARGET): $(CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Rule to compile source files to object files
%.o: %.cc
	@echo "Compiling $<"
	@g++ -c $< -o $@ -lpthread 

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS) $(KVS_TARGET) $(KVS_COORD_TARGET)
