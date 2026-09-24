#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gstpipeline.h"
#include "gstnvdsmeta.h"
#include "nvds_roi_meta.h"
#include "nvdsmeta.h"
#include "nvll_osd_struct.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include<gst/gst.h>
#include<vector>
#include<string>
#include <glib-unix.h>
#include<unordered_map>
#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <thread>


void link_to_nvstreammux(GstElement* source, GstPad* pad, GstPad* nvstreammux_sinkpad) {
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


gboolean send_eos_to_pipeline(gpointer pipeline){
    g_print("Sending EOS to pipeline...\n");
    gst_element_send_event((GstElement*)pipeline, gst_event_new_eos());
    return G_SOURCE_REMOVE;
}


gboolean handle_bus_message(GstBus* bus, GstMessage* msg, gpointer loop) {
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


struct Object {
    guint64 track_id {};
    gint frame_num {};
    int x1 {};
    int y1 {};
    int x2 {};
    int y2 {};
};


template<typename T>
class TsQueue {
private:
    std::queue<T> _queue {};
    std::mutex _mutext {};

public:

    void push(T item) {
        std::lock_guard<std::mutex> lock{ this->_mutext };
        this->_queue.push(item);
        // g_print("Queue size: %lu\n", this->_queue.size());
    }   // Hết scope tự động mở khóa

    bool pop(T& item) {
        std::lock_guard<std::mutex> lock{ this->_mutext };

        if (this->_queue.empty()) {
            return false;
        }
        item = this->_queue.front();
        this->_queue.pop();

        return true;
    }

    bool is_empty() {
        std::lock_guard<std::mutex> lock{ this->_mutext };
        return this->_queue.empty();
    }
};


class FileRepo {
private:
    TsQueue<Object> _queue {};
    std::ofstream _outfile {};
    std::thread _thread {};
    bool _stop { false };
public:
    FileRepo(std::string file_path) {
        this->_outfile = std::ofstream{ file_path, std::ios::out };

        if(!this->_outfile.is_open()) {
            g_printerr("Cannot open output file!\n");
            std::abort();
        }

        this->_thread = std::thread { &FileRepo::_pop_and_write, this };    // khởi động thread pop từ queue và lưu vào file
    }

    void add(Object obj) {
        this->_queue.push(obj);
    }

    void close() {
        this->_stop = true;
        this->_thread.join();
        this->_outfile.close();
        g_print("Output file repo saved!\n");
    }

private:
    void _pop_and_write() {
        Object obj {};
        char line[128] {};
        while (!this->_stop or !this->_queue.is_empty()) {
            bool found = this->_queue.pop(obj);
            
            if (!found) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            std::snprintf(
                line, sizeof(line),
                "{\"track_id\": %lu, \"frame_num\": %d, \"x1\": %d, \"y1\": %d, \"x2\": %d, \"y2\": %d}\n",
                obj.track_id, obj.frame_num, obj.x1, obj.y1, obj.x2, obj.y2
            );
            this->_outfile << line;
        }
    }
};


GstPadProbeReturn save_to_repo(GstPad* pad, GstPadProbeInfo* info, gpointer repo_) {
    FileRepo* repo { (FileRepo*)repo_ };
    
    GstBuffer* buffer { (GstBuffer*)info->data };
    NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };

    NvDsFrameMeta* frame_meta {};
    NvDsObjectMeta* obj_meta {};
    for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
        frame_meta = (NvDsFrameMeta*)i_frame->data;

        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };
        
        for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
            obj_meta = (NvDsObjectMeta*)i_obj->data;

            int x1 = (int)obj_meta->rect_params.left;
            int y1 = (int)obj_meta->rect_params.top;
            int x2 = (int)obj_meta->rect_params.left + obj_meta->rect_params.width;
            int y2 = (int)obj_meta->rect_params.top + obj_meta->rect_params.height;

            Object obj { obj_meta->object_id, frame_meta->frame_num, x1, y1, x2, y2 };
            
            repo->add(obj);
        }
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    GstElement* source { gst_element_factory_make("nvurisrcbin", "source") };
    g_object_set(
        G_OBJECT(source), 
        "uri", "rtsp://admin:12345@192.168.3.26/live",
        "cudadec-memtype", 0,
        NULL
    );
    
    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "batch-size", 1,
        "width", 960,
        "height", 640,
        "batched-push-timeout", 40000,
        "nvbuf-memory-type", 2, // 4: iGPU, 2; dGPU
        NULL
    );

    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", "awl_tasks/task2.2/nvinfer_detector_config_file.yml",
        NULL
    );      // 1 vài thuộc tính trong file config có thể được ghi đè thông qua Gst Properties

    GstElement* tracker { gst_element_factory_make("nvtracker", "tracker") };
    g_object_set(
        G_OBJECT(tracker),
        "tracker-width", 640,
        "tracker-height", 640,
        "gpu-id", 0,
        "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file", "/opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
        NULL
    );

    GstElement* nvdslogger { gst_element_factory_make ("nvdslogger", "nvdslogger") };
    g_object_set(G_OBJECT(nvdslogger), "fps-measurement-interval-sec", 1, NULL);

    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };

    GstElement* nvosd { gst_element_factory_make("nvdsosd", "nvosd") };
    g_object_set(
        G_OBJECT(nvosd),
        "process-mode", 1,
        "display-text", 1,
        NULL
    );
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        streammux,
        detector,
        tracker,
        nvdslogger,
        converter,
        nvosd,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );

    // link source -> streammux
    GstPad* sinkpad { gst_element_request_pad_simple(streammux, "sink_0") };
    g_signal_connect(source, "pad-added", G_CALLBACK(link_to_nvstreammux), sinkpad);
    gst_object_unref(sinkpad);

     gboolean _link_success { false };
    _link_success = gst_element_link_many(
        streammux,
        detector,
        tracker,
        nvdslogger,
        converter,
        nvosd,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }

    // đăng ký bắt sự kiện interrupt
    g_unix_signal_add(SIGINT, send_eos_to_pipeline, pipeline); // g_unix_signal_add hoạt động theo hàng đợi. Khi Ctrl-C, hàm send_eos_to_pipeline không chạy ngay mà chờ đến lượt được GMainLoop xử lý. 

    FileRepo repo { "outputs/repo.jsonl" };
    GstPad* tracker_srcpad { gst_element_get_static_pad(tracker, "src") };
    gst_pad_add_probe(tracker_srcpad, GST_PAD_PROBE_TYPE_BUFFER, save_to_repo, &repo, NULL);
    g_object_unref(tracker_srcpad);

    GstStateChangeReturn _change_state { gst_element_set_state(pipeline, GST_STATE_PLAYING) };
    if (_change_state == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Pipeline playing error\n");
        g_object_unref(pipeline);
        return -1;
    }

    g_print("Pipeline running...\n");
    
    GstBus* bus { gst_element_get_bus(pipeline) };
    GMainLoop* loop { g_main_loop_new(NULL, FALSE) };
    gst_bus_add_watch(bus, handle_bus_message, loop);
    gst_object_unref(bus);
    
    // Khóa thread ở đây
    g_main_loop_run(loop);
    // Code chỉ chạy tiếp khi g_main_loop_quit() được gọi

    repo.close();

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
    g_main_loop_unref(loop);
}