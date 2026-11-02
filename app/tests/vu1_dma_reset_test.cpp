// DBZIW-TEST: a hard VU1 reset cancels the microprogram in flight; a DMA pause does not (issue 018; DW5's
// vu1_dma_reset tests from app/tests/vu1_lift_test.cpp at 438a3de, guarding DW5 48b7400/414944c).
//
// A synthetic microprogram (no game data, no graphics backend) runs on the async device worker: XGKICK A, a NOP
// runway, SQ of a sentinel, XGKICK B, E bit. While A is being sent, the test pauses VIF1's DMA channel; for the
// hard reset case it also queues a VIF reset and a VU1 reset. Pause: both packets and the SQ land. Hard reset:
// only A lands, and the VU registers are kept. Run once per execution path, each in its own process (runLifted
// caches PS2X_VU1_LIFT): `interpreter` or `lift`.
#include "runtime/ps2_vu1.h"
#include "runtime/ps2_vu1_lift.h"
#include "ps2_runtime.h"
#include "runtime/ps2_device_queue.h"
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
static int setenv(const char *name, const char *value, int) { return _putenv_s(name, value); }
#endif

extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[] = {nullptr};

namespace {

void require(bool condition, const char *msg)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

// Synthetic SDK-independent microcode: XGKICK A, NOP runway, SQ sentinel,
// XGKICK B, E-bit + delay. No game data and no graphics backend is initialised.
class CancelGate {
public:
    void arrive() { std::unique_lock lock(mutex); entered=true; cv.notify_all(); cv.wait(lock,[&]{return released;}); }
    void wait() { std::unique_lock lock(mutex); cv.wait(lock,[&]{return entered;}); }
    void release() { std::lock_guard lock(mutex); released=true; cv.notify_all(); }
private:
    std::mutex mutex; std::condition_variable cv; bool entered=false,released=false;
};

std::vector<uint32_t> syntheticCancelCode()
{
    constexpr uint32_t lowerNop=0x8000033cu, upperNop=0x000002ffu;
    std::vector<uint32_t> code(4096);
    for(size_t i=0;i<code.size();i+=2) { code[i]=lowerNop; code[i+1]=upperNop; }
    code[0]=0x800006fcu | (1u<<11); // XGKICK vi1
    code[64]=(1u<<25)|(15u<<21)|(1u<<11)|64u; // SQ.xyzw vf1,64(vi0)
    code[66]=0x800006fcu | (2u<<11); // XGKICK vi2
    code[69]=upperNop|(1u<<30); // E bit at PC272
    return code;
}

unsigned syntheticLiftCalls=0; // worker-owned; observed only after explicit drain

void syntheticCancelLift(vl::Run &c)
{
    ++syntheticLiftCalls;
    vl::checkReset(c);
    // XGKICK vi1 at 0, SQ vf1 at 256, XGKICK vi2 at 264, E bit at 272 + delay slot.
    for (uint32_t pc = 0; pc <= 280; pc += 8)
    {
        vl::LiftAccess::begin(c, pc, c.st.vi);
        if (pc == 0)
            vl::LiftAccess::xgkick(c, static_cast<uint32_t>(c.st.vi[1]) & 0x3FFu);
        else if (pc == 256)
            vl::sq<15, false>(c, c.mem, 64, vl::load(c.st.vf[1]), 0u);
        else if (pc == 264)
            vl::LiftAccess::xgkick(c, static_cast<uint32_t>(c.st.vi[2]) & 0x3FFu);
        vl::LiftAccess::end(c, pc);
    }
    c.st.pc = 288;
}

void testRealVuDmaCancellation(bool lifted)
{
    // runLifted caches its enabled flag. These two modes must run in separate
    // executable invocations.
    setenv("PS2X_VU1_LIFT",lifted?"1":"0",1);
    const auto code=syntheticCancelCode();
    const vl::Registration registration{vl::hashCode(reinterpret_cast<const uint8_t *>(code.data()),16384),0,syntheticCancelLift};
    const vl::Registrar registrar(&registration,1);
    syntheticLiftCalls=0;
    for(bool hardReset : {false,true}) {
        CancelGate gate;
        PS2Runtime rt;
        require(rt.memory().initialize(),"real cancellation memory init");
        auto &mem=rt.memory(); auto &vu=rt.vu1();
        std::memcpy(mem.getVU1Code(),code.data(),16384);
        std::memset(mem.getVU1Data(),0,16384);
        auto packet=[&](unsigned qword,uint32_t marker) {
            uint64_t words[4]={1ull|(1ull<<15)|(1ull<<60),0xeull,marker,0x4full};
            std::memcpy(mem.getVU1Data()+qword*16,words,sizeof(words));
        };
        packet(0,0x1111); packet(16,0x2222);
        vu.state().vi[1]=0;vu.state().vi[2]=16;
        const float sentinel[4]={11.0f,22.0f,33.0f,44.0f};
        std::memcpy(vu.state().vf[1],sentinel,sizeof(sentinel));
        std::vector<uint32_t> packets;
        GifArbiter arbiter;
        arbiter.setProcessPacketFn([&](const uint8_t *bytes,uint32_t size) {
            require(size==32,"synthetic XGKICK packet length");
            uint32_t marker;std::memcpy(&marker,bytes+16,4);packets.push_back(marker);
            if(packets.size()==1) gate.arrive();
        });
        mem.setGifArbiter(&arbiter);
        mem.setVu1MscalCallback([&](uint32_t pc,uint32_t top,uint32_t itop) {
            vu.execute(mem.getVU1Code(),16384,mem.getVU1Data(),16384,rt.gs(),&mem,pc,top,itop,10000);
        });
        ps2x::device::Config config;config.async=true;
        config.controlWrite=[&](uint32_t address,uint32_t value){mem.writeIORegister(address,value);};
        ps2x::device::attach(mem,std::move(config));
        // Ensure every exception releases the worker before queue destruction.
        struct Cleanup { PS2Memory &mem;CancelGate &gate;~Cleanup(){gate.release();ps2x::device::detach(mem);} } cleanup{mem,gate};
        mem.m_ioRegisters[0x10009000]=0x101;
        ps2x::device::TransferSnapshot transfer;transfer.hadVif1=true;
        const uint32_t vif[4]={0x14000000u,0,0,0};
        transfer.vif1Chunks.emplace_back(reinterpret_cast<const uint8_t *>(vif),reinterpret_cast<const uint8_t *>(vif)+sizeof(vif));
        require(ps2x::device::submitTransfers(mem,std::move(transfer),false),"synthetic VIF transfer submitted");
        gate.wait();
        uint32_t saved=0;
        require(ps2x::device::pauseDmaChannel(mem,0x10009000,saved),"real VU Pause nonblocking");
        require(saved==0x101,"real VU Pause old CHCR");
        if(hardReset) {
            require(ps2x::device::submitControlWrite(mem,0x10003c10,1),"real VIF reset submitted");
            require(ps2x::device::submitVu1Reset(mem,[&]{vu.resetMicroExecution();}),"real hard VU reset submitted");
        }
        gate.release();
        ps2x::device::drainAll(mem,ps2x::device::DrainReason::Explicit);
        require(packets==(hardReset?std::vector<uint32_t>{0x1111}:std::vector<uint32_t>{0x1111,0x2222}),
                "real execution preserves emitted A; hard reset suppresses B; Pause accepts B");
        const uint8_t zeros[16]={};
        require(std::memcmp(mem.getVU1Data()+64*16,hardReset?static_cast<const void *>(zeros):static_cast<const void *>(sentinel),16)==0,
                "real hard reset suppresses later SQ; Pause preserves it");
        require(std::memcmp(vu.state().vf[1],sentinel,16)==0,"micro reset retains VU registers");
        require((syntheticLiftCalls!=0)==lifted,"requested real interpreter/lift execution path was used");
        const auto stats=ps2x::device::stats(mem);
        require(stats.submitted==stats.completed && stats.applied==stats.submitted,"real cancellation retires sequences");
    }
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc != 2 || (std::string(argv[1]) != "interpreter" && std::string(argv[1]) != "lift"))
        {
            std::cerr << "usage: " << argv[0] << " interpreter|lift\n";
            return 2;
        }
        testRealVuDmaCancellation(std::string(argv[1]) == "lift");
        std::cout << "Real VU DMA cancellation tests passed (" << argv[1] << ")\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
