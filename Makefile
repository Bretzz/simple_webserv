SERV		:=tcpserv
STT			:=stt

# flags
CXX			:= c++
CXXFLAGS	:= -Wall -Werror -Wextra -std=c++11

SERV_SRCS	:= main.cpp \
			tcpserv.cpp oaklog.cpp

OBJ_DIR		:= obj/
SERV_OBJS	:= $(addprefix $(OBJ_DIR), $(SERV_SRCS:.cpp=.o))

all: $(SERV)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)%.o : %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(SERV): $(SERV_OBJS)
	$(CXX) $(CXXFLAGS) $(SERV_OBJS) -o $@

clean:
	rm $(SERV_OBJS)

fclean:
	rm -rf $(OBJ_DIR)

re: fclean all

.PHONY: all clean fclean