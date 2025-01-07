NAME=libft_malloc.a

SRC_DIR=Source
SRC_FILES=bucket_alloc.c big_alloc.c malloc.c
OBJ_DIR=Obj

DEFINES?=#FT_MALLOC_DEBUG_LOG

OBJ_FILES=$(SRC_FILES:.c=.o)
CC=gcc
C_FLAGS=-ggdb -I. $(addprefix -D,$(DEFINES)) #-Wall -Wextra -Werror

TESTS=main alloc_performance general free gburgun
# We cannot enable optimizations for tests because calls to
# malloc would be stripped away in many circumstances
TEST_C_FLAGS:=$(C_FLAGS)

C_FLAGS:=$(C_FLAGS) -O3

all: $(NAME)

.PRECIOUS: $(OBJ_DIR)/%.o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c malloc.h $(SRC_DIR)/malloc_internal.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c $< -o $@

$(NAME): $(addprefix $(OBJ_DIR)/,$(OBJ_FILES)) Makefile
	ar rcs $@ $(addprefix $(OBJ_DIR)/,$(OBJ_FILES))

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)
	rm -f Tests/*.test

re: fclean all

.PRECIOUS: Tests/%
Tests/%: Tests/%.c $(NAME)
	$(CC) $(TEST_C_FLAGS) $< $(NAME) -o $@.test
	./$@.test

tests: $(addprefix Tests/,$(TESTS))

.PHONY: all clean fclean re tests
