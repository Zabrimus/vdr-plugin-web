/*
 * streamevent.h: DSM-CC stream events (do-it-now) for HbbTV applications
 *
 * See the README file for copyright information and how to reach the author.
 */

#pragma once

#include <vdr/device.h>

// a new live channel, device is null before the switch
void StreamEventsChannelSwitch(cDevice *device);

// component tag and PID of an elementary stream of the current service (from the PMT)
void StreamEventsComponent(int componentTag, int pid);

// the browser wants (or doesn't want anymore) the stream events of a component
bool StreamEventsRequest(int componentTag, bool enable);

// stop the sender thread
void StreamEventsShutdown();
