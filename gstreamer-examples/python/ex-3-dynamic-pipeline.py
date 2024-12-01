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

def main():







    Gst.init(sys.argv[1:])

    # Create the elements
    source = Gst.ElementFactory.make("uridecodebin", "source")
    convert = Gst.ElementFactory.make("audioconvert", "convert")
    resample = Gst.ElementFactory.make("audioresample", "resample")
    sink = Gst.ElementFactory.make("autoaudiosink", "sink")




















    # Create the empty pipeline
    pipeline = Gst.Pipeline.new("test-pipeline")

    if not pipeline or not source or not convert or not resample or not sink:
        logger.error("Not all elements could be created.")
        sys.exit(1)


    # Build the pipeline. Note that we are NOT linking the source at this
    # point. We will do it later.
    pipeline.add(source)
    pipeline.add(convert)
    pipeline.add(resample)
    pipeline.add(sink)





    if not convert.link(resample) or not resample.link(sink):
        logger.error("Elements could not be linked.")
        sys.exit(1)



    # Set the URI to play
    source.set_property(
        "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
    )



    # Connect to the pad-added signal
    source.connect("pad-added", pad_added_handler, convert)















    # Start playing
    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        logger.error("Unable to set the pipeline to the playing state.")
        sys.exit(1)



    # Listen to the bus
    bus = pipeline.get_bus()
    terminate = False
    while True:
        msg = bus.timed_pop_filtered(
            Gst.CLOCK_TIME_NONE,
            Gst.MessageType.STATE_CHANGED | Gst.MessageType.ERROR | Gst.MessageType.EOS
        )


        # Parse message
        if msg:
            



            if msg.type == Gst.MessageType.ERROR:
                err, debug_info = msg.parse_error()
                logger.error("Error received from element {}: {}".format(msg.src.get_name(), err.message))
                logger.error("Debugging information: {}".format(debug_info if debug_info else "none"))
                terminate = True
            


            elif msg.type == Gst.MessageType.ERROR:
                logger.info("End-Of-Stream reached.")
                terminate = True
            
            elif msg.type == Gst.MessageType.STATE_CHANGED:

                if msg.src is pipeline:
                    old_state, new_state, pending_state = msg.parse_state_changed()
                    logger.info("Pipeline state changed from {} to {}:".format(
                        Gst.Element.state_get_name(old_state),Gst.Element.state_get_name(new_state) 
                    ))
                    

            else:
                logger.error("Unexpected message received.")
                terminate = True
        
        if terminate:
            break






    pipeline.set_state(Gst.State.NULL)





def pad_added_handler(src, pad, convert):












    sink_pad = convert.get_static_pad("sink")


    


    logger.info("Received new pad '{}' from {}".format(pad.get_name(), src.get_name()))
    
    # If our converter is already linked, we have nothing to do here
    if sink_pad.is_linked():
        logger.info("We are already linked. Ignoring.")
        return
    

    # Check the new pad's type
    new_pad_caps = pad.get_current_caps()




    new_pad_struct = new_pad_caps.get_structure(0)





    new_pad_type = new_pad_struct.get_name()
    if not new_pad_type.startswith("audio/x-raw"):
        logger.info("It has type '{}', which is not raw audio. Ignoring.".format(new_pad_type))
        return
    


    # Attempt the link
    ret = pad.link(sink_pad)
    if ret != Gst.PadLinkReturn.OK:
        logger.error("Type is '{}' but link failed.".format(new_pad_type))
    else:
        logger.info("Link succeeded (type '{}')".format(new_pad_type))


if __name__ == '__main__':
    main()