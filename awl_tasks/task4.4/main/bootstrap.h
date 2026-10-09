#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "main/settings.h"
#include "domain/ports/repo.h"

#include <gst/gst.h>
#include <optional>
#include <memory>


struct BootstrapRet {
    GstElement* pipeline {};
    GMainLoop* loop {};
    std::unique_ptr<IObjectRepo> object_repo {};
};


std::optional<BootstrapRet> bootstrap(Settings settings);

#endif

