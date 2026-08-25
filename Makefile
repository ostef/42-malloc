TARGET=libft_malloc.so
TEST_TARGET=dburgun

SRC_DIR=Source
INCLUDE_DIRS=.
BUILD_DIR=.build

SRC_FILES=malloc.c
DEP_FILES=$(addsuffix .d,$(SRC_FILES))
OBJ_FILES=$(addsuffix .o,$(SRC_FILES))

LIB_DIRS=
LIBS=

CC=gcc
C_FLAGS=-Wall -Wextra -Werror -g
DEFINES=#FT_MALLOC_DEBUG_LOG VERIFY_LIST

all: $(TARGET) $(TEST_TARGET)

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) $(addprefix -D,$(DEFINES)) -MMD -MP -MF$(BUILD_DIR)/$*.c.d $(addprefix -I,$(INCLUDE_DIRS)) -c $< -o $@

$(TARGET): $(addprefix $(BUILD_DIR)/,$(OBJ_FILES))
	$(CC) -shared $(addprefix $(BUILD_DIR)/,$(OBJ_FILES)) $(addprefix -L,$(LIB_DIRS) $(LIB_DIRS)) $(addprefix -l,$(LIBS) $(LIBS)) -o $@

$(TEST_TARGET): $(TARGET) $(TEST_TARGET).c
	$(CC) $(C_FLAGS) -I. $(TEST_TARGET).c -L. -lft_malloc -Wl,-rpath='$$ORIGIN' -o $(TEST_TARGET)

-include $(addprefix $(BUILD_DIR)/,$(DEP_FILES))

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(TARGET)

re: | fclean all

.PHONY: all clean fclean re

