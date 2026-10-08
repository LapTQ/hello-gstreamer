#ifndef VISUALIZE_UTILS_H
#define VISUALIZE_UTILS_H

#include "domain/entities/object.h"
#include "domain/entities/person_view_type.h"
#include "domain/entities/action_state_type.h"
#include "domain/ports/repo.h"

#include "gstnvdsmeta.h"

#include <string>
#include <unordered_map>

class Visualizer {
private:
    IObjectRepo& _repo;
    std::unordered_map<ViewType, std::string> _view_type_to_string {};
    std::unordered_map<ActionStateType, std::string> _action_state_to_string {};
    
public:
    Visualizer(
        IObjectRepo& repo, 
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
                unsigned int x1 { (unsigned int)(obj.x1n * (float)W) };
                unsigned int y2 { (unsigned int)(obj.y2n * (float)H) };
                
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

inline GstPadProbeReturn visualize(GstPad* pad, GstPadProbeInfo* info, gpointer visualizer_) {
    Visualizer* visualizer { (Visualizer*)visualizer_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    visualizer->visualize(buffer);
    return GST_PAD_PROBE_OK;
}

#endif