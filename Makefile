MAKE_INC=make.inc
include $(MAKE_INC)

HEADERNAMES = lazygaspi_hs.h
DEPS = include/lazygaspi_hs.h src/gaspi_utils.h src/utils.h
OBJS = bin/init.o bin/general.o bin/read.o bin/write.o bin/prefetch.o
OUTPUT_FILE_FORMAT=lazygaspi_hs_*.out

ifeq "$(LIB_STATIC)" "1"
  LIBNAME_EXT=lib$(LIBNAME).a
else
  LIBNAME_EXT=lib$(LIBNAME).so
endif

ifneq "$(WITH_MPI)" "1"
  COMPILER=$(CXX)
  CXXFLAGS+=-pthread
else ifdef "$(MPI_PATH)"
  COMPILER=$(MPI_PATH)/bin/mpicxx
else
  COMPILER=mpicxx
endif

MACHINEFILE=machinefile

TESTS = test0

DEFAULT_test0 = -n 4 -k 5 -r 10 -2 12

DIR_TESTS=$(PREFIX)/tests
DIR_LIB=$(PREFIX)/lib
DIR_INCLUDE=$(PREFIX)/include

SHARED_LIB_DIR=/usr/local/lib

TESTSCRIPT=$(DIR_TESTS)/run_test.sh
TESTSCRIPTALL=$(DIR_TESTS)/run_all.sh
SCRIPTS = $(TESTSCRIPT) $(TESTSCRIPTALL)


.PHONY: clean uninstall remove_all install tests $(TESTS) test_script move all

all: install tests

install: $(OBJS)
	@mkdir -p $(PREFIX)/lib
	@mkdir -p $(PREFIX)/include
	@cp include/* $(PREFIX)/include
	@if test "$(LIB_STATIC)" = "1"; then\
		ar rcs $(DIR_LIB)/$(LIBNAME_EXT) $(OBJS);\
		echo "Library successfully installed at $(DIR_LIB) as $(LIBNAME_EXT)!";\
	else\
		$(CXX) -shared $(OBJS) -o $(DIR_LIB)/$(LIBNAME_EXT);\
		echo "Library successfully installed at $(DIR_LIB) as $(LIBNAME_EXT)!";\
		echo "Make sure to move this to a directory that can be found by the dynamic linker.";\
		echo "You can use 'make move' to move it to $(SHARED_LIB_DIR)";\
	fi

#							TESTS TARGET

tests:
	@if [ ! -f $(DIR_LIB)/$(LIBNAME_EXT) ]; then\
	 echo "You need to install library first! Use 'make install';" false; fi
	@if [ -z $(EIGEN) ]; then echo "Tests require Eigen. Use --eigen=<path> to \
									indicate a path to the Eigen headers (or \
									install it from eigen.tuxfamily.org)";\
	 false; fi
	@mkdir -p $(DIR_TESTS)
	@$(foreach t, $(TESTS), $(MAKE) -s $(t);)
	@$(MAKE) -s test_script
	@if [ ! -z $(PREMADE_MF) ] && [ -f $(PREMADE_MF) ]; then\
	 mv $(PREMADE_MF) $(DIR_TESTS)/$(MACHINEFILE); fi
	@echo "Tests successfully installed!"

$(TESTS):	
	@$(COMPILER) $(CXXFLAGS) $(INCLUDES) -I$(EIGEN) src/$@.cpp\
 -o $(DIR_TESTS)/$@ -L$(PREFIX)/lib $(LDFLAGS) -l$(LIBNAME) $(LDLIBS) $(EXTRALIBS);

test_script:
	@rm -f $(TESTSCRIPT) $(TESTSCRIPTALL)
	@echo "if [ ! -f $(MACHINEFILE) ]; then echo \"Could not find \
$(MACHINEFILE).\"; fi" >> $(TESTSCRIPT)
	@if test "$(WITH_MPI)" = "1"; then\
		if [ -z "$(MPI_PATH)" ]; then\
			echo "mpirun -hostfile $(MACHINEFILE) \$$1 \$${@:2}" >> $(TESTSCRIPT);\
		else\
			echo "$(MPI_PATH)/bin/mpirun -hostfile $(MACHINEFILE) \$$1 \$${@:2}"\
				>> $(TESTSCRIPT);\
		fi;\
	else echo "gaspi_run -m $(MACHINEFILE) \$$1 \$${@:2}" >> $(TESTSCRIPT); fi
	@echo "cd \$$(dirname \$$0)" >> $(TESTSCRIPTALL)
	@$(foreach t, $(TESTS),\
		echo "if [ \$$# = 0 ]; then ARGS=\"$(DEFAULT_$(t))\"; else ARGS=\$$@; fi" >> $(TESTSCRIPTALL);\
	 	echo "./run_test.sh $(t) \$$ARGS" >> $(TESTSCRIPTALL);)
	@chmod a+x $(TESTSCRIPT) $(TESTSCRIPTALL)


#						CLEAN/UNINSTALL TARGETS

remove_all:
	@$(MAKE) -s uninstall
	@$(MAKE) -s clean

clean:
	@rm -fr bin

uninstall:
	@rm -f  $(DIR_LIB)/$(LIBNAME_EXT)
	@if [ -d $(DIR_LIB) ] && [ -z "$$(ls -A $(DIR_LIB))" ]; then rmdir $(DIR_LIB); fi
	@$(foreach h, $(HEADERNAMES), rm -f  $(DIR_INCLUDE)/$(h); )
	@if [ -d $(DIR_INCLUDE) ] && [ -z "$$(ls -A $(DIR_INCLUDE))" ]; then\
		rmdir $(DIR_INCLUDE); fi
	@$(foreach t, $(TESTS), rm -f $(DIR_TESTS)/$(t); )
	@$(foreach script, $(SCRIPTS), rm -f $(script); )
	@if [ ! -z $(PREMADE_MF) ] && [ -f $(DIR_TESTS)/$(MACHINEFILE) ]; then\
	 mv $(DIR_TESTS)/$(MACHINEFILE) $(PREMADE_MF); fi
	@rm -f $(DIR_TESTS)/$(OUTPUT_FILE_FORMAT)
	@if [ -d $(DIR_TESTS) ] && [ -z "$$(ls -A $(DIR_TESTS))" ]; then\
		rmdir $(DIR_TESTS); fi
	@if [ -d $(PREFIX) ] && [ -z "$$(ls -A $(PREFIX))" ]; then\
		rmdir $(PREFIX); fi

#						OBJECT FILES TARGET

bin/%.o : src/%.cpp $(DEPS)
	@mkdir -p bin
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ $(LDFLAGS) $(LDLIBS)

#						MOVE SHARED LIB TARGET

ifeq "$(LIB_STATIC)" "1"
move:
	@echo "No need to move static library. This is only meant for shared libraries..."
else
move:
	@echo "Moving shared library to $(SHARED_LIB_DIR)..."
	@sudo cp -fv $(DIR_LIB)/$(LIBNAME_EXT) $(SHARED_LIB_DIR)
endif























