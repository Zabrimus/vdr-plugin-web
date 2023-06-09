/*
 * ait.h: AIT section filter
 *
 * See the main source file  for copyright information and
 * how to reach the author.
 *
 */

#pragma once

#include <vdr/filter.h>
#include <vdr/thread.h>


#define MAXPMTENTRIES 64



/* ETSI TS 102 809
 * Possible values of the stream content descriptor
 */
enum application_control_codes
{
   acc_reserved           = 0x00,
   acc_Autostart          = 0x01,
   acc_present            = 0x02,
   acc_destroy            = 0x03,
   acc_kill               = 0x04,
   acc_prefetch           = 0x05,
   acc_remote             = 0x06,
   acc_disabled           = 0x07,
   acc_playback_autostart = 0x08,
};

#define MAXPMTENTRIES 64
        
class cAitFilter : public cFilter
{
   private:
      cMutex mutex;
      cTimeMs timer;
      int patVersion;
      int pmtVersion;
      int sid;
      int pmtPid;
      int pmtNextCheck;
   protected:
      virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
   public:
      cAitFilter(int Sid);
      virtual void SetStatus(bool On);
      void Trigger(int Sid = -1);
};
