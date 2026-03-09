NAME	=	gomoku

SRCS	=	src/main.cpp	\
			src/error.cpp \
			src/utils.cpp \
			src/algo.cpp \
			src/front.cpp 

		
OBJ =	${SRCS:.c=.o}

CC =	cpp

CFLAGS	=	 -Wall -Wextra -Werror -g #-fsanitize=address

.c.o:
			$(CC) $(CFLAGS) -c $< -o $(<:.c=.o)

$(NAME): ${OBJ}
		$(CC) $(CFLAGS) -o $(NAME) ${OBJ}

all:	${NAME}

clean:
		rm -f ${OBJ}

fclean:	clean
		rm -f ${NAME}

re:	fclean all

.PHONY: all clean fclean re