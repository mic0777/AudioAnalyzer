## AudioAnalyzer: find musical key and tempo of compressed audio files (MP3, FLAC, etc)

This small utility creates a CSV file with the following parameters for all audio files in the given folder:

File Name,Duration,Frequency,Key,Tempo

Each line in the CSV file corresponds the audio file with File Name from the specified folder.
All audio formats supported by FFMPEG library (including WAV, MP3, FLAC, etc) can be used.

It is optimized to run in multiple threads to process huge number of files quickly.

The following opensource libraries are required:

[ffmpeg](https://github.com/FFmpeg/FFmpeg/) to decode compressed audio file

[libKeyFinder](https://github.com/ibsh/libKeyFinder/) to estimate the musical key of many different audio file formats.

[aubio](https://github.com/aubio/aubio/) to estimate tempo

[![Build Status](https://github.com/mic0777/AudioAnalyzer/workflows/build/badge.svg)](https://github.com/mic0777/AudioAnalyzer/actions?query=workflow%3Abuild)

### Usage

```sh
$ ./AudioAnalyzer <folder with audio files> <result CSV file path>
```

### Building

You will need to have the following dependencies installed on your machine

- [ffmpeg](https://www.ffmpeg.org/) 
- [fftw3](https://fftw.org)
- [libkeyfinder](https://github.com/mixxxdj/libkeyfinder/)
- [libaubio](https://github.com/aubio/aubio/)

As long as these dependencies are installed then you should be able to
simply type:

```
$ cmake .
$ make
```
If you have any problems with compilation, please view this file (with exact steps to build it on fresh Ubuntu):
[main.yml](https://github.com/mic0777/AudioAnalyzer/blob/9d480adf7081de81367e98f5fe166d2dacc77264/.github/workflows/main.yml)

