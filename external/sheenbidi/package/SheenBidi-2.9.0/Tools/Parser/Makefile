PARSER = $(DEBUG)/Parser

PARSER_SRCS = $(PARSER_DIR)/BidiBrackets.cpp \
              $(PARSER_DIR)/BidiCharacterTest.cpp \
              $(PARSER_DIR)/BidiMirroring.cpp \
              $(PARSER_DIR)/BidiTest.cpp \
              $(PARSER_DIR)/DataFile.cpp \
              $(PARSER_DIR)/DerivedBidiClass.cpp \
              $(PARSER_DIR)/DerivedCoreProperties.cpp \
              $(PARSER_DIR)/DerivedGeneralCategory.cpp \
              $(PARSER_DIR)/PropertyValueAliases.cpp \
              $(PARSER_DIR)/PropList.cpp \
              $(PARSER_DIR)/Scripts.cpp \
              $(PARSER_DIR)/UnicodeData.cpp \
              $(PARSER_DIR)/UnicodeVersion.cpp

PARSER_OBJS = $(PARSER_SRCS:$(PARSER_DIR)/%.cpp=$(PARSER)/%.o)

$(PARSER):
	mkdir $(PARSER)

$(PARSER)/%.o: $(PARSER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) -c $< -o $@

$(PARSER_TARGET): $(PARSER_OBJS)
	$(AR) $(ARFLAGS) $(PARSER_TARGET) $(PARSER_OBJS)

parser: $(PARSER) $(PARSER_TARGET)

parser_clean:
	$(RM) $(PARSER)/*.o
	$(RM) $(PARSER_TARGET)
