# About
This repository is a container for single-file, public domain libraries written in C. Each
file in this library are independant of the others, except where noted.

- dr_vfs: A library for loading files from both the native file system and archives
such as Zip files.
- dr_fsw: A library for watching for changes to the file system.
- dr_path: A path manipulation library.
- dr_gui: A low-level GUI library.
- dr_util: A library with miscellaneous functionality that doesn't really belong to
any particular category.
- dr_math: A very incomplete vector math library. You don't want to be using this
right now.
- dr_audio: A simple library for audio playback and recording. Very incomplete and
work-in-progress.
- dr_wav: A simple library for reading audio data from .wav files.
- dr_flac: A simple library for decoding FLAC files.
- dr_2d: A work-in-progress library for drawing 2D graphics.
- dr_mtl: A compiler for materials for use in graphics software. Takes a material file
(such as Wavefront MTL files), compiles it into an intermediate bytecode representation,
then generates shader code (GLSL, etc.)