# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/students/200111223/user-land-filesystem/fs/newfs

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/students/200111223/user-land-filesystem/fs/newfs/build

# Include any dependencies generated for this target.
include CMakeFiles/newfs.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/newfs.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/newfs.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/newfs.dir/flags.make

CMakeFiles/newfs.dir/src/debug.c.o: CMakeFiles/newfs.dir/flags.make
CMakeFiles/newfs.dir/src/debug.c.o: ../src/debug.c
CMakeFiles/newfs.dir/src/debug.c.o: CMakeFiles/newfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/students/200111223/user-land-filesystem/fs/newfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/newfs.dir/src/debug.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/newfs.dir/src/debug.c.o -MF CMakeFiles/newfs.dir/src/debug.c.o.d -o CMakeFiles/newfs.dir/src/debug.c.o -c /home/students/200111223/user-land-filesystem/fs/newfs/src/debug.c

CMakeFiles/newfs.dir/src/debug.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/newfs.dir/src/debug.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/students/200111223/user-land-filesystem/fs/newfs/src/debug.c > CMakeFiles/newfs.dir/src/debug.c.i

CMakeFiles/newfs.dir/src/debug.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/newfs.dir/src/debug.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/students/200111223/user-land-filesystem/fs/newfs/src/debug.c -o CMakeFiles/newfs.dir/src/debug.c.s

CMakeFiles/newfs.dir/src/newfs.c.o: CMakeFiles/newfs.dir/flags.make
CMakeFiles/newfs.dir/src/newfs.c.o: ../src/newfs.c
CMakeFiles/newfs.dir/src/newfs.c.o: CMakeFiles/newfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/students/200111223/user-land-filesystem/fs/newfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/newfs.dir/src/newfs.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/newfs.dir/src/newfs.c.o -MF CMakeFiles/newfs.dir/src/newfs.c.o.d -o CMakeFiles/newfs.dir/src/newfs.c.o -c /home/students/200111223/user-land-filesystem/fs/newfs/src/newfs.c

CMakeFiles/newfs.dir/src/newfs.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/newfs.dir/src/newfs.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/students/200111223/user-land-filesystem/fs/newfs/src/newfs.c > CMakeFiles/newfs.dir/src/newfs.c.i

CMakeFiles/newfs.dir/src/newfs.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/newfs.dir/src/newfs.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/students/200111223/user-land-filesystem/fs/newfs/src/newfs.c -o CMakeFiles/newfs.dir/src/newfs.c.s

CMakeFiles/newfs.dir/src/utils.c.o: CMakeFiles/newfs.dir/flags.make
CMakeFiles/newfs.dir/src/utils.c.o: ../src/utils.c
CMakeFiles/newfs.dir/src/utils.c.o: CMakeFiles/newfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/students/200111223/user-land-filesystem/fs/newfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object CMakeFiles/newfs.dir/src/utils.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/newfs.dir/src/utils.c.o -MF CMakeFiles/newfs.dir/src/utils.c.o.d -o CMakeFiles/newfs.dir/src/utils.c.o -c /home/students/200111223/user-land-filesystem/fs/newfs/src/utils.c

CMakeFiles/newfs.dir/src/utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/newfs.dir/src/utils.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/students/200111223/user-land-filesystem/fs/newfs/src/utils.c > CMakeFiles/newfs.dir/src/utils.c.i

CMakeFiles/newfs.dir/src/utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/newfs.dir/src/utils.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/students/200111223/user-land-filesystem/fs/newfs/src/utils.c -o CMakeFiles/newfs.dir/src/utils.c.s

# Object files for target newfs
newfs_OBJECTS = \
"CMakeFiles/newfs.dir/src/debug.c.o" \
"CMakeFiles/newfs.dir/src/newfs.c.o" \
"CMakeFiles/newfs.dir/src/utils.c.o"

# External object files for target newfs
newfs_EXTERNAL_OBJECTS =

newfs: CMakeFiles/newfs.dir/src/debug.c.o
newfs: CMakeFiles/newfs.dir/src/newfs.c.o
newfs: CMakeFiles/newfs.dir/src/utils.c.o
newfs: CMakeFiles/newfs.dir/build.make
newfs: /usr/lib/x86_64-linux-gnu/libfuse.so
newfs: /home/students/200111223/lib/libddriver.a
newfs: CMakeFiles/newfs.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/students/200111223/user-land-filesystem/fs/newfs/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking C executable newfs"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/newfs.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/newfs.dir/build: newfs
.PHONY : CMakeFiles/newfs.dir/build

CMakeFiles/newfs.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/newfs.dir/cmake_clean.cmake
.PHONY : CMakeFiles/newfs.dir/clean

CMakeFiles/newfs.dir/depend:
	cd /home/students/200111223/user-land-filesystem/fs/newfs/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/students/200111223/user-land-filesystem/fs/newfs /home/students/200111223/user-land-filesystem/fs/newfs /home/students/200111223/user-land-filesystem/fs/newfs/build /home/students/200111223/user-land-filesystem/fs/newfs/build /home/students/200111223/user-land-filesystem/fs/newfs/build/CMakeFiles/newfs.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/newfs.dir/depend

