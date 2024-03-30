TARGET = myserver
# Define source files
SOURCES = getMethod.cc postMethod.cc frontserverV1.cc
# Automatically generate a list of object files
OBJECTS = $(SOURCES:.cc=.o)
# Default target
all: $(TARGET)
# Rule to link the executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@g++ -o $(TARGET) $(OBJECTS) -lpthread

# Rule to compile source files to object files
%.o: %.cc
	@echo "Compiling $<"
	@g++ -c $< -o $@ -lpthread 

# Clean the built files
clean:
	@echo "Cleaning..."
	@rm -f $(TARGET) $(OBJECTS)
