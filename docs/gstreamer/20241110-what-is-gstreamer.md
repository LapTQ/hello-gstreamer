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

![](../../assets/gstreamer-overview.png)

## GStreamer packages
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
- **Allow binary-only plugins**: Plugins are shared libraries that are loaded at runtime. Since all the properties of the plugin can be set using the `GObject` properties, there is no need (and in fact no way) to have any header files installed for the plugins. Special care has been taken to make plugins completely self-contained.

## Foundation

![](../../assets/gs-simple-player.png)

![](../../assets/gstreamer-communication.png)

* ***Elements***:
    * You will usually create a chain of elements linked together and let data flow through this chain of elements.
    * An element has one specific function.
    * GStreamer ships with a large collection of elements by default. If needed, you can also write new elements. That topic is explained in greater detail in the *GStreamer Plugin Writer's Guide*.
* ***Pads***:
    * They are used to negotiate links and data flow between elements, can be viewed as a  “port” on an element.
    * Links are only allowed between two pads when the allowed data types (capabilities) of the two pads are compatible.
    * Data usually means ***buffers*** (`GstBuffer`) and ***events*** (`GstEvent`).
* ***Bins*** and ***Pipelines***:
    * A bin is a special (subclass) of elements themselves used to contain a collection of elements. Therefore, you can, for example, change state on all elements in a bin by changing the state of that bin itself.
    * Bins also forward bus messages from their contained children.
    * A pipeline is a top-level bin. It provides a bus for the application. It manages the synchronization for its children.
    * Once started, pipelines will run in a separate thread.
* ***Communication***:
    * ***Buffers***: object sent between elements. Buffers always travel from sources to sinks.
    * ***Events***: object sent between elements or from the application to elements.
    * ***Messages***: object that elements post to the pipeline's message bus, where they will be held for collection by the application synchronously or asynchronously.
    * ***Queries***: allow applications to request information. Elements can also use queries to request information from their peer elements. Queries are always answered synchronously.


## References

1. https://gstreamer.freedesktop.org/documentation/application-development/introduction/gstreamer.html?gi-language=python
2. https://gstreamer.freedesktop.org/documentation/application-development/introduction/motivation.html?gi-language=python
3. https://gstreamer.freedesktop.org/documentation/application-development/introduction/basics.html?gi-language=python
