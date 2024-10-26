import sys
import gi
from pprint import pprint

gi.require_version('GLib', '2.0')
gi.require_version('GObject', '2.0')
gi.require_version('Gst', '1.0')

from gi.repository import Gst, GObject, GLib

pipeline = None
bus = None
message = None

# initialize GStreamer
pprint(sys.argv)
Gst.init(sys.argv[1:])
# if you pass your command-line parameters argc and argv to Gst.init(), 
# your application will automatically benefit from the GStreamer standard command-line options

# build the pipeline
pipeline = Gst.parse_launch(
    "playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm"
)
# In GStreamer you usually build the pipeline by manually assembling the individual elements, 
# but, when the pipeline is easy enough, and you do not need any advanced features, 
# you can take the shortcut: Gst.parse_launch(). This function takes a textual representation 
# of a pipeline and turns it into an actual pipeline, which is very handy.

# Here, we are building a pipeline composed of a single element called `playbin`. 
# playbin is a special element which acts as a source and as a sink, and is a whole pipeline. 
# Internally, it creates and connects all the necessary elements to play your media.
# In this example, we are only passing one parameter to playbin, which is the URI of the media.

# start playing
pipeline.set_state(Gst.State.PLAYING)
# Every GStreamer element has an associated state. For now, suffice to say that playback
# will not start unless you set the pipeline to the PLAYING state.

# the following sleeps and the video will start at the same time.
import time
for _ in range(5):
    time.sleep(1)
    print(_)

# wait until EOS or error
bus = pipeline.get_bus()
msg = bus.timed_pop_filtered(
    Gst.CLOCK_TIME_NONE,
    Gst.MessageType.ERROR | Gst.MessageType.EOS
)
# Gst.Element.get_bus() retrieves the pipeline's bus
# Gst.Bus.timed_pop_filtered() will block until you receive either an ERROR or an EOS.

# the following sleeps will not run until the video is finished.
import time
for _ in range(5):
    time.sleep(1)
    print(_)

# free resources
pipeline.set_state(Gst.State.NULL)