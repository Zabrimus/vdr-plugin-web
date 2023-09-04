/*
 * web.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#pragma once

#include <string>
#include <vdr/plugin.h>

static const char *VERSION = "0.0.1";
static const char *DESCRIPTION = "Enter description for 'web' plugin";
static const char *MAINMENUENTRY = "Web";

class cPluginWeb : public cPlugin {
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

    const char *MainMenuEntry() override { return MAINMENUENTRY; }
    cOsdObject *MainMenuAction() override;
    cMenuSetupPage *SetupMenu() override;

    bool SetupParse(const char *Name, const char *Value) override;
    bool Service(const char *Id, void *Data = nullptr) override;
    const char **SVDRPHelpPages() override;
    cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) override;

    bool readConfiguration(const char* configFile);

private:
    std::string videoInfo;
};
