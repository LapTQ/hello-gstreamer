#ifndef LATENCY_MEASURER_H
#define LATENCY_MEASURER_H

#include <gst/gst.h>
#include <gstnvdsmeta.h>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>
#include <cstdio>

class LatencyMeasurer {
private:
    std::mutex _mutex;
    // Map: [source_id][frame_num] -> start_time
    std::unordered_map<guint, std::unordered_map<gint, std::chrono::steady_clock::time_point>> _start_times;

    // Tích lũy để tính trung bình mỗi chu kỳ
    std::unordered_map<guint, double> _accum_latency_ms;
    std::unordered_map<guint, int> _accum_frame_count;
    std::chrono::steady_clock::time_point _last_log_time;
    int _report_interval_sec;

public:
    explicit LatencyMeasurer(int report_interval_sec = 1)
        : _last_log_time(std::chrono::steady_clock::now()),
          _report_interval_sec(report_interval_sec) {}

    void on_frame_start(guint source_id, gint frame_num) {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_start_times[source_id][frame_num] = std::chrono::steady_clock::now();

        // Giới hạn bộ đệm tối đa 5000 frames cho mỗi stream để giải phóng bộ nhớ
        if (this->_start_times[source_id].size() > 5000) {
            this->_start_times[source_id].erase(this->_start_times[source_id].begin());
        }
    }

    void on_frame_end(guint source_id, gint frame_num) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(this->_mutex);

        auto s_it = this->_start_times.find(source_id);
        if (s_it != this->_start_times.end()) {
            auto f_it = s_it->second.find(frame_num);
            if (f_it != s_it->second.end()) {
                double lat_ms = std::chrono::duration<double, std::milli>(now - f_it->second).count();
                s_it->second.erase(f_it);

                this->_accum_latency_ms[source_id] += lat_ms;
                this->_accum_frame_count[source_id]++;
            }
        }

        // Báo cáo trung bình latency định kỳ (mặc định 1 giây)
        auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(now - this->_last_log_time).count();
        if (elapsed_sec >= this->_report_interval_sec && !this->_accum_frame_count.empty()) {
            std::string log_str = "**LATENCY : ";
            for (const auto& [sid, count] : this->_accum_frame_count) {
                if (count > 0) {
                    double avg_latency = this->_accum_latency_ms[sid] / count;
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "Stream_%u (%.2f ms)   ", sid, avg_latency);
                    log_str += buf;
                }
            }
            g_print("%s\n", log_str.c_str());

            this->_accum_latency_ms.clear();
            this->_accum_frame_count.clear();
            this->_last_log_time = now;
        }
    }
};

// Pad Probe callbacks
inline GstPadProbeReturn latency_start_probe(GstPad* pad, GstPadProbeInfo* info, gpointer measurer_) {
    auto* measurer = static_cast<LatencyMeasurer*>(measurer_);
    GstBuffer* buf = static_cast<GstBuffer*>(info->data);
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) return GST_PAD_PROBE_OK;

    for (GList* l_frame = batch_meta->frame_meta_list; l_frame != nullptr; l_frame = l_frame->next) {
        auto* frame_meta = static_cast<NvDsFrameMeta*>(l_frame->data);
        measurer->on_frame_start(frame_meta->source_id, frame_meta->frame_num);
    }
    return GST_PAD_PROBE_OK;
}

inline GstPadProbeReturn latency_end_probe(GstPad* pad, GstPadProbeInfo* info, gpointer measurer_) {
    auto* measurer = static_cast<LatencyMeasurer*>(measurer_);
    GstBuffer* buf = static_cast<GstBuffer*>(info->data);
    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) return GST_PAD_PROBE_OK;

    for (GList* l_frame = batch_meta->frame_meta_list; l_frame != nullptr; l_frame = l_frame->next) {
        auto* frame_meta = static_cast<NvDsFrameMeta*>(l_frame->data);
        measurer->on_frame_end(frame_meta->source_id, frame_meta->frame_num);
    }
    return GST_PAD_PROBE_OK;
}

#endif // LATENCY_MEASURER_H

