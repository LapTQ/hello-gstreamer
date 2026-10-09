#include "main/bootstrap.h"
#include "main/settings.h"

#include <gst/gst.h>

#include <iostream>
#include <optional>
#include <string>
#include <exception>

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    Settings settings { Settings::from_yaml("awl_tasks/task4.4/configs/settings.yaml") };

    std::optional<BootstrapRet> ret_opt { bootstrap(settings) };

    if (!ret_opt.has_value()) {
        g_printerr("Failed to initialize pipeline\n");
        return -1;
    }

    BootstrapRet ret_ = std::move(ret_opt.value());
    GstElement* pipeline { ret_.pipeline };
    GMainLoop* loop { ret_.loop };

    GstStateChangeReturn change_state { gst_element_set_state(pipeline, GST_STATE_PLAYING) };
    if (change_state == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Pipeline playing error\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_print("Pipeline running...\n");

    // Khóa thread ở đây
    g_main_loop_run(loop);

    // Dọn dẹp khi loop dừng
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}

