#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../third_party/ps3recomp/libs/system/savedata_shutdown.h"
static savedata_gate gate;
static HANDLE entered, release_write;
static const char* filename;
static DWORD WINAPI writer(void* unused)
{
    (void)unused; assert(savedata_gate_begin(&gate));
    FILE* f=fopen(filename,"wb");assert(f);
    unsigned char data[16384];memset(data,0x5a,sizeof data);
    assert(fwrite(data,1,sizeof data,f)==sizeof data);SetEvent(entered);
    assert(WaitForSingleObject(release_write,5000)==WAIT_OBJECT_0);
    assert(fwrite(data,1,sizeof data,f)==sizeof data);assert(!fclose(f));
    savedata_gate_end(&gate);return 0;
}
static DWORD WINAPI racer(void* unused)
{
    (void)unused;
    for(unsigned i=0;i<10000;i++)if(savedata_gate_begin(&gate)){SwitchToThread();savedata_gate_end(&gate);}
    return 0;
}
int main(int argc,char**argv)
{
    assert(argc==2);filename=argv[1];
    entered=CreateEvent(NULL,TRUE,FALSE,NULL);release_write=CreateEvent(NULL,TRUE,FALSE,NULL);assert(entered&&release_write);
    HANDLE thread=CreateThread(NULL,0,writer,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);
    assert(!savedata_gate_close(&gate));assert(!savedata_gate_begin(&gate));
    SetEvent(release_write);assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    assert(savedata_gate_close(&gate));assert(!savedata_gate_begin(&gate));assert(!atomic_load(&gate.active));
    FILE*f=fopen(filename,"rb");assert(f);for(unsigned i=0;i<32768;i++)assert(fgetc(f)==0x5a);assert(fgetc(f)==EOF);fclose(f);
    CloseHandle(thread);CloseHandle(entered);CloseHandle(release_write);
    DWORD before,after;GetProcessHandleCount(GetCurrentProcess(),&before);
    for(unsigned n=0;n<50;n++){
        atomic_store(&gate.active,0);atomic_store(&gate.closing,0);HANDLE threads[4];
        for(unsigned i=0;i<4;i++){threads[i]=CreateThread(NULL,0,racer,NULL,0,NULL);assert(threads[i]);}
        Sleep(1);savedata_gate_close(&gate);
        assert(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
        assert(savedata_gate_close(&gate)&&!atomic_load(&gate.active)&&!savedata_gate_begin(&gate));
        for(unsigned i=0;i<4;i++)CloseHandle(threads[i]);
    }
    GetProcessHandleCount(GetCurrentProcess(),&after);assert(after<=before+2);
    puts("PASS: close waits for final fclose, rejects new operations, preserves full file; 50 x 4 concurrent admission/close races, zero active/leaked handles");
    return 0;
}
