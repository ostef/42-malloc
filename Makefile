NAME=libft_malloc.a

SRC_DIR=Source
SRC_FILES=bucket_alloc.c malloc.c
OBJ_DIR=Obj

DEFINES?=#FT_MALLOC_DEBUG_LOG FT_MALLOC_MIN_ALLOC_CAPACITY=100

OBJ_FILES=$(SRC_FILES:.c=.o)
CC=gcc
C_FLAGS=-Wall -Wextra -Werror -ggdb -I. $(addprefix -D,$(DEFINES))

all: $(NAME)

.PRECIOUS: $(OBJ_DIR)/%.o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c malloc.h $(SRC_DIR)/malloc_internal.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c $< -o $@

$(NAME): $(addprefix $(OBJ_DIR)/,$(OBJ_FILES))
	ar rcs $@ $(addprefix $(OBJ_DIR)/,$(OBJ_FILES))

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

# .PRECIOUS: Tests/%
Tests/%: Tests/%.c $(NAME)
	$(CC) $(C_FLAGS) $< $(NAME) -o $@.test
	./$@.test
	@rm -f $@.test

.PHONY: all clean fclean re
