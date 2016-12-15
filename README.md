# NV12Tile-To-NV12-Conversion
NV12Tile to NV12 Conversion

This algorithm is checked with 480p,720p, 1080p and other resolutions. Videos both in landscape and portrait mode are been able to be converted from nv12 tiled to nv12 format using this algorithm.

For memory layout please refer:
https://linuxtv.org/downloads/v4l-dvb-apis/re36.html

NOTE : Input file must be a yuv file in NV12Tile format. See instructions file under examples folder for more details.

SUGGESTION : To get output in yuv420 planner format, change value of OUTPUT_FORMAT macro from "nv12" to "yuv420p" and recompile.
