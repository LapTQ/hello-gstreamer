#ifndef PERSON_VIEW_UTILS_H
#define PERSON_VIEW_UTILS_H

#include "person_view_type.h"
#include "ops.h"

#include "safe_gstnvdsinfer.h"
#include "gstnvdsmeta.h"
#include <gst/gst.h>

#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

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


#endif