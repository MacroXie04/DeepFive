# ========================================================================================= #
#  Bobcat UI Application Makefile       													#	#
#  Hongzhe Xie                      														#
#  CSE 030 Data Structure 															   		#
#  University of California, Merced    												   		#
# ========================================================================================= #

# ===================================== PROJECT CONFIG ==================================== #
SRC_DIR      := src
TEST_DIR     := test
OBJ_DIR      := objects
LOCAL_BIN    := bin
APP          := app
MAIN         := main
TEST         := test
HEADERS      := $(shell find $(SRC_DIR) -name '*.h')
SRC          := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ          := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
TEST_SRC     := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJ     := $(TEST_SRC:$(TEST_DIR)/%.cpp=$(OBJ_DIR)/test/%.o)
LOCAL_BIN_DIR:= $(LOCAL_BIN)
BIN_DIR      := $(LOCAL_BIN)
OUT          := $(BIN_DIR)/$(APP)
TEST_OUT     := $(BIN_DIR)/$(TEST)

MAKEFLAGS   += --no-print-directory

# ================================ PLATFORM DETECTION ====================================== #
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  # macOS
  CXX        := clang++
  CXXFLAGS   := -Wall `fltk-config --cxxflags` -std=c++17 -DGL_SILENCE_DEPRECATION -I.
  GLFLAGS    := -framework OpenGL
else
  # assume Linux
  CXX        := g++
  CXXFLAGS   := -Wall `fltk-config --cxxflags` -std=c++17 -I.
  GLFLAGS    := -lGL -lGLU
endif

LDFLAGS := `fltk-config --ldflags` -lfltk_gl $(GLFLAGS)

# ==================================== RULES ================================================ #

all: $(OUT)

$(OUT): $(OBJ) | $(OBJ_DIR) $(LOCAL_BIN_DIR)
	$(CXX) $(OBJ) -o $(OUT) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LOCAL_BIN_DIR):
	mkdir -p $(LOCAL_BIN_DIR)

run: all
	@if command -v clear >/dev/null 2>&1 && [ -n "$$TERM" ] && [ "$$TERM" != "dumb" ]; then clear; fi
	@$(LOCAL_BIN_DIR)/$(APP)

test: $(OBJ) $(TEST_OBJ) | $(BIN_DIR) $(LOCAL_BIN_DIR)
	$(CXX) $(filter-out $(OBJ_DIR)/$(MAIN).o,$(OBJ)) $(TEST_OBJ) -o $(TEST_OUT) $(LDFLAGS)
	$(LOCAL_BIN_DIR)/$(TEST) --output=color || true

autograde: clean test
	xvfb-run $(LOCAL_BIN_DIR)/$(TEST) || true

$(OBJ_DIR)/test/%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LOCAL_BIN_DIR)/$(APP) $(LOCAL_BIN_DIR)/$(TEST)
	rm -rf $(OBJ_DIR)
	rmdir $(LOCAL_BIN_DIR) 2> /dev/null || true

.PHONY: all run test autograde clean