TARGET=libft_malloc.so

SRC_DIR=Source
INCLUDE_DIRS=.
BUILD_DIR=.build

TESTS=test0 test1 test2 test3 test4 test5 test6 dburgun
SRC_FILES=malloc.c print.c
DEP_FILES=$(addsuffix .d,$(SRC_FILES))
OBJ_FILES=$(addsuffix .o,$(SRC_FILES))

LIB_DIRS=
LIBS=pthread

CC=gcc
C_FLAGS=-Wall -Wextra -Werror -g -Wno-unused-variable -Wno-unused-function
DEFINES=FT_MALLOC_ENABLE_ASSERTS #FT_MALLOC_DEBUG_LOG #VERIFY_LIST

all: $(TARGET) $(TEST_TARGET)

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) -fPIC $(addprefix -D,$(DEFINES)) -MMD -MP -MF$(BUILD_DIR)/$*.c.d $(addprefix -I,$(INCLUDE_DIRS)) -c $< -o $@

$(TARGET): $(addprefix $(BUILD_DIR)/,$(OBJ_FILES))
	$(CC) -shared $(addprefix $(BUILD_DIR)/,$(OBJ_FILES)) $(addprefix -L,$(LIB_DIRS) $(LIB_DIRS)) $(addprefix -l,$(LIBS)) -o $@

.PRECIOUS: Tests/%
Tests/%: Tests/%.c $(TARGET)
	$(CC) -I. $< -L. -lft_malloc -Wl,-rpath='$$ORIGIN/..' -o $@.test
	$@.test

tests: $(addprefix Tests/,$(TESTS))

-include $(addprefix $(BUILD_DIR)/,$(DEP_FILES))

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(TARGET)
	rm -f $(addsuffix .test,$(addprefix Tests/,$(TESTS)))

re: | fclean all

.PHONY: all clean fclean re

