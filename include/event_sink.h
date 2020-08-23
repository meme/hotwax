#pragma once

#include "frida-gum.h"

G_BEGIN_DECLS

#define GUM_TYPE_FAKE_EVENT_SINK (gum_fake_event_sink_get_type ())
G_DECLARE_FINAL_TYPE (GumFakeEventSink, gum_fake_event_sink, GUM,
    FAKE_EVENT_SINK, GObject)

struct _GumFakeEventSink {
  GObject parent;
  GumEventType mask;
};

GumEventSink* gum_fake_event_sink_new(void);
void gum_fake_event_sink_reset(GumFakeEventSink* self);

G_END_DECLS