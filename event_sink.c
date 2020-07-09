/*
 * Copyright (C) 2009-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Keegan S. <keegan@sdf.org>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "event_sink.h"

static void gum_fake_event_sink_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_fake_event_sink_finalize (GObject * obj);
static GumEventType gum_fake_event_sink_query_mask (GumEventSink * sink);
static void gum_fake_event_sink_process (GumEventSink * sink,
    const GumEvent * ev);

G_DEFINE_TYPE_EXTENDED (GumFakeEventSink,
                        gum_fake_event_sink,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_EVENT_SINK,
                                               gum_fake_event_sink_iface_init))

static void gum_fake_event_sink_class_init(GumFakeEventSinkClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = gum_fake_event_sink_finalize;
}

static void gum_fake_event_sink_iface_init(gpointer g_iface, gpointer iface_data) {
  GumEventSinkInterface* iface = g_iface;
  iface->query_mask = gum_fake_event_sink_query_mask;
  iface->process = gum_fake_event_sink_process;
}

static void gum_fake_event_sink_init (GumFakeEventSink* self) {
}

static void gum_fake_event_sink_finalize (GObject* obj) {
  G_OBJECT_CLASS (gum_fake_event_sink_parent_class)->finalize (obj);
}

GumEventSink* gum_fake_event_sink_new (void) {
  GumFakeEventSink* sink;
  sink = g_object_new(GUM_TYPE_FAKE_EVENT_SINK, NULL);
  return GUM_EVENT_SINK(sink);
}

void gum_fake_event_sink_reset(GumFakeEventSink * self) {
}

static GumEventType gum_fake_event_sink_query_mask(GumEventSink* sink) {
  return 0;
}

static void gum_fake_event_sink_process (GumEventSink* sink, const GumEvent* ev) {
}