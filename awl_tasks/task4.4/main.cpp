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
#include <cstring>
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
#include <gstnvdsinfer.h>
#include <cmath>
#include <algorithm>
#include <optional>

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


gboolean send_eos_to_nvstreammux(gpointer streammux){
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


GstElement* create_multi_source_bin(const std::string& bin_name, const std::vector<std::string>& list_uris, int width, int height, bool live_source) {

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

        g_signal_connect(source, "pad-added", G_CALLBACK(link_to_nvstreammux), sinkpad);

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


GstElement* create_osd(int width, int height) {
    GstElement* tiler { gst_element_factory_make("nvmultistreamtiler", "tiler") };
    g_object_set(
        G_OBJECT(tiler),
        "width", width,
        "height", height,
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


GstElement* create_mp4_filesink(const std::string& bin_name, const std::string& output_file_path) {
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser { gst_element_factory_make("h264parse", "parser") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", output_file_path.c_str(), NULL);


    GstElement* bin { gst_bin_new(bin_name.c_str()) };
    gst_bin_add_many(GST_BIN(bin), encoder, parser, mp4mux, sink, NULL);
    if (!gst_element_link_many(encoder, parser, mp4mux, sink, NULL)) {
        g_printerr("Failed to link elements in mp4_filesink_bin\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* encoder_sink_pad { gst_element_get_static_pad(encoder, "sink") };
    GstPad* ghost_pad { gst_ghost_pad_new("sink", encoder_sink_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(encoder_sink_pad);

    return bin;
}


class ActionStateProcessor {
private:
    float _width_ratio { 1.0 };
    float _height_ratio { 1.0 };
    std::optional<std::vector<gint>> _classes_to_rescale { std::nullopt };
    unsigned int _class_offset {};
public:
    ActionStateProcessor(
        float width_ratio = 1.0, 
        float height_ratio = 1.0, 
        std::optional<std::vector<gint>> class_to_rescale = std::nullopt,
        unsigned int class_offset = 1000
    )
        : 
        _width_ratio { width_ratio }, 
        _height_ratio { height_ratio },
        _classes_to_rescale { class_to_rescale },
        _class_offset { class_offset } 
    {}

    void preprocess(GstBuffer* buffer) {
        NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };
        NvDsFrameMeta* frame_meta {};

        NvDsObjectMeta* obj_meta {};
        NvDsObjectMeta* obj_meta_rescaled {};
        for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
            frame_meta = (NvDsFrameMeta*)i_frame->data;
            for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
                obj_meta = (NvDsObjectMeta*)i_obj->data;

                if (this->_classes_to_rescale.has_value()) {
                    if (std::find(this->_classes_to_rescale.value().begin(), this->_classes_to_rescale.value().end(), obj_meta->class_id) == this->_classes_to_rescale.value().end()) {
                        continue;
                    }
                }

                obj_meta_rescaled = nvds_acquire_obj_meta_from_pool(batch_meta);

                float x1 { obj_meta->rect_params.left };
                float y1 { obj_meta->rect_params.top };
                float w { obj_meta->rect_params.width };
                float h { obj_meta->rect_params.height };
                float xc { x1 + w / 2 };
                float yc { y1 + h / 2 };
                w = w * this->_width_ratio;
                h = h * this->_height_ratio;
                x1 = std::max(0.0f, xc - w / 2);
                y1 = std::max(0.0f, yc - h / 2);
                float x2 = std::min(xc + w / 2, frame_meta->pipeline_width * 1.0f);
                float y2 = std::min(yc + h / 2, frame_meta->pipeline_height * 1.0f);
                w = x2 - x1;
                h = y2 - y1;

                obj_meta_rescaled->unique_component_id = obj_meta->unique_component_id;

                obj_meta_rescaled->class_id = obj_meta->class_id + this->_class_offset;     // tạo class ảo
                obj_meta_rescaled->rect_params.left = x1;
                obj_meta_rescaled->rect_params.top = y1;
                obj_meta_rescaled->rect_params.width = w;
                obj_meta_rescaled->rect_params.height = h;

                obj_meta_rescaled->rect_params.border_width = 2;
                obj_meta_rescaled->rect_params.border_color = {1.0f, 1.0f, 0.0f, 1.0f};

                nvds_add_obj_meta_to_frame(frame_meta, obj_meta_rescaled, obj_meta); // thêm vào frame_meta
            }
        }
    }

    void postprocess(GstBuffer* buffer) {
        NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };
        NvDsFrameMeta* frame_meta {};

        NvDsObjectMeta* obj_meta {};
        NvDsUserMeta* user_meta {};
        
        for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
            frame_meta = (NvDsFrameMeta*)i_frame->data;

            std::vector<NvDsObjectMeta*> rescaled_objects {};
            for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
                obj_meta = (NvDsObjectMeta*)i_obj->data;

                if (this->_classes_to_rescale.has_value()) {
                    if (std::find(this->_classes_to_rescale.value().begin(), this->_classes_to_rescale.value().end(), obj_meta->class_id - this->_class_offset) == this->_classes_to_rescale.value().end()) {
                        continue;
                    }
                }

                rescaled_objects.push_back(obj_meta);
            }

            for (NvDsObjectMeta* obj_meta_rescaled: rescaled_objects) {
                NvDsObjectMeta* obj_meta = obj_meta_rescaled->parent;  // object gốc

                // lấy kết quả infer từ obj_user_meta_list (vì output-tensor-meta = 1) của object tạm và chuyển sang object gốc
                for (GList* i_usermeta { obj_meta_rescaled->obj_user_meta_list }; i_usermeta != NULL; i_usermeta = i_usermeta->next) {
                    user_meta = (NvDsUserMeta*)i_usermeta->data;
                    nvds_add_user_meta_to_obj(obj_meta, user_meta);
                }

                // QUAN TRỌNG: Gỡ danh sách ở child_obj để nvds_remove_obj_meta_from_frame không free user_meta này
                g_list_free(obj_meta_rescaled->obj_user_meta_list);
                obj_meta_rescaled->obj_user_meta_list = NULL;

                nvds_remove_obj_meta_from_frame(frame_meta, obj_meta_rescaled);     // xoá object tạm khỏi frame_meta
            }
        }

        
        
    }
};

GstPadProbeReturn preprocess_for_actionstate(GstPad* pad, GstPadProbeInfo* info, gpointer processor_) {
    ActionStateProcessor* processor { (ActionStateProcessor*)processor_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    processor->preprocess(buffer);
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn postprocess_for_actionstate(GstPad* pad, GstPadProbeInfo* info, gpointer processor_) {
    ActionStateProcessor* processor { (ActionStateProcessor*)processor_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    processor->postprocess(buffer);
    return GST_PAD_PROBE_OK;
}


enum class DetectionClassType {
    PERSON,
};

enum class ViewType {
    FRONT,
    SIDE,
    BACK,
};

struct View {
    ViewType type { ViewType::FRONT };
    float score { 0 };
};

enum class ActionStateType {
    HAND_INTO_PANT_POCKET,
    HAND_INTO_SHIRT_POCKET,
    HAND_INTO_BAG,
    HAND_INTO_BASKET,
    HAND_INTO_SHELF,
    HOLDING_PRODUCT,
    NOT_HOLDING_PRODUCT,
};

struct ActionState {
    ActionStateType type { ActionStateType::NOT_HOLDING_PRODUCT };
    float score { 0 };
};

struct Object {
    guint camera_id {};
    gint frame_num {};
    guint64 track_id {};
    float x1n {};
    float y1n {};
    float x2n {};
    float y2n {};
    DetectionClassType class_type { DetectionClassType::PERSON };
    std::optional<View> view { std::nullopt };
    std::optional<ActionState> action_state { std::nullopt };
};


class FakeObjectRepo {
private:
    std::unordered_map<guint, std::unordered_map<gint, std::vector<Object>>> _data {};
public:

    void add(Object obj) {
        this->_data[obj.camera_id][obj.frame_num].push_back(obj);
    }

    std::vector<Object> get_objects_by_frame(guint camera_id, gint frame_num, std::optional<std::vector<DetectionClassType>> class_filter = std::nullopt) {
        std::vector<Object> objects {};
        for (Object obj: this->_data[camera_id][frame_num]) {
            if (class_filter.has_value()) {
                if (std::find(class_filter.value().begin(), class_filter.value().end(), obj.class_type) == class_filter.value().end()) {
                    continue;
                }
            }
            objects.push_back(obj);
        }
        return objects;
    }
};


std::vector<float> softmax(const std::vector<float>& logits) {
    std::vector<float> exp_logits(logits.size());
    float sum_exp = 0.0f;

    for (size_t i = 0; i < logits.size(); ++i) {
        exp_logits[i] = std::exp(logits[i]);
        sum_exp += exp_logits[i];
    }

    for (size_t i = 0; i < exp_logits.size(); ++i) {
        exp_logits[i] /= sum_exp;
    }

    return exp_logits;
}

unsigned int argmax(const std::vector<float>& vec) {
    return std::distance(vec.begin(), std::max_element(vec.begin(), vec.end()));
}


class DetectionParser {
private:
    std::unordered_map<gint, DetectionClassType> _index_to_class_type {};

public:
    DetectionParser(std::unordered_map<gint, DetectionClassType> index_to_class_type)
        : _index_to_class_type { index_to_class_type }
    {}

    std::optional<Object> parse(NvDsFrameMeta* frame_meta, NvDsObjectMeta* obj_meta) {
        if (this->_index_to_class_type.find(obj_meta->class_id) == this->_index_to_class_type.end()) {
            return std::nullopt;
        }
        
        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };

        float x1n = (float)obj_meta->rect_params.left / W;
        float y1n = (float)obj_meta->rect_params.top / H;
        float x2n = (float)(obj_meta->rect_params.left + obj_meta->rect_params.width) / W;
        float y2n = (float)(obj_meta->rect_params.top + obj_meta->rect_params.height) / H;

        Object obj {
            frame_meta->source_id,
            frame_meta->frame_num,
            obj_meta->object_id,
            x1n,
            y1n,
            x2n,
            y2n,
            this->_index_to_class_type.at(obj_meta->class_id)
        };

        return obj;
    }
};


class PersonViewParser {
private:
    guint _gie_unique_id {};
    std::string _output_layer_name {};
    std::unordered_map<unsigned int, ViewType> _index_to_view_type {};

public:
    PersonViewParser(
        guint gie_unique_id, 
        const std::string& output_layer_name, 
        const std::unordered_map<unsigned int, ViewType>& index_to_view_type
    )
        : 
        _gie_unique_id { gie_unique_id }, 
        _output_layer_name { output_layer_name }, 
        _index_to_view_type { index_to_view_type } 
    {}

    std::optional<View> parse(const NvDsObjectMeta* obj_meta) {
        NvDsUserMeta* user_meta {};
        
        // duyệt qua từng user meta trong object
        for (GList* i_usermeta { obj_meta->obj_user_meta_list }; i_usermeta != NULL; i_usermeta = i_usermeta->next) {
            user_meta = (NvDsUserMeta*)i_usermeta->data;

            NvDsMetaType meta_type = user_meta->base_meta.meta_type;    
            NvDsInferTensorMeta* meta = ((NvDsInferTensorMeta*)user_meta->user_meta_data);
            
            if (!(meta_type == NVDSINFER_TENSOR_OUTPUT_META && meta->unique_id == this->_gie_unique_id)) {
                continue;
            }

            std::vector<float> logits {};
            for (guint i_l {0}; i_l < meta->num_output_layers; i_l++) {
                const char* layer_name = meta->output_layers_info[i_l].layerName;
                unsigned int size = meta->output_layers_info[i_l].inferDims.d[0];  // ma trận 1 chiều

                if (layer_name != this->_output_layer_name) {
                    continue;
                }

                for (unsigned int i {0}; i < size; ++i) {
                    float* tensor_data = (float*)meta->out_buf_ptrs_host[i_l];
                    logits.push_back(tensor_data[i]);
                }
            }

            std::vector<float> scores = softmax(logits);
            unsigned int amax = argmax(scores);
            float score = scores[amax];

            View view { 
                this->_index_to_view_type.at(amax), 
                score 
            };

            return view;
        }

        return std::nullopt;
    }
};

/*
- Dữ liệu tensor sau khi inference sẽ được tự động copy về CPU Host RAM và nằm trong out_buf_ptrs_host.
- out_buf_ptrs_host là một **mảng các con trỏ**, có kích thước bằng đúng tensor_meta->num_output_layers (số lượng Output Layers của model)
- Mỗi con trỏ trong out_buf_ptrs_host trỏ tới một vùng nhớ chứa tensor của một Output Layer.
- Nếu sau này bạn dùng model có 3 đầu ra (ví dụ YOLO có "boxes", "scores", "classes"):
    out_buf_ptrs_host[0] trỏ tới tensor "boxes".
    out_buf_ptrs_host[1] trỏ tới tensor "scores".
    out_buf_ptrs_host[2] trỏ tới tensor "classes".
- Mỗi tensor tương ứng với một dải ô nhớ tuyến tính 1 chiều. Dù tensor trong model có bao nhiêu chiều đi nữa, TensorRT luôn trải phẳng (flatten) tensor đó thành một mảng 1D liên tục.
*/

class ActionStateParser {
private:
    guint _gie_unique_id {};
    std::string _output_layer_name {};
    std::unordered_map<unsigned int, ActionStateType> _index_to_action_state {};

public:
    ActionStateParser(
        guint gie_unique_id, 
        const std::string& output_layer_name, 
        const std::unordered_map<unsigned int, ActionStateType>& index_to_action_state
    )
        : 
        _gie_unique_id { gie_unique_id }, 
        _output_layer_name { output_layer_name }, 
        _index_to_action_state { index_to_action_state } 
    {}

    std::optional<ActionState> parse(const NvDsObjectMeta* obj_meta) {
        NvDsUserMeta* user_meta {};
        
        // duyệt qua từng user meta trong object
        for (GList* i_usermeta { obj_meta->obj_user_meta_list }; i_usermeta != NULL; i_usermeta = i_usermeta->next) {
            user_meta = (NvDsUserMeta*)i_usermeta->data;

            NvDsMetaType meta_type = user_meta->base_meta.meta_type;    
            NvDsInferTensorMeta* meta = ((NvDsInferTensorMeta*)user_meta->user_meta_data);
            
            if (!(meta_type == NVDSINFER_TENSOR_OUTPUT_META && meta->unique_id == this->_gie_unique_id)) {
                continue;
            }

            std::vector<float> logits {};
            for (guint i_l {0}; i_l < meta->num_output_layers; i_l++) {
                const char* layer_name = meta->output_layers_info[i_l].layerName;
                unsigned int size = meta->output_layers_info[i_l].inferDims.d[0];  // ma trận 1 chiều

                if (layer_name != this->_output_layer_name) {
                    continue;
                }

                for (unsigned int i {0}; i < size; ++i) {
                    float* tensor_data = (float*)meta->out_buf_ptrs_host[i_l];
                    logits.push_back(tensor_data[i]);
                }
            }

            std::vector<float> scores = softmax(logits);
            unsigned int amax = argmax(scores);
            float score = scores[amax];

            ActionState action_state { 
                this->_index_to_action_state.at(amax), 
                score 
            };

            return action_state;
        }

        return std::nullopt;
    }
};


class OutputParser {
private:
    DetectionParser& _detection_parser;
    PersonViewParser& _person_view_parser;
    ActionStateParser& _action_state_parser;
    FakeObjectRepo& _repo;

public:
    OutputParser(
        DetectionParser& detection_parser,
        PersonViewParser& person_view_parser, 
        ActionStateParser& action_state_parser,
        FakeObjectRepo& repo
    ) 
        : 
        _detection_parser { detection_parser },
        _person_view_parser { person_view_parser },
        _action_state_parser { action_state_parser },
        _repo { repo }
    {}

    void parse(GstBuffer* buffer) {
        NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };

        NvDsFrameMeta* frame_meta {};
        NvDsObjectMeta* obj_meta {};

        // duyệt qua từng frame
        for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
            frame_meta = (NvDsFrameMeta*)i_frame->data;
            
            std::vector<NvDsObjectMeta*> to_remove {};
            // duyệt qua từng object trong frame
            for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
                obj_meta = (NvDsObjectMeta*)i_obj->data;

                std::optional<Object> obj_opt = this->_detection_parser.parse(frame_meta, obj_meta);
                if (!obj_opt.has_value()) {
                    to_remove.push_back(obj_meta);
                    continue;
                }
                
                Object obj = obj_opt.value();
                obj.view = this->_person_view_parser.parse(obj_meta);
                obj.action_state = this->_action_state_parser.parse(obj_meta);

                this->_repo.add(obj);
            }
            
            // xoá class không quan tâm (VD để khỏi phải vẽ lên OSD)
            for (NvDsObjectMeta* obj_meta : to_remove) {
                nvds_remove_obj_meta_from_frame(frame_meta, obj_meta);
            }
        }
    }
};


GstPadProbeReturn parse_pipeline_output(GstPad* pad, GstPadProbeInfo* info, gpointer parser_) {
    OutputParser* parser { (OutputParser*)parser_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    parser->parse(buffer);
    return GST_PAD_PROBE_OK;
}


class Visualizer {
private:
    FakeObjectRepo& _repo;
    std::unordered_map<ViewType, std::string> _view_type_to_string {};
    std::unordered_map<ActionStateType, std::string> _action_state_to_string {};
public:
    Visualizer(
        FakeObjectRepo& repo, 
        std::unordered_map<ViewType, std::string> view_type_to_string,
        std::unordered_map<ActionStateType, std::string> action_state_to_string
    )
        : 
        _repo { repo }, 
        _view_type_to_string { view_type_to_string },
        _action_state_to_string { action_state_to_string }
    {}

    void visualize(GstBuffer* buffer) {
        NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };
        NvDsFrameMeta* frame_meta {};
        for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
            frame_meta = (NvDsFrameMeta*)i_frame->data;
            unsigned int W { frame_meta->pipeline_width };
            unsigned int H { frame_meta->pipeline_height };
            
            std::vector<Object> objects { this->_repo.get_objects_by_frame(frame_meta->source_id, frame_meta->frame_num) };

            for (Object obj: objects) {
                unsigned int x1 { (unsigned int)(obj.x1n * W) };
                unsigned int y1 { (unsigned int)(obj.y1n * H) };
                unsigned int x2 { (unsigned int)(obj.x2n * W) };
                unsigned int y2 { (unsigned int)(obj.y2n * H) };
                
                // vẽ person-view
                if (obj.view.has_value()) {
                    std::string view_str { this->_view_type_to_string.at(obj.view.value().type) };
                    this->put_text(frame_meta, view_str, x1, y2 + 2);
                }

                // vẽ action-state
                if (obj.action_state.has_value()) {
                    std::string state_str { this->_action_state_to_string.at(obj.action_state.value().type) };
                    this->put_text(frame_meta, state_str, x1, y2 + 27);
                }
            }
        }
    }

    void put_text(
        NvDsFrameMeta* frame_meta, 
        const std::string& text, 
        unsigned int x, unsigned int y, 
        unsigned int font_scale = 15, 
        NvOSD_ColorParams color = {0.0, 1.0, 0.0, 1.0}, 
        NvOSD_ColorParams background_color = {0.0, 0.0, 0.0, 1.0}
    ) {
        NvDsDisplayMeta* display_meta { nvds_acquire_display_meta_from_pool(frame_meta->base_meta.batch_meta) };

        display_meta->num_labels = 1;
        NvOSD_TextParams* text_params = &display_meta->text_params[0];
        text_params->x_offset = x;
        text_params->y_offset = y;
        text_params->display_text = (char*)g_malloc0(64);
        snprintf(text_params->display_text, 64, "%s", text.c_str());
        
        text_params->font_params.font_name =  (char*)g_malloc0(64);
        snprintf(text_params->font_params.font_name, 64, "Serif Bold");
        text_params->font_params.font_size = font_scale;
        text_params->font_params.font_color = color;

        text_params->set_bg_clr = 1;
        text_params->text_bg_clr = background_color;

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }
};

GstPadProbeReturn visualize(GstPad* pad, GstPadProbeInfo* info, gpointer visualizer_) {
    Visualizer* visualizer { (Visualizer*)visualizer_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    visualizer->visualize(buffer);
    return GST_PAD_PROBE_OK;
}


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        // "file:///home/laptq/hello-gstreamer/assets/sample_720p.h264",
        "rtsp://admin:12345@192.168.3.26/live",
        "rtsp://admin:12345@192.168.3.21/live"
    };

    GstElement* source { 
        create_multi_source_bin(
            "multi_source_bin", 
            list_uris, 
            960, 
            640, 
            true
        ) 
    };

    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", "awl_tasks/task4.4/nvinfer_detector_config_file.yml",
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

    GstElement* person_view_classifier { gst_element_factory_make("nvinfer", "person_view_classifier") };
    g_object_set(
        G_OBJECT(person_view_classifier),
        "config-file-path", "awl_tasks/task4.4/nvinfer_person_view_config_file.yml",
        NULL
    );

    GstElement* action_state_classifier { gst_element_factory_make("nvinfer", "action_state_classifier") };
    g_object_set(
        G_OBJECT(action_state_classifier),
        "config-file-path", "awl_tasks/task4.4/nvinfer_action_state_config_file.yml",
        NULL
    );

    GstElement* analytics_tap { gst_element_factory_make("identity", "analytics_tap") };

    GstElement* nvdslogger { gst_element_factory_make ("nvdslogger", "nvdslogger") };
    g_object_set(G_OBJECT(nvdslogger), "fps-measurement-interval-sec", 1, NULL);

    
    GstElement* osd { create_osd(1920, 640) };
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* sink { create_mp4_filesink("mp4_filesink_bin", "outputs/output.mp4") };
    // GstElement* sink { gst_element_factory_make("fakesink", "sink") };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        detector,
        tracker,
        person_view_classifier,
        action_state_classifier,
        analytics_tap,
        nvdslogger,
        osd,
        sink,
        NULL
    );

    gboolean _link_success { false };
    _link_success = gst_element_link_many(
        source,
        detector,
        tracker,
        person_view_classifier,
        action_state_classifier,
        analytics_tap,
        nvdslogger,
        osd,
        sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }

    ActionStateProcessor actionstate_processor {
        1.3f, 1.1f,
        std::vector<gint> {0},
        1000
    };
    GstPad* actionstate_sinkpad { gst_element_get_static_pad(action_state_classifier, "sink") };
    gst_pad_add_probe(actionstate_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, preprocess_for_actionstate, &actionstate_processor, NULL);
    g_object_unref(actionstate_sinkpad);
    GstPad* actionstate_srcpad { gst_element_get_static_pad(action_state_classifier, "src") };
    gst_pad_add_probe(actionstate_srcpad, GST_PAD_PROBE_TYPE_BUFFER, postprocess_for_actionstate, &actionstate_processor, NULL);
    g_object_unref(actionstate_srcpad);

    DetectionParser detection_parser {
        std::unordered_map<gint, DetectionClassType> {
            {0, DetectionClassType::PERSON}, // class_index =
        } 
    }; // class_index = 0 (person)
    PersonViewParser person_view_parser {
        2, // gie_unique_id
        "output", // output_layer_name
        {
            {0, ViewType::FRONT},
            {1, ViewType::SIDE},
            {2, ViewType::BACK}
        }
    };
    ActionStateParser action_state_parser {
        3, // gie_unique_id
        "output", // output_layer_name
        {
            {0, ActionStateType::HAND_INTO_PANT_POCKET},
            {1, ActionStateType::HAND_INTO_SHIRT_POCKET},
            {2, ActionStateType::HAND_INTO_BAG},
            {3, ActionStateType::HAND_INTO_BASKET},
            {4, ActionStateType::HAND_INTO_SHELF},
            {5, ActionStateType::HOLDING_PRODUCT},
            {6, ActionStateType::NOT_HOLDING_PRODUCT},
        }
    };
    FakeObjectRepo fake_person_repo {};
    OutputParser parser { detection_parser, person_view_parser, action_state_parser, fake_person_repo };
    Visualizer visualizer { 
        fake_person_repo, 
        {
            {ViewType::FRONT, "Front"}, 
            {ViewType::SIDE, "Side"}, 
            {ViewType::BACK, "Back"}
        },
        {
            {ActionStateType::HAND_INTO_PANT_POCKET, "Hand->pant"},
            {ActionStateType::HAND_INTO_SHIRT_POCKET, "Hand->shirt"},
            {ActionStateType::HAND_INTO_BAG, "Hand->bag"},
            {ActionStateType::HAND_INTO_BASKET, "Hand->basket"},
            {ActionStateType::HAND_INTO_SHELF, "Hand->shelf"},
            {ActionStateType::HOLDING_PRODUCT, "Hold product"},
            {ActionStateType::NOT_HOLDING_PRODUCT, "Not hold product"},
        },
    };

    GstPad* analytics_tap_srcpad { gst_element_get_static_pad(analytics_tap, "src") };
    gst_pad_add_probe(analytics_tap_srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_pipeline_output, &parser, NULL);
    g_object_unref(analytics_tap_srcpad);

    GstPad* osd_sinkpad { gst_element_get_static_pad(osd, "sink") };
    gst_pad_add_probe(osd_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, visualize, &visualizer, NULL);
    g_object_unref(osd_sinkpad);

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