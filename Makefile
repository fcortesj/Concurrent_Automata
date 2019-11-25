all: src/sisctrl

CXXFLAGS=-g
LDFLAGS=-lyaml-cpp

INVOICE_OBJECTS=src/sisctrl.o

src/sisctrl: $(INVOICE_OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/sisctrl.o: src/sisctrl.cc src/edge.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/*.cc~ Makefile~ src/sisctrl