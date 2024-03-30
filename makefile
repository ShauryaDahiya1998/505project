CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
INCLUDES = -I/opt/homebrew/Cellar/boost/1.84.0_1/include -I/opt/homebrew/Cellar/thrift/0.20.0/include
LIBS = -L/opt/homebrew/Cellar/thrift/0.20.0/lib -lthrift

TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc httpCreatorSource.cc frontserverV1.cc

SRCS_SERVER = kvs.cc gen-cpp/StorageOps.cpp

TARGET_SERVER = kvs 

# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET) $(TARGET_SERVER)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@g++ -o $(TARGET) $(OBJECTS) -lpthread

$(TARGET_SERVER): $(SRCS_SERVER)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Rule to compile source files to object files
%.o: %.cc
	@echo "Compiling $<"
	@g++ -c $< -o $@ -lpthread 

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS) $(TARGET_SERVER)
