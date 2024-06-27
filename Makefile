CFLAGS=-Wall -Wextra

NAME=ft_ping

SRC=main.c
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
