#ifndef ACTION_STATE_UTILS_H
#define ACTION_STATE_UTILS_H

#include "domain/entities/action_state_type.h"
#include "domain/ports/action_state_parser.h"
#include "app/services/ops.h"

#include "infra/common/safe_gstnvdsinfer.h"
#include "gstnvdsmeta.h"
#include "nvdsmeta.h"
#include <gst/gst.h>

#include <optional>
#include <vector>
#include <string>

/* Tiền xử lý trước và sau nvinfer để rescale box */
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

    /* Tạo object meta mới với box được rescale, kèm class ID ảo  */
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

                obj_meta_rescaled = nvds_acquire_obj_meta_from_pool(batch_meta);    // tạo object meta ảo

                // rescale box
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
                float x2 = std::min(xc + w / 2, (float)frame_meta->pipeline_width);
                float y2 = std::min(yc + h / 2, (float)frame_meta->pipeline_height);
                w = x2 - x1;
                h = y2 - y1;

                obj_meta_rescaled->unique_component_id = obj_meta->unique_component_id;

                obj_meta_rescaled->class_id = obj_meta->class_id + (gint)this->_class_offset;     // class ảo
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
                    if (std::find(this->_classes_to_rescale.value().begin(), this->_classes_to_rescale.value().end(), obj_meta->class_id - (gint)this->_class_offset) == this->_classes_to_rescale.value().end()) {
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

inline GstPadProbeReturn preprocess_for_actionstate(GstPad* pad, GstPadProbeInfo* info, gpointer processor_) {
    ActionStateProcessor* processor { (ActionStateProcessor*)processor_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    processor->preprocess(buffer);
    return GST_PAD_PROBE_OK;
}

inline GstPadProbeReturn postprocess_for_actionstate(GstPad* pad, GstPadProbeInfo* info, gpointer processor_) {
    ActionStateProcessor* processor { (ActionStateProcessor*)processor_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    processor->postprocess(buffer);
    return GST_PAD_PROBE_OK;
}


class ActionStateParser : public IActionStateParser {
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

    std::optional<ActionState> parse(const NvDsObjectMeta* obj_meta) override {
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

#endif