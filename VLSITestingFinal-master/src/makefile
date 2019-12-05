#

CXX      := g++
CXXFLAGS := -std=c++11 -Wall
OBJS     := $(subst .cpp,.o,$(wildcard *.cpp))
PROG     := atpg

.PHONY: default debug release clean direct

default: release

debug: CXXFLAGS += -g
debug: ${PROG}

release: CXXFLAGS += -O2 -DNDEBUG
release: ${PROG}

${PROG}: ${OBJS}
	${CXX} ${CXXFLAGS} $^ -lm -o $@

# automatic dependency generation
-include $(subst .o,.d,${OBJS})

%.o: %.d

%.d: %.cpp
	${CXX} ${CXXFLAGS} -MM -MT "$(subst .d,.o,$@) $@" $< > $@
# end automatic dependency generation

clean:
	rm ${PROG}
	rm *.{d,o}

direct:
	${CXX} $(CXXFLAGS) $(OBJS:.o=.cpp) -o ${PROG}
