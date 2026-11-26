/*
 * status.h: keep track of VDR device status
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#pragma once

#include <atomic>
#include <vdr/status.h>
#include <vdr/epg.h>
#include "ait.h"

class cHbbtvDeviceStatus : public cStatus
{
private:
   class cSwitchWorker;
   cSwitchWorker *switchWorker;
   cDevice *device;
   cAitFilter *aitFilter;
   // also read by the regular worker (CheckProgramme)
   std::atomic<int> sid;
   int volume = 0;
   std::atomic<tEventID> lastPresentEventId{0};
   // runs in the switch worker, never in the callback of the channel switch
   void Switch(int channelNumber, const cDevice *vdrDevice);
protected:
   void ChannelSwitch(const cDevice *device, int channelNumber, bool LiveView) override;
   virtual void SetVolume(int Volume, bool Absolute);

public:
   cHbbtvDeviceStatus();
   ~cHbbtvDeviceStatus() override;
   int Sid() const { return sid; }

   // called regularly, sends the channel again if the present programme changes
   void CheckProgramme();
};
