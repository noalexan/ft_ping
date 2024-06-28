ifeq ($(MAKE_BUILD_TYPE), Release)
	CFLAGS=-Wall -Wextra -Werror -O2 -Os
else
	CFLAGS=-Wall -Wextra -g -fsanitize=memory
	LDFLAGS=-fsanitize=memory
endif

NAME=ft_ping

SRC=main.c ft_ping.c
OBJ=$(SRC:.c=.o)

.PHONY: all
all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) $(OBJ)

.PHONY: fclean
fclean:
	$(RM) $(NAME) $(OBJ)

.PHONY: re
re: fclean all
