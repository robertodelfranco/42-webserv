NAME	=	webserv
CC		=	c++
FLAGS	=	-Wall -Werror -Wextra -std=c++98

SRC_DIR	=	src
SRCS	=	main.cpp \
			Config/Config.cpp \
			Config/ConfigLexer.cpp \
			Config/ConfigParser.cpp \
			Config/ConfigParseError.cpp \
			Config/Token.cpp \
			Network/FileDescriptor.cpp \
			Network/Socket.cpp \
			Network/Connection.cpp \
			Network/EventLoop.cpp \
			Network/Router.cpp \
			ServerConfig/ServerConfig.cpp \
			ServerConfig/Location.cpp \
			ServerConfig/Listen.cpp \
			Utils/Utils.cpp \
			Response/HttpResponse.cpp \
			Request/HttpRequest.cpp \
			Request/HttpParser.cpp \


MAGENTA	=	\033[1;95m
YELLOW	=	\033[1;93m
GREEN	=	\033[1;92m
CYAN	=	\033[1;96m
RED		=	\033[1;91m
NC		=	\033[0m

OBJ_DIR	=	objs
OBJS	=	$(patsubst %.cpp, $(OBJ_DIR)/%.o, $(SRCS))
VPATH	=	$(SRC_DIR):Config:ServerConfig:Utils:Parser:Network

all:
	@if $(MAKE) -q ${NAME} 2>/dev/null; then \
		echo "${YELLOW}✅ ${NAME} already compiled!${NC}"; \
	else \
		$(MAKE) ${NAME} --no-print-directory; \
	fi

${NAME}: ${OBJS}
	@$(CC) $(OBJS) -o $(NAME)
	@echo "${CYAN}$(NAME) compiled successfully!${NC}"

$(OBJ_DIR)/%.o : %.cpp
	@mkdir -p $(dir $@)
	@${CC} ${FLAGS} -o $@ -c $<


clean:
	@rm -rf $(OBJ_DIR)
	@echo "${RED}Object files and empty object directory cleaned!${NC}"

fclean:	clean
	@rm -rf ${NAME}
	@rm -rf ShrubberyTarget_shrubbery
	@echo "${RED}All files cleaned!${NC}"

re: fclean all

.PHONY: all clean fclean re