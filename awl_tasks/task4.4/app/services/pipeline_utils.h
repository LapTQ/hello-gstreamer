#ifndef PIPELINE_UTILS_H
#define PIPELINE_UTILS_H

#include "gst/gstelement.h"
#include <gst/gst.h>
#include <glib-unix.h>

#include <string>
#include <vector>

inline void link_source_to_nvstreammux(GstElement* source, GstPad* pad, GstPad* nvstreammux_sinkpad) {
    if (gst_pad_is_linked(nvstreammux_sinkpad)) {
        return;
    }

    if (!gst_pad_can_link(pad, nvstreammux_sinkpad)) {
        return;
    }

    GstPadLinkReturn _link_ret { gst_pad_link(pad, nvstreammux_sinkpad) };
    if (GST_PAD_LINK_FAILED(_link_ret)) {
        g_printerr("Link nvurisrcbin -> nvstreammux error");
    }
}


inline gboolean send_eos_to_nvstreammux(gpointer streammux){
    g_print("Sending EOS to streammux...\n");
    
    // Lặp qua tất cả các sink pad (sink_0, sink_1, ...) của streammux
    for (int i = 0; ; ++i) {
        std::string pad_name = "sink_" + std::to_string(i);
        GstPad* sinkpad { gst_element_get_static_pad((GstElement*)streammux, pad_name.c_str()) };
        if (!sinkpad) {
            break; // Đã duyệt hết các pad
        }
        gst_pad_send_event(sinkpad, gst_event_new_eos());
        gst_object_unref(sinkpad);
    }

    return G_SOURCE_REMOVE;
}


inline gboolean handle_bus_message(GstBus* bus, GstMessage* msg, gpointer loop) {
    GMainLoop* loop_ { (GMainLoop*)loop };

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError* err {};
        gchar* debug_info;
        gst_message_parse_error(msg, &err, &debug_info);

        g_printerr("Bus received error from element: %s\n", GST_OBJECT_NAME(msg->src));
        g_printerr("Error message: %s\n", err->message);
        g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");

        g_clear_error(&err);
        g_free(debug_info);

        g_main_loop_quit(loop_);
    }
    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        g_print("Bus received EOS.\n");

        g_main_loop_quit(loop_);
    }

    return G_SOURCE_CONTINUE;
}


inline GstElement* queued(GstElement* element) {
    GstElement* queue { gst_element_factory_make("queue", g_strdup_printf("queue_of_[%s]", gst_element_get_name(element))) };

    gchar* name { g_strdup_printf("queue->[%s]", gst_element_get_name(element)) };
    GstElement* bin { gst_bin_new(name) };

    gst_bin_add_many(GST_BIN(bin), queue, element, NULL);
    if (!gst_element_link_many(queue, element, NULL)) {
        g_printerr("Failed to link queue to element\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* queue_sink_pad { gst_element_get_static_pad(queue, "sink") };
    GstPad* ghost_sink_pad { gst_ghost_pad_new("sink", queue_sink_pad) };
    gst_element_add_pad(bin, ghost_sink_pad);
    gst_object_unref(queue_sink_pad);

    GstPad* element_src_pad { gst_element_get_static_pad(element, "src") };
    GstPad* ghost_src_pad { gst_ghost_pad_new("src", element_src_pad) };
    gst_element_add_pad(bin, ghost_src_pad);
    gst_object_unref(element_src_pad);

    return bin;
}


inline GstElement* create_multi_source_bin(const std::string& bin_name, const std::vector<std::string>& list_uris, int width, int height, bool live_source) {

    GstElement* bin { gst_pipeline_new(bin_name.c_str()) };

    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "live-source", live_source ? 1 : 0,
        "batch-size", list_uris.size(),
        "width", width,
        "height", height,
        "batched-push-timeout", 40000,
        "nvbuf-memory-type", 2, // 4: iGPU, 2; dGPU
        NULL
    );

    for (std::size_t i_u {0}; i_u < list_uris.size(); ++i_u) {
        GstElement* source { gst_element_factory_make("nvurisrcbin", ("source" + std::to_string(i_u)).c_str())};
        g_object_set(
            G_OBJECT(source), 
            "uri", list_uris[i_u].c_str(),
            "cudadec-memtype", 0,
            // reconnect
            "rtsp-reconnect-attempts", -1,
            "rtsp-reconnect-interval", 15,

            NULL
        );
        
        GstPad* sinkpad { gst_element_request_pad_simple(streammux, ("sink_" + std::to_string(i_u)).c_str())};

        g_signal_connect(source, "pad-added", G_CALLBACK(link_source_to_nvstreammux), sinkpad);

        gst_bin_add(GST_BIN(bin), source);

        gst_object_unref(sinkpad);
    }

    gst_bin_add(GST_BIN(bin), streammux);

    GstPad* streammux_src_pad { gst_element_get_static_pad(streammux, "src") };
    GstPad* ghost_pad { gst_ghost_pad_new("src", streammux_src_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(streammux_src_pad);

    // đăng ký bắt sự kiện interrupt
    g_unix_signal_add(SIGINT, send_eos_to_nvstreammux, streammux); // g_unix_signal_add hoạt động theo hàng đợi. Khi Ctrl-C, hàm send_eos_to_nvstreammux không chạy ngay mà chờ đến lượt được GMainLoop xử lý. 

    return bin;
}


inline GstElement* create_osd(int width, int height) {
    GstElement* tiler { gst_element_factory_make("nvmultistreamtiler", "tiler") };
    g_object_set(
        G_OBJECT(tiler),
        "width", width,
        "height", height,
        NULL
    );
    tiler = queued(tiler);

    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };
    converter = queued(converter);

    GstElement* nvosd { gst_element_factory_make("nvdsosd", "nvosd") };
    g_object_set(
        G_OBJECT(nvosd),
        "process-mode", 1,
        "display-text", 1,
        NULL
    );
    nvosd = queued(nvosd);

    GstElement* bin { gst_pipeline_new("osd_bin") };
    gst_bin_add_many(GST_BIN(bin), tiler, converter, nvosd, NULL);
    if (!gst_element_link_many(tiler, converter, nvosd, NULL)) {
        g_printerr("Failed to link elements in osd_bin\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* tiler_sink_pad { gst_element_get_static_pad(tiler, "sink") };
    GstPad* ghost_pad { gst_ghost_pad_new("sink", tiler_sink_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(tiler_sink_pad);

    GstPad* nvosd_src_pad { gst_element_get_static_pad(nvosd, "src") };
    GstPad* ghost_src_pad { gst_ghost_pad_new("src", nvosd_src_pad) };
    gst_element_add_pad(bin, ghost_src_pad);
    gst_object_unref(nvosd_src_pad);

    return bin;
}


inline GstElement* create_mp4_filesink(const std::string& bin_name, const std::string& output_file_path) {
    GstElement* queue { gst_element_factory_make("queue", "mp4_queue") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser { gst_element_factory_make("h264parse", "parser") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", output_file_path.c_str(), NULL);

    GstElement* bin { gst_bin_new(bin_name.c_str()) };
    gst_bin_add_many(GST_BIN(bin), queue, encoder, parser, mp4mux, sink, NULL);
    if (!gst_element_link_many(queue, encoder, parser, mp4mux, sink, NULL)) {
        g_printerr("Failed to link elements in mp4_filesink_bin\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* queue_sink_pad { gst_element_get_static_pad(queue, "sink") };
    GstPad* ghost_pad { gst_ghost_pad_new("sink", queue_sink_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(queue_sink_pad);

    return bin;
}


#endif