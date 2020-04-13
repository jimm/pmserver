NAME = pmserver
LIBS = -lc -lc++ -lportmidi
LDFLAGS += $(LIBS)

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

SRC = $(wildcard src/*.cpp)
OBJS = $(SRC:%.cpp=%.o)
TEST_SRC = $(wildcard test/*.cpp)
TEST_OBJS = $(TEST_SRC:%.cpp=%.o)
TEST_OBJ_FILTERS = src/$(NAME).o

.PHONY: all test install clean distclean
all: $(NAME)

$(NAME): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

-include $(C_SRC:%.c=%.d)
-include $(CPP_SRC:%.cpp=%.d)

test: $(NAME)_test
	./$(NAME)_test

$(NAME)_test:	$(OBJS) $(TEST_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(filter-out $(TEST_OBJ_FILTERS),$^)

install:	$(bindir)/$(NAME)

$(bindir)/$(NAME):	$(NAME)
	cp ./$(NAME) $(bindir)
	chmod 755 $(bindir)/$(NAME)

clean:
	rm -f $(NAME) $(NAME)_test src/*.o test/*.o

distclean: clean
	rm -f src/*.d test/*.d
