ifeq ($(MAKE_BUILD_TYPE), Release)
	CFLAGS=-Wall -Wextra -Werror -O3 -Os
else
	CFLAGS=-Wall -Wextra -g
endif

LDFLAGS=-lm

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
fclean: clean
	$(RM) $(NAME)

.PHONY: re
re: fclean all
