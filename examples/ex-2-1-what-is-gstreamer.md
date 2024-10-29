# What is Gstreamer?

- Based on "plugins".
- The plugins can be "linked".

## Plugins

GStreamer plug-ins could be classified into
- protocols handling
- sources: for audio and video (involves protocol plugins)
- formats: parsers, formaters, muxers, demuxers, metadata, subtitles
- codecs: coders and decoders
- filters: converters, mixers, effects, ...
- sinks: for audio and video (involves protocol plugins)

![](../assets/gstreamer-overview.png)

GStreamer is packaged into
- gstreamer: the core package
- gst-plugins-base: an essential exemplary set of elements
- gst-plugins-good: a set of good-quality plug-ins under LGPL
- gst-plugins-ugly: a set of good-quality plug-ins that might pose distribution problems
- gst-plugins-bad: a set of plug-ins that need more quality
- gst-libav: a set of plug-ins that wrap libav for decoding and encoding
- a few others packages

## Design Principles

- **Object oriented**: GStreamer adheres to `GObject`, the `GLib 2.0` object model. GStreamer intends to be similar in programming methodology to `GTK+`.
- **Extensible**: All GStreamer Objects can be extended using the `GObject` inheritance methods.
- **Allow binary-only plugins**: Plugins are shared libraries that are loaded at runtime. Special care has been taken to make plugins completely self-contained.

## References

1. https://gstreamer.freedesktop.org/documentation/application-development/introduction/gstreamer.html?gi-language=python
2. https://gstreamer.freedesktop.org/documentation/application-development/introduction/motivation.html?gi-language=python
