# audiosplitter

## Description
Splits audio in different files, each corresponding to a channel identified from the input file.


## Compilation system/ Dependencies:

The compilation system is run on UNIX. To run the program, the user must have installed:
[*] ffmpeg libraries (see https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu) 
[*] cmake (see https://cmake.org/install/).

## Instructions

1) To run the code, open the zipped archive or download it from https://github.com/throwaway31313/audiosplitter.

2) In the working directory, export the "PKG_CONFIG_PATH" to the "pkgconfig" folder, found in /bin/ of the ffmpeg installation directory.

>> export PKG_CONFIG_PATH= -DIRECTORY-TO-PKCONFIG-

If the library was installed via the link above, then simply run:

>> export PKG_CONFIG_PATH=~/ffmpeg_build/lib/pkgconfig/

3) Now, type "make" and the program should compile.

4) To run the program, go to the bin directory by typing "cd bin" and type:

>> ./audiosplitter.out "input_file_name"

5) If the program ran correctly, multiple files should appear in the /bin directory,
each corresponding to a different channel in the original audio file.

6) To play the generated audio files, type the command indicated by the program.
