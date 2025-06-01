CFLAGS = -std=c++17 -O2
EX_LDDEP_FLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

OUTPUT = -o libs/libmalachite.so
OUTPUT_OPTIONS = -fpic -shared

AGGREGATE_OUTPUT = -o ../aggregate/libs/libmalachite.so

COMPILED_FILES = \
	src/*cpp \
	src/core/*.cpp \
	src/render/*.cpp

INCLUDE_LIBS = \
	-I include/ \
	-I include/core/ \
	-I include/render/

EXPORT = -o /usr/lib/libmalachite.so

.PHONY: local clean

malachite:
	g++ $(CFLAGS) $(EXPORT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS) $(EX_LDDEP_FLAGS)
	g++ $(CFLAGS) $(OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS) $(EX_LDDEP_FLAGS)
	g++ $(CFLAGS) $(AGGREGATE_OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS) $(EX_LDDEP_FLAGS)

local:
	g++ $(CFLAGS) $(OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS) $(EX_LDDEP_FLAGS)
	g++ $(CFLAGS) $(AGGREGATE_OUTPUT) $(OUTPUT_OPTIONS) $(COMPILED_FILES) $(INCLUDE_LIBS) $(EX_LDDEP_FLAGS)

clean:
	rm -f libs/libmalachite.so
	rm -f ../aggregate/libs/libmalachite.so
	rm -f /usr/lib/libmalachite.so