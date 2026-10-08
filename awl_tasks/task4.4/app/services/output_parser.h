#ifndef OUTPUT_PARSER_H
#define OUTPUT_PARSER_H

#include "domain/ports/action_state_parser.h"
#include "domain/ports/person_view_parser.h"
#include "domain/ports/detection_parser.h"
#include "domain/ports/repo.h"
#include "domain/entities/object.h"

#include "gstnvdsmeta.h"

class OutputParser {
private:
    IDetectionParser& _detection_parser;
    IPersonViewParser& _person_view_parser;
    IActionStateParser& _action_state_parser;
    IObjectRepo& _repo;

public:
    OutputParser(
        IDetectionParser& detection_parser,
        IPersonViewParser& person_view_parser, 
        IActionStateParser& action_state_parser,
        IObjectRepo& repo
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


inline GstPadProbeReturn parse_pipeline_output(GstPad* pad, GstPadProbeInfo* info, gpointer parser_) {
    OutputParser* parser { (OutputParser*)parser_ };
    GstBuffer* buffer { (GstBuffer*)info->data };
    parser->parse(buffer);
    return GST_PAD_PROBE_OK;
}

#endif