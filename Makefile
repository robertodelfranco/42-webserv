NAME	=	webserv
CC		=	c++
FLAGS	=	-Wall -Werror -Wextra -std=c++98

SRC_DIR	=	src/
DIRS	=	./src/Utils ./src/Config ./src/Parser ./src/Network
SRCS	=	main.cpp \
# 			$(addprefix $(DIRS[0])/, Utils.cpp) \
# 			$(addprefix $(DIRS[1])/, Config.cpp ConfigError.cpp Location.cpp Server.cpp) \
# 			$(addprefix $(DIRS[2])/, Parser.cpp ParserError.cpp) \
# 			$(addprefix $(DIRS[3])/, Network.cpp ClientConnection.cpp Response.cpp Request.cpp)

MAGENTA	=	\033[1;95m
YELLOW	=	\033[1;93m
GREEN	=	\033[1;92m
CYAN	=	\033[1;96m
RED		=	\033[1;91m
NC		=	\033[0m

OBJS	=	$(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.cpp=.o)))
OBJ_DIR	=	objs

all:
	@if $(MAKE) -q ${NAME} 2>/dev/null; then \
		echo "${YELLOW}✅ ${NAME} already compiled!${NC}"; \
	else \
		$(MAKE) ${NAME} --no-print-directory; \
	fi

${NAME}: ${OBJS}
	@$(CC) $(OBJS) -o $(NAME)
	@echo "${CYAN}$(NAME) compiled successfully!${NC}"

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@${CC} ${FLAGS} -o $@ -c $<


clean:
	@rm -rf ${OBJS}
	@if [ -d "$(OBJ_DIR)" ]; then \
		if [ ! "$(ls -A $(OBJ_DIR))" ]; then \
			rmdir $(OBJ_DIR); \
		fi \
	fi
	@echo "${RED}Object files and empty object directory cleaned!${NC}"

fclean:	clean
	@rm -rf ${NAME}
	@rm -rf ShrubberyTarget_shrubbery
	@echo "${RED}All files cleaned!${NC}"

re: fclean all

.PHONY: all clean fclean re