# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -O3

# Include & libraries (assuming homebrew standard location for SFML on mac)
# If SFML is installed differently, paths will need adjustment.
INCLUDES = -I/opt/homebrew/include -I./src
LIBS = -L/opt/homebrew/lib -lsfml-graphics -lsfml-window -lsfml-system

# Binary
NAME = Gomoku

# Sources
SRCS = src/main.cpp \
       src/front/GameUI.cpp \
       src/front/Input.cpp \
       src/engine/Rules.cpp \
	   src/Gomoku.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
