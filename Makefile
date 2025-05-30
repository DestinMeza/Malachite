OUTPUT = -o libs/libmalachite.so
OUTPUT_OPTIONS = -fpic -shared

AGGREGATE_OUTPUT = -o ../aggregate/libs/libmalachite.so

COMPILED_FILES = src/*cpp src/core/*.cpp
INCLUDE_LIBS = \
	-I include/ \
	-I include/core/

EXPORT = -o /usr/lib/libmalachite.so

malachite:
	g++ $(EXPORT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS)

local:
	g++ $(OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS)
	g++ $(AGGREGATE_OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS)

clean:
	rm -f libs/libmalachite.so
	rm -f ../aggregate/libs/libmalachite.so
	rm -f /usr/lib/libmalachite.so