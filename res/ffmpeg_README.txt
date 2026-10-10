Put the FFmpeg builds for the Recorder here (Windows and macOS only):

  ffmpeg.exe   Windows 64-bit build
  ffmpeg_mac   macOS build (use a universal build, or build per CPU: arm64 / x86_64)

Use an LGPL build (no --enable-gpl, no --enable-nonfree) so the mod can be shared.
It needs the encoders you want to offer: libx264 is GPL, so for an LGPL build rely on the
hardware encoders (h264_nvenc, h264_amf, h264_qsv, h264_videotoolbox) and the built in aac.
Version 6.0 or newer is recommended.

This text file only exists so the "res/ffmpeg*" pattern in mod.json always matches something.
