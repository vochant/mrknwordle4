#pragma once

#include "widgets/widget.hpp"
#include "paintbrush/paintbrush.hpp"

#include <mutex>

extern float fps;

extern std::mutex renderer_lock;

extern Widget* root_widget;

void renderer(PaintBrush* pb);