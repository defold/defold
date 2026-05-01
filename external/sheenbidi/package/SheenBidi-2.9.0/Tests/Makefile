TESTS_INCLUDES = -I$(ROOT_DIR) -I$(HEADERS_DIR) -I$(TOOLS_DIR)
TESTS_FLAGS = $(TESTS_INCLUDES)
TESTS_LIBS = -L$(DEBUG) -l$(LIB_SHEENBIDI) -l$(LIB_PARSER)

TESTS      = $(DEBUG)/Testing
TESTS_UTIL = $(TESTS)/Utilities

TESTS_SRCS = $(TESTS_DIR)/AlgorithmTests.cpp \
             $(TESTS_DIR)/BidiTypeLookupTests.cpp \
             $(TESTS_DIR)/BracketLookupTests.cpp \
             $(TESTS_DIR)/CodepointSequenceTests.cpp \
             $(TESTS_DIR)/CodepointTests.cpp \
             $(TESTS_DIR)/GeneralCategoryLookupTests.cpp \
             $(TESTS_DIR)/main.cpp \
             $(TESTS_DIR)/MirrorLookupTests.cpp \
             $(TESTS_DIR)/RunQueueTests.cpp \
             $(TESTS_DIR)/ScriptLocatorTests.cpp \
             $(TESTS_DIR)/ScriptLookupTests.cpp \
             $(TESTS_DIR)/ScriptTests.cpp \
             $(TESTS_DIR)/Utilities/Convert.cpp

TESTS_OBJS = $(TESTS_SRCS:$(TESTS_DIR)/%.cpp=$(TESTS)/%.o)

$(TESTS):
	mkdir $(TESTS)
	mkdir $(TESTS_UTIL)

$(TESTS)/%.o: $(TESTS_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(TESTS_FLAGS) -c $< -o $@

$(TESTS_TARGET): $(TESTS_OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(TESTS_FLAGS) $(TESTS_LIBS)

tests: debug parser $(TESTS) $(TESTS_TARGET)

tests_clean:
	$(RM) $(TESTS)/*.o
	$(RM) $(TESTS_UTIL)/*.o
	$(RM) $(TESTS_TARGET)
