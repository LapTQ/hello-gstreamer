# https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html?gi-language=python

import sys
import gi
import logging

gi.require_version("GLib", "2.0")
gi.require_version("GObject", "2.0")
gi.require_version("Gst", "1.0")

from gi.repository import Gst, GLib, GObject


logging.basicConfig(level=logging.DEBUG, format="[%(name)s] [%(levelname)8s] - %(message)s")
logger = logging.getLogger(__name__)


# ======================================================================







# Initialize GStreamer
Gst.init(sys.argv[1:])


# Create the elements
source = Gst.ElementFactory.make("videotestsrc", "source")
sink = Gst.ElementFactory.make("autovideosink", "sink")
# videotestsrc is a source element (it produces data), which creates a test video pattern. 
# This element is useful for debugging purposes (and tutorials) and is not usually found 
# in real applications.
# autovideosink is a sink element (it consumes data), which displays on a window the images 
# it receives. There exist several video sinks, depending on the OS. autovideosink automatically 
# selects the best one, so your code is more platform-independent.

# Create the empty pipeline
pipeline = Gst.Pipeline.new("test-pipeline")
# All elements in GStreamer must typically be contained inside a pipeline before they can be used, 
# because it takes care of some clocking and messaging functions.
# A pipeline is a particular type of Gst.Bin, which is the element used to contain other elements. 
# Therefore all methods which apply to bins also apply to pipelines.


if not pipeline or not source or not sink:
    logger.error("Not all elements could be created.")
    sys.exit(1)



# Build the pipeline
pipeline.add(source)
pipeline.add(sink)
# we call Gst.Bin.add() to add elements to the pipeline.





if not source.link(sink):   # These elements, however, are not linked with each other yet. For this, we need to use Gst.Element.link().
    logger.error("Elements could not be linked.")
    sys.exit(1)




# Modify the source's properties
source.props.pattern = 0
# Can alternatively be done using `source.set_property("pattern",0)`
# or using `Gst.util_set_object_arg(source, "pattern", 0)`
# GStreamer elements are all a particular kind of GObject.Object, which is the entity 
# offering property facilities.
# The current state of a property can be fetched by either: `source.props.pattern` or `source.get_property("pattern")`
# Try changing the value of the pattern property!!!


# Start playing
ret = pipeline.set_state(Gst.State.PLAYING)
if ret == Gst.StateChangeReturn.FAILURE:
    logger.error("Unable to set the pipeline to the playing state.")
    sys.exit(1)




# Wait for EOS or error
bus = pipeline.get_bus()
msg = bus.timed_pop_filtered(
    Gst.CLOCK_TIME_NONE, 
    Gst.MessageType.ERROR | Gst.MessageType.EOS
)
# Gst.Bus.timed_pop_filtered() waits for execution to end and returns with a Gst.Message.


# Parse message
if msg:
    
    
    
    
    if msg.type == Gst.MessageType.ERROR:
        err, debug_info = msg.parse_error() # returns a GLib GLib.Error error structure and a string
        logger.error(f"Error received from element {msg.src.get_name()}: {err.message}")
        logger.error(f"Debugging information: {debug_info if debug_info else 'none'}")
    
    
    
    elif msg.type == Gst.MessageType.EOS:
        logger.info("End-Of-Stream reached.")
    
    else:
        # This should not happen as we only asked for ERRORs and EOS
        logger.error("Unexpected message received.")

# At this point it is worth introducing the GStreamer bus a bit more formally. 
# It is the object responsible for delivering to the application the Gst.Messages generated 
# by the elements, in order and to the application thread. This last point is important, 
# because the actual streaming of media is done in another thread than the application.



pipeline.set_state(Gst.State.NULL)