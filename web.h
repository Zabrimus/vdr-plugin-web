/*
 * web.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#pragma once

#include <string>
#include <shared_mutex>
#include <memory>
#include <vdr/plugin.h>
#include "service/web_service.h"

static const char *VERSION = "0.0.1";
static const char *DESCRIPTION = "Uses the cefbrowser to show HTTP Pages, HbbTV applications and videos";
static const char *MAINMENUENTRY = "Web";
static char* MAINMENUENTRYALT = nullptr;

extern std::shared_mutex videoPlayerLock;

class cRegularWorker: public cThread {
    private:
        cMutex m_mutex;
        cCondVar m_waitCondition;

    public:
        cRegularWorker();
        ~cRegularWorker();

        void Stop();
        void Action();
};

class cPluginWeb : public cPlugin {
private:
    std::unique_ptr<cRegularWorker> regularWorker;

public:
    cPluginWeb();
    ~cPluginWeb() override;

    const char *Version() override { return VERSION; }
    const char *Description() override { return DESCRIPTION; }
    const char *CommandLineHelp() override;
    bool ProcessArgs(int argc, char *argv[]) override;

    bool Initialize() override;
    bool Start() override;
    void Stop() override;

    void Housekeeping() override;
    void MainThreadHook() override;

    cString Active() override;
    time_t WakeupTime() override;

    const char *MainMenuEntry() override { return MAINMENUENTRYALT != nullptr ? MAINMENUENTRYALT : MAINMENUENTRY; }
    cOsdObject *MainMenuAction() override;
    cMenuSetupPage *SetupMenu() override;

    bool SetupParse(const char *Name, const char *Value) override;
    bool Service(const char *Id, void *Data) override;
    const char **SVDRPHelpPages() override;
    cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) override;

    bool readConfiguration(const char* configFile);
};
