NAME := webserv

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
CPPFLAGS := -Iincludes -Iincludes/Config -Iincludes/HTTP -Iincludes/Server

SRCS := srcs/main.cpp \
	srcs/Config/Config.cpp \
	srcs/Config/ConfigError.cpp \
	srcs/Config/ConfigLexer.cpp \
	srcs/Config/ConfigParser.cpp \
	srcs/Config/ConfigValidator.cpp \
	srcs/HTTP/HttpHelper.cpp \
	srcs/HTTP/HttpMethod.cpp \
	srcs/HTTP/HttpParser.cpp \
	srcs/HTTP/HttpRequest.cpp \
	srcs/HTTP/HttpSyntax.cpp \
	srcs/HTTP/Router.cpp \
	srcs/Server/Connection.cpp \
	srcs/Server/EventLoop.cpp
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

clean:
	$(RM) $(OBJS) $(DEPS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

-include $(DEPS)
