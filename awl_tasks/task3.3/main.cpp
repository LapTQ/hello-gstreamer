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
#include<gst/gst.h>
#include<vector>
#include<string>
#include <glib-unix.h>
#include<unordered_map>
#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <unordered_set>


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
    float x1n {};
    float y1n {};
    float x2n {};
    float y2n {};
};

struct Line {
    float x1n {};
    float y1n {};
    float x2n {};
    float y2n {};
};


// Hàm phụ: Kiểm tra điểm (x, y) có nằm trên đoạn giới hạn bởi (px, py) và (qx, qy) không
// Lưu ý: Chỉ gọi hàm này khi 3 điểm đã được xác định là thẳng hàng.
inline bool onSegment(float px, float py, float qx, float qy, float x, float y) {
    return x <= std::max(px, qx) && x >= std::min(px, qx) &&
           y <= std::max(py, qy) && y >= std::min(py, qy);
}

// Hàm phụ: Xác định hướng của 3 điểm p, q, r
inline int orientation(float px, float py, float qx, float qy, float rx, float ry) {
    float val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    
    // Sử dụng Epsilon để xử lý sai số của kiểu float trong C++
    constexpr float EPSILON = 1e-6f; 
    
    if (val > EPSILON) return 1;       // Cùng chiều kim đồng hồ
    if (val < -EPSILON) return 2;      // Ngược chiều kim đồng hồ
    return 0;                          // Thẳng hàng
}

// Hàm chính: Trả về true nếu l1 và l2 cắt nhau
bool doIntersect(const Line& l1, const Line& l2) {
    int o1 = orientation(l1.x1n, l1.y1n, l1.x2n, l1.y2n, l2.x1n, l2.y1n);
    int o2 = orientation(l1.x1n, l1.y1n, l1.x2n, l1.y2n, l2.x2n, l2.y2n);
    int o3 = orientation(l2.x1n, l2.y1n, l2.x2n, l2.y2n, l1.x1n, l1.y1n);
    int o4 = orientation(l2.x1n, l2.y1n, l2.x2n, l2.y2n, l1.x2n, l1.y2n);

    // Trường hợp 1: Cắt nhau tổng quát
    if (o1 != o2 && o3 != o4) {
        return true;
    }

    // Trường hợp 2: Các điểm thẳng hàng và đè lên nhau (overlap)
    if (o1 == 0 && onSegment(l1.x1n, l1.y1n, l1.x2n, l1.y2n, l2.x1n, l2.y1n)) return true;
    if (o2 == 0 && onSegment(l1.x1n, l1.y1n, l1.x2n, l1.y2n, l2.x2n, l2.y2n)) return true;
    if (o3 == 0 && onSegment(l2.x1n, l2.y1n, l2.x2n, l2.y2n, l1.x1n, l1.y1n)) return true;
    if (o4 == 0 && onSegment(l2.x1n, l2.y1n, l2.x2n, l2.y2n, l1.x2n, l1.y2n)) return true;

    return false; // Không cắt nhau
}


class LineCrossingDetector {
private:
    Line _line;
    std::unordered_map<guint64, std::vector<Object>> _tracks {};
    std::unordered_set<guint64> _crossed {};
public:
    LineCrossingDetector(Line line)
        : _line { line }
    {}

