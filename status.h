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
   int volume = 0;
protected:
   void ChannelSwitch(const cDevice *device, int channelNumber, bool LiveView) override;
   virtual void SetVolume(int Volume, bool Absolute);

public:
   cHbbtvDeviceStatus();
   ~cHbbtvDeviceStatus() override;
   int Sid() const { return sid; }
};
