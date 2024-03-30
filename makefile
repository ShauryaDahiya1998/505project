CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
LIBS = -lthrift

TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc httpCreatorSource.cc frontserverV1.cc

KVS_SERVER = kvs.cc gen-cpp/StorageOps.cpp

KVS_TARGET = kvs 

# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET) $(KVS_TARGET)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@g++  $(CXXFLAGS)  -o $(TARGET) gen-cpp/StorageOps.cpp $(OBJECTS)  -lpthread $(LIBS)

$(KVS_TARGET): $(KVS_SERVER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Rule to compile source files to object files
%.o: %.cc
	@echo "Compiling $<"
	@g++ -c $< -o $@ -lpthread 

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS) $(KVS_TARGET)
