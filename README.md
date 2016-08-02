# NV12Tile-To-NV12-Conversion
NV12Tile to NV12 Conversion

This algo is checked with 480p,720p and 1080p resolutions.

For memory layout please refer:
https://linuxtv.org/downloads/v4l-dvb-apis/re36.html

NOTE :- Input file must be a yuv file in NV12Tile format.

To get output in yuv420 planner format, change value of
OUTPUT_FORMAT macro from "nv12" to "yuv420p" and recompile.
