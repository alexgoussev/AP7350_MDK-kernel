/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _COMMANDLISTENER_H__
#define _COMMANDLISTENER_H__

#include <sysutils/FrameworkListener.h>
#include "VoldCommand.h"

class CommandListener : public FrameworkListener {
public:
    CommandListener();
    virtual ~CommandListener() {}
    void sendBroadcast(int code, const char *msg, bool addErrno);

private:
    static void dumpArgs(int argc, char **argv, int argObscure);

    class DumpCmd : public VoldCommand {
    public:
        DumpCmd();
        virtual ~DumpCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

    class VolumeCmd : public VoldCommand {
    public:
        VolumeCmd();
        virtual ~VolumeCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

    //M{
    class USBCmd : public VoldCommand {
    public:
    	USBCmd();
    	virtual ~USBCmd() {}
    	int runCommand(SocketClient *c, int argc, char **argv);
    };
    
    class CDROMCmd : public VoldCommand {
    public:
    	CDROMCmd();
    	virtual ~CDROMCmd() {}
    	int runCommand(SocketClient *c, int argc, char **argv);
    };
    //}M

    class AsecCmd : public VoldCommand {
    public:
        AsecCmd();
        virtual ~AsecCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    private:
        void listAsecsInDirectory(SocketClient *c, const char *directory);
    };

    class ObbCmd : public VoldCommand {
    public:
        ObbCmd();
        virtual ~ObbCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

    class StorageCmd : public VoldCommand {
    public:
        StorageCmd();
        virtual ~StorageCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

    class CryptfsCmd : public VoldCommand {
    public:
        CryptfsCmd();
        virtual ~CryptfsCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

    class FstrimCmd : public VoldCommand {
    public:
        FstrimCmd();
        virtual ~FstrimCmd() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };

#if defined (ENG_BUILD_ENG)
    class SilkRoad : public VoldCommand {
    public:
        SilkRoad();
        virtual ~SilkRoad() {}
        int runCommand(SocketClient *c, int argc, char ** argv);
    };	
#endif
	
};

#endif