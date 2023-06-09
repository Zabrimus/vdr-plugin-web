/*
 * status.h: keep track of VDR device status
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#pragma once

#include <vdr/status.h>
#include "ait.h"

class cHbbtvDeviceStatus : public cStatus
{
private:
   cDevice *device;
   cAitFilter *aitFilter;
   int sid;
protected:
   void ChannelSwitch(const cDevice *device, int channelNumber, bool LiveView) override;

public:
   cHbbtvDeviceStatus();
   ~cHbbtvDeviceStatus() override;
   int Sid() const { return sid; }
};

extern cHbbtvDeviceStatus *HbbtvDeviceStatus;
