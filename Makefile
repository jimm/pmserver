NAME = pmserver
CPPFLAGS += -std=c++14
LIBS = -lportmidi
LDFLAGS += $(LIBS)

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

SRC = $(wildcard src/*.cpp)
OBJS = $(SRC:%.cpp=%.o)
TEST_SRC = $(wildcard test/*.cpp)
TEST_OBJS = $(TEST_SRC:%.cpp=%.o)
TEST_OBJ_FILTERS = src/$(NAME).o
TEST_LIBS = $(LIBS) -lCatch2Main -lCatch2

%.d: %.cpp
	@set -e; rm -f $@; \
	 $(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$


.PHONY: all test install uninstall tags clean distclean
all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

-include $(SRC:%.cpp=%.d)

test: $(NAME)_test
	./$(NAME)_test

$(NAME)_test:	$(OBJS) $(TEST_OBJS)
	$(CXX) $(LDFLAGS) $(TEST_LIBS) -o $@ $(filter-out $(TEST_OBJ_FILTERS),$^)

install:	$(bindir)/$(NAME)

$(bindir)/$(NAME):	$(NAME)
	cp ./$(NAME) $(bindir)
	chmod 755 $(bindir)/$(NAME)

uninstall:
	rm -f $(bindir)/$(name)

tags:	TAGS

TAGS:	$(SRC)
	etags $(SRC)

clean:
	rm -f $(NAME) $(NAME)_test src/*.o test/*.o

distclean: clean
	rm -f src/*.d test/*.d