    void on_new_frame(const std::vector<Object> objects, NvDsBatchMeta* batch_meta,NvDsFrameMeta* frame_meta) {

        for (Object obj: objects) {
            this->_tracks[obj.track_id].push_back(obj);

            size_t size { this->_tracks[obj.track_id].size() };
            if (size < 2) {
                continue;
            }

            if (this->_crossed.find(obj.track_id) != this->_crossed.end()) {
                continue;
            }

            Line cur_move {
                (this->_tracks[obj.track_id][size - 2].x1n + this->_tracks[obj.track_id][size - 2].x2n) / 2,
                (this->_tracks[obj.track_id][size - 2].y1n + this->_tracks[obj.track_id][size - 2].y2n) / 2,
                (this->_tracks[obj.track_id][size - 1].x1n + this->_tracks[obj.track_id][size - 1].x2n) / 2,
                (this->_tracks[obj.track_id][size - 1].y1n + this->_tracks[obj.track_id][size - 1].y2n) / 2,
            };
            bool do_cross { doIntersect(this->_line, cur_move )};

            if (do_cross) {
                this->_crossed.insert(obj.track_id);

                g_print("[LINE CROSS] Frame ID: %d | Track ID: %lu\n", obj.frame_num, obj.track_id);
            }

        }
        
        this->_draw_osd(batch_meta, frame_meta);
    }

private:
    void _draw_osd(NvDsBatchMeta* batch_meta, NvDsFrameMeta* frame_meta) {
        // throw std::logic_error("chưa implement");
        NvDsDisplayMeta* display_meta { nvds_acquire_display_meta_from_pool(batch_meta) };

        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };

        // vẽ đường
        unsigned int lx1 = (unsigned int)(this->_line.x1n * W);
        unsigned int ly1 = (unsigned int)(this->_line.y1n * H);
        unsigned int lx2 = (unsigned int)(this->_line.x2n * W);
        unsigned int ly2 = (unsigned int)(this->_line.y2n * H);

        display_meta->num_lines = 1;
        NvOSD_LineParams* line_params = &display_meta->line_params[0];
        line_params->x1 = lx1;
        line_params->y1 = ly1;
        line_params->x2 = lx2;
        line_params->y2 = ly2;
        line_params->line_width = 4;
        line_params->line_color = {0.0, 1.0, 0.0, 1.0};

        // vẽ chữ
        display_meta->num_labels = 1;
        NvOSD_TextParams* text_params = &display_meta->text_params[0];
        text_params->x_offset = lx1 - 10;
        text_params->y_offset = ly1 + 5;
        text_params->display_text = (char*)g_malloc0(64);
        snprintf(text_params->display_text, 64, "%lu", this->_crossed.size());
        
        text_params->font_params.font_name =  (char*)g_malloc0(64);
        snprintf(text_params->font_params.font_name, 64, "Serif");
        text_params->font_params.font_size = 30;
        text_params->font_params.font_color = {0.0, 1.0, 0.0, 1.0};

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }
};


GstPadProbeReturn detect_line_crossing(GstPad* pad, GstPadProbeInfo* info, gpointer crossing_detector_) {
    LineCrossingDetector* crossing_detector { (LineCrossingDetector*)crossing_detector_ };
    
    GstBuffer* buffer { (GstBuffer*)info->data };
    NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };

    NvDsFrameMeta* frame_meta {};
    NvDsObjectMeta* obj_meta {};
    std::vector<Object> objects {};
    for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
        frame_meta = (NvDsFrameMeta*)i_frame->data;

        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };
        
        objects.clear();
        for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
            obj_meta = (NvDsObjectMeta*)i_obj->data;

            float x1n = (float)obj_meta->rect_params.left / W;
            float y1n = (float)obj_meta->rect_params.top / H;
            float x2n = (float)(obj_meta->rect_params.left + obj_meta->rect_params.width) / W;
            float y2n = (float)(obj_meta->rect_params.top + obj_meta->rect_params.height) / H;

            Object obj { obj_meta->object_id, frame_meta->frame_num, x1n, y1n, x2n, y2n };
            
            objects.push_back(obj);
        }
        crossing_detector->on_new_frame(objects, batch_meta, frame_meta);
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    GstElement* source { gst_element_factory_make("nvurisrcbin", "source") };
    g_object_set(
        G_OBJECT(source), 
        "uri", "file:///home/laptq/hello-gstreamer/assets/sample_720p.h264",
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

    LineCrossingDetector crossing_detector { Line {0.35f, 0.75f, 0.9f, 0.5f } };
    GstPad* tracker_srcpad { gst_element_get_static_pad(tracker, "src") };
    gst_pad_add_probe(tracker_srcpad, GST_PAD_PROBE_TYPE_BUFFER, detect_line_crossing, &crossing_detector, NULL);
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

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
    g_main_loop_unref(loop);
}