NAME = pmserver
LIBS = -lc -lc++ -lportmidi
LDFLAGS += $(LIBS)

SRC = $(wildcard src/*.cpp)
OBJS = $(SRC:%.cpp=%.o)
TEST_SRC = $(wildcard test/*.cpp)
TEST_OBJS = $(TEST_SRC:%.cpp=%.o)
TEST_OBJ_FILTERS = src/$(NAME).o

.PHONY: all
all: $(NAME)

$(NAME): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

-include $(C_SRC:%.c=%.d)
-include $(CPP_SRC:%.cpp=%.d)

.PHONY: test
test: $(NAME)_test
	./$(NAME)_test

$(NAME)_test:	$(OBJS) $(TEST_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(filter-out $(TEST_OBJ_FILTERS),$^)

.PHONY: clean
clean:
	rm -f $(NAME) $(NAME)_test src/*.o test/*.o

.PHONY: distclean
distclean: clean
	rm -f src/*.d test/*.d
